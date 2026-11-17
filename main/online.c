#include "online.h"
#include "unipus.h"
#include <cJSON.h>
#include <errno.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_tls_errors.h>
#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef fcntl
#undef fcntl
#endif

#ifdef DICT_UART_TRACE
#define UART_TRACE(...) printf(__VA_ARGS__)
#else
#define UART_TRACE(...) ((void)0)
#endif

/* Raw TCP probe uses only exported socket APIs and always closes its socket. */
static int tcp_probe(const unsigned char address[4], uint16_t port,
                     int timeout_ms) {
  int fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0)
    return errno ? errno : EIO;
  int result = 0;
  if (fd >= FD_SETSIZE) {
    result = EINVAL;
    goto done;
  }
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    result = errno ? errno : EIO;
    goto done;
  }
  struct sockaddr_in peer = {.sin_family = AF_INET};
  unsigned char *port_bytes = (unsigned char *)&peer.sin_port;
  port_bytes[0] = (unsigned char)(port >> 8);
  port_bytes[1] = (unsigned char)port;
  memcpy(&peer.sin_addr.s_addr, address, 4);
  if (lwip_connect(fd, (struct sockaddr *)&peer, sizeof(peer)) == 0)
    goto done;
  result = errno ? errno : EIO;
  if (result != EINPROGRESS && result != EWOULDBLOCK && result != EALREADY)
    goto done;
  fd_set writable;
  FD_ZERO(&writable);
  FD_SET(fd, &writable);
  struct timeval timeout = {.tv_sec = timeout_ms / 1000,
                            .tv_usec = (timeout_ms % 1000) * 1000};
  int ready = lwip_select(fd + 1, NULL, &writable, NULL, &timeout);
  if (ready == 0)
    result = ETIMEDOUT;
  else if (ready < 0)
    result = errno ? errno : EIO;
  else {
    socklen_t size = sizeof(peer);
    result = lwip_getpeername(fd, (struct sockaddr *)&peer, &size) == 0
                 ? 0
                 : (errno ? errno : ENOTCONN);
  }
done:
  lwip_close(fd);
  return result;
}
/* End raw TCP probe. */
typedef struct {
  char *data;
  size_t used;
  bool overflow;
  int64_t deadline;
  bool header_seen;
  void (*trace)(const char *, void *);
  bool (*cancelled)(void *);
  void *context;
} Response;
static esp_err_t receive(esp_http_client_event_t *e) {
  Response *r = e->user_data;
  if ((r->cancelled && r->cancelled(r->context)) ||
      esp_timer_get_time() >= r->deadline)
    return ESP_FAIL;
  if (e->event_id == HTTP_EVENT_ON_CONNECTED) {
    int64_t left_ms = (r->deadline - esp_timer_get_time()) / 1000;
    if (left_ms < 1)
      return ESP_FAIL;
    esp_http_client_set_timeout_ms(e->client,
                                   left_ms < 10000 ? (int)left_ms : 10000);
  }
  if (r->trace) {
    if (e->event_id == HTTP_EVENT_ON_CONNECTED)
      r->trace("net: connected (TCP/TLS complete)", r->context);
    if (e->event_id == HTTP_EVENT_ON_HEADER && !r->header_seen) {
      r->header_seen = true;
      r->trace("net: response headers", r->context);
    }
    if (e->event_id == HTTP_EVENT_ERROR)
      r->trace("net: transport error event", r->context);
    if (e->event_id == HTTP_EVENT_ON_DATA && r->used == 0)
      r->trace("net: first response data", r->context);
  }
  if (e->event_id == HTTP_EVENT_ON_DATA) {
    if (e->data_len < 0 || r->used + (size_t)e->data_len > 32768) {
      r->overflow = true;
      return ESP_FAIL;
    }
    memcpy(r->data + r->used, e->data, e->data_len);
    r->used += e->data_len;
    r->data[r->used] = 0;
  }
  return ESP_OK;
}
static cJSON *parse_complete_json(const char *data, size_t used,
                                  int64_t expected, bool chunked,
                                  bool examples) {
  (void)examples;
  if (!used || (expected >= 0 && !(chunked && expected == 0) &&
                (uint64_t)expected != used))
    return NULL;
  const char *end = NULL;
  cJSON *json = cJSON_ParseWithLengthOpts(data, used + 1, &end, true);
  if (!json)
    return NULL;
  cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
  bool valid =
      end == data + used && cJSON_IsObject(json) && cJSON_IsNumber(code);
  if (valid && code->valueint == 0) {
    cJSON *value = cJSON_GetObjectItemCaseSensitive(json, "value");
    cJSON *list = cJSON_GetObjectItemCaseSensitive(value, "list");
    valid = cJSON_IsObject(value) && (!list || cJSON_IsArray(list));
    if (valid && list) {
      cJSON *entry = NULL;
      cJSON_ArrayForEach(entry, list) {
        cJSON *word = cJSON_GetObjectItemCaseSensitive(entry, "wordValue");
        cJSON *head = cJSON_GetObjectItemCaseSensitive(word, "word");
        if (!cJSON_IsObject(word) || !cJSON_IsString(head) ||
            !*head->valuestring ||
            !cJSON_IsArray(
                cJSON_GetObjectItemCaseSensitive(word, "category"))) {
          valid = false;
          break;
        }
      }
    }
  }
  if (!valid) {
    cJSON_Delete(json);
    return NULL;
  }
  return json;
}
bool online_query(const char *word, bool examples, const char *pem, char *out,
                  size_t cap, void (*trace)(const char *, void *),
                  bool (*cancelled)(void *), void *context) {
  out[0] = 0;
  if (time(NULL) < 1704067200) {
    snprintf(out, cap, "设备时间错误，请先联网校时。");
    return false;
  }
  size_t n = strlen(word);
  if (!n || n > 96) {
    snprintf(out, cap, "查询词为空或过长。");
    return false;
  }
  for (size_t i = 0; i < n; i++)
    if ((unsigned char)word[i] >= 128) {
      snprintf(out, cap,
               "当前外研试用接口使用英汉词典，请输入英文词头再查询。");
      return false;
    }
  char token[64], body[768];
  if (!unipus_token((int64_t)time(NULL), token)) {
    snprintf(out, cap, "外研试用鉴权生成失败（AES-CBC）。");
    return false;
  }
  cJSON *request = cJSON_CreateObject();
  bool encoded = request && cJSON_AddStringToObject(request, "name", word) &&
                 cJSON_PrintPreallocated(request, body, sizeof(body), false);
  cJSON_Delete(request);
  if (!encoded) {
    snprintf(out, cap, "无法编码查询词。");
    return false;
  }
  const char *url = "https://open.unipus.cn/openapi/dict/v1/keyword/1/1";
  Response r = {
      .data = heap_caps_malloc(32769, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
      .deadline = esp_timer_get_time() + 20000000,
      .trace = trace,
      .cancelled = cancelled,
      .context = context};
  if (!r.data) {
    snprintf(out, cap, "内存不足，请稍后重试。");
    return false;
  }
  r.data[0] = 0;
  int tcp_result = -1;
  if (trace) {
    char info[192];
    esp_netif_ip_info_t ip;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
      snprintf(info, sizeof(info), "net: station ip=" IPSTR " gateway=" IPSTR,
               IP2STR(&ip.ip), IP2STR(&ip.gw));
      trace(info, context);
    }
    const char *host = UNIPUS_HOST;
    struct hostent *resolved = lwip_gethostbyname(host);
    if (resolved && resolved->h_addrtype == AF_INET &&
        resolved->h_length == 4 && resolved->h_addr_list &&
        resolved->h_addr_list[0]) {
      const unsigned char *address =
          (const unsigned char *)resolved->h_addr_list[0];
      snprintf(info, sizeof(info), "net: DNS %s -> %u.%u.%u.%u", host,
               address[0], address[1], address[2], address[3]);
      trace(info, context);
      int64_t started = esp_timer_get_time();
      tcp_result = tcp_probe(address, 443, 1500);
      snprintf(info, sizeof(info), "net: TCP443 %s errno=%d elapsed_ms=%lld",
               tcp_result ? "failed" : "connected", tcp_result,
               (long long)((esp_timer_get_time() - started) / 1000));
    } else
      snprintf(info, sizeof(info), "net: DNS failed for %s", host);
    trace(info, context);
  }
  if (trace) {
    char memory[160];
    snprintf(memory, sizeof(memory),
             "net: heap internal_free=%u largest=%u psram_free=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                               MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL |
                                                        MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    trace(memory, context);
  }
  esp_http_client_config_t config = {.url = url,
                                     .cert_pem = pem,
                                     .timeout_ms = 10000,
                                     .is_async = false,
                                     .transport_type = HTTP_TRANSPORT_OVER_SSL,
                                     .event_handler = receive,
                                     .user_data = &r,
                                     .disable_auto_redirect = true,
                                     .buffer_size = 1024,
                                     .addr_type = HTTP_ADDR_TYPE_INET,
                                     .max_authorization_retries = -1};
  cJSON *json = NULL;
  int64_t expected = -1;
  bool chunked = false;
  /* Request attempts: read-only lookup POST, at most one fresh-connection
   * retry. */
  esp_err_t error = ESP_ERR_TIMEOUT, tls_status = ESP_OK;
  int saved_errno = 0, socket_error = 0, status = 0, tls_error = 0,
      tls_flags = 0;
  for (unsigned attempt = 0; attempt < 2; attempt++) {
    if (cancelled && cancelled(context)) {
      error = ESP_ERR_INVALID_STATE;
      break;
    }
    int64_t remaining = r.deadline - esp_timer_get_time();
    if (remaining < 1000000) {
      error = ESP_ERR_TIMEOUT;
      break;
    }
    /* TCP setup and TLS may each consume timeout_ms. Keep both in budget. */
    config.timeout_ms =
        remaining / 2000 < 5000 ? (int)(remaining / 2000) : 5000;
    int64_t attempt_started = esp_timer_get_time();
    if (attempt)
      config.tls_version = ESP_HTTP_CLIENT_TLS_VER_TLS_1_2;
    r.used = 0;
    r.overflow = r.header_seen = false;
    r.data[0] = 0;
    esp_http_client_handle_t http = esp_http_client_init(&config);
    if (!http) {
      free(r.data);
      snprintf(out, cap, "无法创建 HTTPS 请求。");
      return false;
    }
    esp_http_client_set_header(http, "User-Agent",
                               "VibeDictionary/0.1 (ESP32)");
    esp_http_client_set_method(http, HTTP_METHOD_POST);
    esp_http_client_set_header(http, "Content-Type", "application/json");
    esp_http_client_set_header(http, "p-app-id", UNIPUS_TRIAL_APP_ID);
    esp_http_client_set_header(http, "p-cip-txt", token);
    esp_http_client_set_post_field(http, body, (int)strlen(body));
    esp_http_client_set_timeout_ms(http, config.timeout_ms);
    char diagnostic[256];
    snprintf(diagnostic, sizeof(diagnostic),
             "net: attempt=%u timeout_ms=%d tls=%s host=%s", attempt + 1,
             config.timeout_ms, attempt ? "TLS1.2" : "default", UNIPUS_HOST);
    if (trace)
      trace(diagnostic, context);
    error = ESP_ERR_HTTP_EAGAIN;
    while (error == ESP_ERR_HTTP_EAGAIN) {
      if (cancelled && cancelled(context)) {
        error = ESP_ERR_INVALID_STATE;
        break;
      }
      if (esp_timer_get_time() >= r.deadline) {
        error = ESP_ERR_TIMEOUT;
        break;
      }
      error = esp_http_client_perform(http);
      if (error == ESP_ERR_HTTP_EAGAIN)
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    saved_errno = errno;
    socket_error = esp_http_client_get_errno(http);
    status = esp_http_client_get_status_code(http);
    expected = esp_http_client_get_content_length(http);
    chunked = esp_http_client_is_chunked_response(http);
    bool complete = esp_http_client_is_complete_data_received(http);
    tls_error = tls_flags = 0;
    tls_status = esp_http_client_get_and_clear_last_tls_error(http, &tls_error,
                                                              &tls_flags);
    snprintf(diagnostic, sizeof(diagnostic),
             "net: attempt=%u done err=%s status=%d bytes=%u tls=%d flags=0x%x "
             "errno=%d socket=%d transport=%s(0x%x) elapsed_ms=%lld "
             "expected=%lld chunked=%d "
             "complete=%d",
             attempt + 1, esp_err_to_name(error), status, (unsigned)r.used,
             tls_error, tls_flags, saved_errno, socket_error,
             esp_err_to_name(tls_status), tls_status,
             (long long)((esp_timer_get_time() - attempt_started) / 1000),
             (long long)expected, chunked, complete);
    if (trace)
      trace(diagnostic, context);
    esp_http_client_cleanup(http);
    bool transient = tls_status == ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT ||
                     tls_status == ESP_ERR_ESP_TLS_SERVER_HANDSHAKE_TIMEOUT ||
                     tls_status == ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST ||
                     tls_status == ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME;
    bool response_interrupted = error == ESP_ERR_HTTP_INCOMPLETE_DATA ||
                                error == ESP_ERR_HTTP_READ_TIMEOUT ||
                                error == ESP_ERR_HTTP_CONNECTION_CLOSED;
    if (response_interrupted && status == 200 && !tls_error && !tls_flags &&
        !r.overflow && !(cancelled && cancelled(context))) {
      json = parse_complete_json(r.data, r.used, expected, chunked, examples);
      if (json) {
        if (trace)
          trace("net: recovered complete JSON document after framing "
                "interruption",
                context);
        error = ESP_OK;
        break;
      }
    }
    bool retryable = (error == ESP_ERR_HTTP_CONNECT && transient && !r.used) ||
                     (response_interrupted && (status == 0 || status == 200));
    if (attempt != 0 || !retryable || tls_error || tls_flags || r.overflow ||
        (cancelled && cancelled(context)) ||
        r.deadline - esp_timer_get_time() < 1500000)
      break;
    if (trace)
      trace("net: transient request failure; discarding response and retrying "
            "with a fresh TLS client",
            context);
    vTaskDelay(pdMS_TO_TICKS(400));
  }
  /* End request attempts. */
  if (trace) {
    char memory[128];
    snprintf(memory, sizeof(memory),
             "net: finished internal_free=%u largest=%u stack_free=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                               MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL |
                                                        MALLOC_CAP_8BIT),
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
    trace(memory, context);
  }
  bool ok = false;
  if (error != ESP_OK) {
    if (r.overflow)
      snprintf(out, cap, "在线结果过大，未显示。离线词典仍可使用。");
    else
      snprintf(out, cap,
               "%s。\n错误：%s\nTCP 探测：%s (%d)\n已收：%u 字节\n连接码：%d / "
               "%d\n传输：%s "
               "(0x%x)\nTLS：%d / "
               "0x%x\n返回可继续离线查词。",
               status ? "在线响应没有接收完整" : "连接在线服务失败",
               esp_err_to_name(error),
               tcp_result < 0 ? "未完成"
               : tcp_result   ? "失败"
                              : "成功",
               tcp_result, (unsigned)r.used, saved_errno, socket_error,
               esp_err_to_name(tls_status), tls_status, tls_error, tls_flags);
    goto done;
  }
  if (status != 200) {
    snprintf(out, cap, "在线服务返回 HTTP %d%s", status,
             status == 404   ? "：未收录此词。"
             : status == 429 ? "：请求过多，请稍后重试。"
                             : "，请稍后重试。");
    goto done;
  }
  if (!json)
    json = parse_complete_json(r.data, r.used, expected, chunked, examples);
  if (!json) {
    snprintf(out, cap, "在线服务返回了无效数据。");
    goto done;
  }
  ok = unipus_format(json, examples, out, cap);
done:
  cJSON_Delete(json);
  free(r.data);
  return ok;
}
