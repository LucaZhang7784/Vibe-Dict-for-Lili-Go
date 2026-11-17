#include "dict_core.h"
#include "online.h"
#include "sd_font.h"
#include "shift_paging.h"
#include <errno.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <pthread.h>
#include <src/core/lv_obj_class_private.h>
#include <src/core/lv_obj_private.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tactility/concurrent/thread.h>
#include <tactility/device.h>
#include <tactility/drivers/i2c_controller.h>
#include <tactility/drivers/i2s_controller.h>
#include <time.h>
#include <tt_app.h>
#include <tt_lock.h>
#include <tt_wifi.h>

#ifdef DICT_UART_TRACE
#define UART_TRACE(...) printf(__VA_ARGS__)
#else
#define UART_TRACE(...) ((void)0)
#endif

enum {
  LOOKUP = 1,
  CANDIDATES,
  AUDIO,
  ONLINE,
  EXAMPLES,
  SAVE,
  HISTORY,
  FAVORITES,
  TOUCH_CHECK
};
typedef struct {
  int kind;
  unsigned id, page;
  uint32_t cursor;
  bool zh;
  char text[192];
} Job;
typedef struct {
  Job job;
  int status, count;
  bool more;
  uint32_t cursor;
  unsigned total;
  DictEntry entry;
  DictCandidate candidates[5];
  char book[20][192];
  char message[24576];
} Result;
typedef struct {
  AppHandle app;
  char base[256], assets[256], user[256], trace_path[256], query[2][192],
      py[68], lookup[192];
  bool zh, result, online, more, busy, started, net_started, command,
      network_waiting, old_editing, book_visible, candidates_pending;
  unsigned id, page, total, cpage;
  int offline_scroll;
  uint32_t cursor, cursors[128], edit_tick, network_tick;
  unsigned network_id;
  int count, selected, pending_selected;
  DictCandidate candidates[5];
  DictEntry entry;
  char book[20][192];
  unsigned book_count;
  lv_obj_t *book_panel, *candidate_nav, *candidate_page_label, *candidate_prev,
      *candidate_next;
  lv_obj_t *speak_label, *save_label;
  lv_obj_t *touch_panel, *touch_info;
  lv_indev_t *touch_pointer;
  unsigned touch_pointer_count, touch_presses;
  bool touch_was_pressed;
  char touch_details[96];
  unsigned touch_hits[9];
  uint32_t touch_tick;
  lv_obj_t *root, *input, *input_label, *mode_label, *bar, *choices[5],
      *choice_labels[5], *body, *content, *status;
  lv_timer_t *timer;
  QueueHandle_t jobs, net_jobs, results;
  Thread *local_thread;
  pthread_t network_thread;
  struct Device *keyboard_i2c;
  uint8_t shift_held;
  bool shift_unavailable, keyboard_restore_pending;
  atomic_bool stop, cancel_network;
} State;
typedef struct {
  State *state;
  LockHandle lock;
} TraceContext;
static State *state;
static void submit(int kind);
static void input_refresh(void);
static void search_page(void);
static void choose(unsigned index);
static void perform(intptr_t action);
static void candidate_selection(void);
static void say(const char *text);
static void key(lv_event_t *e);
static lv_obj_t *button(lv_obj_t *, const char *, int, int, lv_obj_t **);
static void touch_sample(lv_event_t *e) {
  State *s = state;
  lv_indev_t *indev = lv_event_get_indev(e);
  if (!indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER)
    return;
  unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(e);
  lv_point_t p;
  lv_indev_get_point(indev, &p);
  if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
    s->touch_tick = lv_tick_get();
    s->touch_hits[index]++;
    lv_obj_t *target = lv_event_get_target_obj(e);
    lv_obj_t *label = lv_obj_get_child(target, 0);
    lv_label_set_text_fmt(label, "%u : %u", index + 1, s->touch_hits[index]);
    lv_label_set_text_fmt(s->touch_info,
                          "触点 %ld,%ld · 按下\n每格轻点两次，观察漏点",
                          (long)p.x, (long)p.y);
  } else {
    lv_label_set_text_fmt(
        s->touch_info, "触点 %ld,%ld · %lu ms\n每格轻点两次，观察漏点",
        (long)p.x, (long)p.y, (unsigned long)lv_tick_elaps(s->touch_tick));
  }
}
static void touch_test(void) {
  State *s = state;
  if (s->touch_panel)
    return;
  memset(s->touch_hits, 0, sizeof(s->touch_hits));
  s->touch_pointer = NULL;
  s->touch_pointer_count = s->touch_presses = 0;
  s->touch_was_pressed = false;
  strcpy(s->touch_details, "正在识别触屏控制器");
  for (lv_indev_t *indev = lv_indev_get_next(NULL); indev;
       indev = lv_indev_get_next(indev))
    if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
      s->touch_pointer = indev;
      s->touch_pointer_count++;
    }
  s->touch_panel = lv_obj_create(lv_obj_get_parent(s->root));
  lv_obj_add_flag(s->touch_panel, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(s->touch_panel, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(s->touch_panel, 0, 0);
  lv_obj_set_style_pad_all(s->touch_panel, 0, 0);
  lv_obj_set_style_bg_color(s->touch_panel, lv_color_hex(0xF5F3EF), 0);
  lv_obj_set_style_text_color(s->touch_panel, lv_color_hex(0x302E2B), 0);
  lv_obj_set_style_text_font(s->touch_panel, sd_font_get(), 0);
  lv_obj_remove_flag(s->touch_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_update_layout(s->touch_panel);
  int width = lv_obj_get_content_width(s->touch_panel);
  int height = lv_obj_get_content_height(s->touch_panel);
  lv_obj_t *title = lv_label_create(s->touch_panel);
  lv_label_set_text(title, "触屏测试");
  lv_obj_set_pos(title, 8, 8);
  lv_obj_t *close = button(s->touch_panel, "关闭T", 20, 80, NULL);
  lv_obj_set_pos(close, width - 82, 0);
  s->touch_info = lv_label_create(s->touch_panel);
  lv_obj_set_width(s->touch_info, width - 16);
  lv_obj_set_pos(s->touch_info, 8, 42);
  lv_label_set_text(s->touch_info,
                    "每格轻点两次，观察漏点\nT 关闭；不修改触屏配置");
  for (unsigned i = 0; i < 9; i++) {
    lv_obj_t *target = lv_button_create(s->touch_panel);
    lv_obj_set_size(target, (width - 16) / 3 - 4, (height - 90) / 3 - 4);
    lv_obj_set_pos(target, 8 + (i % 3) * ((width - 16) / 3),
                   88 + (i / 3) * ((height - 90) / 3));
    lv_obj_set_style_bg_color(target, lv_color_hex(0xE56A3B), 0);
    lv_obj_set_style_shadow_width(target, 0, 0);
    lv_obj_t *label = lv_label_create(target);
    lv_label_set_text_fmt(label, "%u : 0", i + 1);
    lv_obj_center(label);
    lv_obj_add_event_cb(target, touch_sample, LV_EVENT_PRESSED,
                        (void *)(uintptr_t)i);
    lv_obj_add_event_cb(target, touch_sample, LV_EVENT_RELEASED,
                        (void *)(uintptr_t)i);
    lv_obj_add_event_cb(target, key, LV_EVENT_KEY, NULL);
  }
  lv_group_focus_obj(close);
  lv_timer_set_period(s->timer, 20);
  submit(TOUCH_CHECK);
}
static bool restore_keyboard(State *s) {
  const uint8_t command = 0x04;
  bool ok = i2c_controller_write(s->keyboard_i2c, 0x55, &command, 1,
                                 pdMS_TO_TICKS(20)) == ERROR_NONE;
  s->keyboard_restore_pending = !ok;
  return ok;
}
static void poll_shift_page(State *s) {
  if (s->touch_panel)
    return;
  if (!s->keyboard_i2c)
    s->keyboard_i2c = device_find_by_name("i2c0");
  if (!s->keyboard_i2c)
    return;
  if (s->keyboard_restore_pending) {
    restore_keyboard(s);
    return;
  }
  if (s->shift_unavailable || s->result || !s->zh || !*s->py) {
    s->shift_held = 0;
    return;
  }
  /* This callback and the native keypad reader run on the LVGL thread.
   * Temporarily read raw state, then restore character mode before yielding.
   * C3 retains its pending character in raw mode, so native typing is retained.
   * No keyboard firmware or persistent setting is changed. */
  const uint8_t command = 0x03;
  uint8_t matrix[5] = {255, 255, 255, 255, 255};
  bool ok = i2c_controller_write(s->keyboard_i2c, 0x55, &command, 1,
                                 pdMS_TO_TICKS(20)) == ERROR_NONE;
  if (ok)
    ok = i2c_controller_read(s->keyboard_i2c, 0x55, matrix, sizeof(matrix),
                             pdMS_TO_TICKS(20)) == ERROR_NONE;
  if (!restore_keyboard(s))
    return;
  if (!ok)
    return;
  for (unsigned i = 0; i < 5; i++)
    if (matrix[i] & 0x80) {
      s->shift_unavailable = true;
      say("键盘未提供 Shift 状态，请点击左右翻组按钮。");
      return;
    }
  int direction = shift_page_edge(matrix, &s->shift_held);
  if (direction)
    perform(direction < 0 ? 3 : 4);
}
static lv_obj_class_t reader_class;
static void scroll_text(int direction, bool page) {
  State *s = state;
  int current = lv_obj_get_scroll_y(s->body);
  int maximum = current + lv_obj_get_scroll_bottom(s->body);
  int target = dict_scroll_target(
      current, maximum, lv_obj_get_content_height(s->body), direction, page);
  lv_obj_scroll_to_y(s->body, target, LV_ANIM_OFF);
}
static void reader_event(const lv_obj_class_t *cls, lv_event_t *e) {
  (void)cls;
  if (lv_event_get_code(e) == LV_EVENT_KEY)
    return;
  lv_obj_event_base(&reader_class, e);
}
static lv_obj_t *reader_create(lv_obj_t *parent) {
  /* Custom LVGL 9.3 widget: inherit the runtime base class using the pinned SDK
   * layout. */
  lv_obj_t *probe = lv_obj_create(parent);
  if (probe->class_p->instance_size != sizeof(lv_obj_t)) {
    UART_TRACE(
        "[dict.ui] LVGL widget ABI mismatch; retaining standard reader\n");
    return probe;
  }
  reader_class = (lv_obj_class_t){.base_class = probe->class_p,
                                  .event_cb = reader_event,
                                  .name = "DictionaryReader",
                                  .width_def = 100,
                                  .height_def = 100,
                                  .editable = LV_OBJ_CLASS_EDITABLE_TRUE,
                                  .instance_size = sizeof(lv_obj_t)};
  lv_obj_delete(probe);
  lv_obj_t *body = lv_obj_class_create_obj(&reader_class, parent);
  lv_obj_class_init_obj(body);
  return body;
}
static void *allocate(size_t n) {
  void *p = heap_caps_calloc(1, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : calloc(1, n);
}
static void say(const char *text) {
  if (state->status)
    lv_label_set_text(state->status, text);
}
static bool acquire(State *s, LockHandle lock) {
  while (!atomic_load(&s->stop))
    if (tt_lock_acquire(lock, pdMS_TO_TICKS(50)))
      return true;
  return false;
}

static void trace_append(State *s, LockHandle lock, const char *message) {
  if (!acquire(s, lock))
    return;
  FILE *f = fopen(s->trace_path, "a");
  if (f) {
    fprintf(f, "%lld %s\n", (long long)time(NULL), message);
    fclose(f);
  }
  tt_lock_release(lock);
}
static void network_trace(const char *message, void *context) {
  TraceContext *t = context;
  trace_append(t->state, t->lock, message);
}
static bool network_cancelled(void *context) {
  TraceContext *t = context;
  return atomic_load(&t->state->stop) || atomic_load(&t->state->cancel_network);
}
static void audio(State *s, Result *r, LockHandle lock) {
  char name[140], path[512];
  if (!dict_audio_name(r->job.text, name, sizeof(name))) {
    strcpy(r->message, "没有中文发音；请选英文词。");
    return;
  }
  snprintf(path, sizeof(path), "%s/audio/%s", s->base, name);
  FILE *f = NULL;
  uint32_t remaining = 0;
  if (!acquire(s, lock))
    return;
  int error = dict_wav_open(path, (void **)&f, &remaining);
  int file_errno = errno;
  tt_lock_release(lock);
  if (error) {
    if (error == -1)
      snprintf(r->message, sizeof(r->message),
               file_errno == ENOENT ? "缺少发音文件：%s" : "无法读取发音：%s",
               name);
    else
      snprintf(r->message, sizeof(r->message),
               error == -3 ? "发音格式不支持：%s" : "发音文件损坏或不完整：%s",
               name);
    char diagnostic[600];
    snprintf(diagnostic, sizeof(diagnostic),
             "audio: open path=%s result=%d errno=%d", path, error, file_errno);
    trace_append(s, lock, diagnostic);
    return;
  }
  char audio_info[180];
  snprintf(audio_info, sizeof(audio_info), "audio: file=%s pcm_bytes=%lu", name,
           (unsigned long)remaining);
  trace_append(s, lock, audio_info);
  struct Device *dev = device_find_by_name("i2s0");
  struct I2sConfig old;
  bool restore = dev && i2s_controller_get_config(dev, &old) == ERROR_NONE;
  struct I2sConfig cfg = {.communication_format = I2S_FORMAT_STAND_I2S,
                          .sample_rate = 48000,
                          .bits_per_sample = 16,
                          .channel_left = 0,
                          .channel_right = 1};
  if (!dev || !device_is_ready(dev) ||
      i2s_controller_set_config(dev, &cfg) != ERROR_NONE) {
    strcpy(r->message, "宿主 I2S 音频接口不可用。");
    goto done;
  }
  int16_t samples[1024], stereo[2048];
  uint32_t original_bytes = remaining;
  int64_t audio_started = esp_timer_get_time();
  while (remaining && !atomic_load(&s->stop)) {
    size_t n = remaining < sizeof(samples) ? remaining : sizeof(samples);
    if (!acquire(s, lock))
      break;
    size_t got = fread(samples, 1, n, f);
    tt_lock_release(lock);
    if (got != n) {
      strcpy(r->message, "读取音频失败。");
      break;
    }
    dict_stereo_frames(samples, got / 2, stereo);
    size_t stereo_bytes = got * 2;
    size_t used = 0;
    while (used < stereo_bytes && !atomic_load(&s->stop)) {
      size_t wrote = 0;
      /* The 0.7.0 driver forwards this argument to IDF's millisecond API. */
      error_t written = i2s_controller_write(dev, (uint8_t *)stereo + used,
                                             stereo_bytes - used, &wrote, 200);
      if (!wrote || (written != ERROR_NONE && written != ERROR_TIMEOUT)) {
        strcpy(r->message, "I2S 播放失败。");
        goto done;
      }
      used += wrote;
    }
    remaining -= got;
  }
  if (!remaining) {
    UART_TRACE("[dict.audio] pcm_frames=%lu expected_ms=%lu enqueue_ms=%lld\n",
               (unsigned long)(original_bytes / 2),
               (unsigned long)(original_bytes / 96),
               (long long)((esp_timer_get_time() - audio_started) / 1000));
    snprintf(audio_info, sizeof(audio_info),
             "audio: expected_ms=%lu enqueue_ms=%lld",
             (unsigned long)(original_bytes / 96),
             (long long)((esp_timer_get_time() - audio_started) / 1000));
    trace_append(s, lock, audio_info);
    vTaskDelay(pdMS_TO_TICKS(120));
    strcpy(r->message, "发音播放完成。");
  }
done:
  if (restore)
    i2s_controller_set_config(dev, &old);
  fclose(f);
}
static void run_worker(State *s, bool network) {
  LockHandle lock = tt_lock_alloc_for_path(s->base);
  TraceContext trace_context = {s, lock};
  while (!atomic_load(&s->stop)) {
    Job j;
    if (xQueueReceive(network ? s->net_jobs : s->jobs, &j, pdMS_TO_TICKS(50)) !=
        pdTRUE)
      continue;
    UART_TRACE("[dict.job] begin kind=%d id=%u network=%d\n", j.kind, j.id,
               network);
    Result *r = allocate(sizeof(*r));
    if (!r)
      continue;
    r->job = j;
    if (network)
      atomic_store(&s->cancel_network, false);
    char job_info[120];
    snprintf(job_info, sizeof(job_info), "job: begin kind=%d id=%u network=%d",
             j.kind, j.id, network);
    trace_append(s, lock, job_info);
    if (j.kind == AUDIO)
      audio(s, r, lock);
    else if (j.kind == TOUCH_CHECK) {
      struct Device *bus = device_find_by_name("i2c0");
      uint8_t reg[] = {0x81, 0x40}, data[6] = {0};
      unsigned address = 0;
      const uint8_t addresses[] = {0x5d, 0x14};
      for (unsigned i = 0; bus && i < 2; i++)
        if (i2c_controller_write_read(bus, addresses[i], reg, sizeof(reg), data,
                                      sizeof(data),
                                      pdMS_TO_TICKS(50)) == ERROR_NONE &&
            data[0] == '9' && data[1] == '1' && data[2] == '1') {
          address = addresses[i];
          break;
        }
      if (address)
        snprintf(r->message, sizeof(r->message), "GT911 0x%02x FW %02x%02x",
                 address, data[5], data[4]);
      else
        strcpy(r->message, "GT911 识别失败（0x5D / 0x14）");
      trace_append(s, lock, r->message);
    } else if (j.kind == ONLINE || j.kind == EXAMPLES) {
      WifiRadioState wifi = tt_wifi_get_radio_state();
      char wifi_info[120];
      snprintf(wifi_info, sizeof(wifi_info), "net: wifi_state=%d rssi=%d", wifi,
               wifi == WifiRadioStateConnectionActive ? tt_wifi_get_rssi() : 0);
      trace_append(s, lock, wifi_info);
      if (wifi != WifiRadioStateConnectionActive) {
        strcpy(r->message, "Wi-Fi 尚未连接。\n请在 Tactility 系统设置中连接 "
                           "可用网络，再重试在线查询。");
      } else {
        char path[512];
        snprintf(path, sizeof(path), "%s/roots.pem", s->assets);
        char *pem = NULL;
        if (acquire(s, lock)) {
          FILE *f = fopen(path, "rb");
          if (f) {
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            rewind(f);
            if (n > 0 && n < 500000) {
              pem = allocate(n + 1);
              if (pem && fread(pem, 1, n, f) != (size_t)n) {
                free(pem);
                pem = NULL;
              }
            }
            fclose(f);
          }
          tt_lock_release(lock);
        }
        trace_append(s, lock,
                     pem ? "net: certificate ready"
                         : "net: certificate missing");
        UART_TRACE("[dict.net] certificate_ready=%d\n", pem != NULL);
        if (pem) {
          r->status = online_query(j.text, j.kind == EXAMPLES, pem, r->message,
                                   sizeof(r->message), network_trace,
                                   network_cancelled, &trace_context);
          free(pem);
        } else
          strcpy(r->message, "缺少 TLS 根证书 assets/roots.pem。");
      }
    } else if (acquire(s, lock)) {
      if (j.kind == LOOKUP) {
        r->status =
            dict_lookup(s->base, j.zh, j.text, j.page, &r->entry, &r->total);
        if (r->status >= 0 && j.page == 0 &&
            !dict_book_save(s->user, false, j.text))
          strcpy(r->message, "历史记录保存失败。");
      }
      if (j.kind == CANDIDATES) {
        r->cursor = j.cursor;
        r->count = dict_candidates(s->base, j.text, &r->cursor, r->candidates,
                                   &r->more);
      }
      if (j.kind == SAVE) {
        r->status = dict_book_save(s->user, true, j.text);
        strcpy(r->message,
               r->status ? "已加入生词本。" : "保存失败，请检查 SD 卡。");
      }
      if (j.kind == HISTORY || j.kind == FAVORITES)
        r->count = dict_book_read(s->user, j.kind == FAVORITES, r->book);
      tt_lock_release(lock);
    }
    snprintf(job_info, sizeof(job_info),
             "job: complete kind=%d id=%u status=%d stack_free=%u", j.kind,
             j.id, r->status, (unsigned)uxTaskGetStackHighWaterMark(NULL));
    trace_append(s, lock, job_info);
    UART_TRACE("[dict.job] complete kind=%d id=%u status=%d\n", j.kind, j.id,
               r->status);
    if (atomic_load(&s->stop) ||
        xQueueSend(s->results, &r, pdMS_TO_TICKS(50)) != pdTRUE)
      free(r);
  }
  tt_lock_free(lock);
}
static int32_t worker(void *data) {
  run_worker(data, false);
  return 0;
}
static void *net_worker(void *data) {
  run_worker(data, true);
  return NULL;
}
static void result_page(void) {
  State *s = state;
  bool restore_scroll = s->online;
  s->result = true;
  s->online = false;
  s->book_visible = false;
  lv_label_set_text(s->speak_label, "发音P");
  lv_label_set_text(s->save_label, "收藏S");
  lv_obj_add_flag(s->input, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s->book_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(s->content, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s->bar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s->candidate_nav, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_pad_ver(s->body, 4, 0);
  if (s->total) {
    lv_label_set_text_fmt(s->content, "%s\n%s\n\n%s", s->entry.head,
                          s->entry.phonetic, s->entry.definition);
    char text[64];
    snprintf(text, sizeof(text), "义项 %u/%u · M切换", s->page + 1, s->total);
    say(text);
  } else {
    lv_label_set_text(s->content,
                      "离线未收录。\n可选择在线释义或例句继续查询。");
    say("未命中，查询文本已保留。");
  }
  lv_obj_scroll_to_y(s->body, restore_scroll ? s->offline_scroll : 0,
                     LV_ANIM_OFF);
  lv_group_focus_obj(s->body);
  lv_group_set_editing(lv_group_get_default(), true);
  input_refresh();
}
static void input_refresh(void) {
  State *s = state;
  lv_obj_remove_flag(s->body, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(s->mode_label, s->zh ? "拼I" : "EN I");
  if (s->result)
    lv_label_set_text(s->input_label, s->lookup);
  else
    lv_label_set_text_fmt(s->input_label, "%s%s%s", s->query[s->zh],
                          s->zh && *s->py ? " | " : "", s->zh ? s->py : "");
  if (!s->result) {
    lv_obj_set_style_pad_ver(s->body, s->zh ? 0 : 4, 0);
    if (s->zh && *s->py) {
      lv_obj_remove_flag(s->bar, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(s->candidate_nav, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_height(s->input, 28);
      if (*s->py)
        lv_obj_add_flag(s->body, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(s->bar, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s->candidate_nav, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_height(s->input, 32);
    }
    lv_label_set_text_fmt(s->candidate_page_label, "第 %u 组%s", s->cpage + 1,
                          s->more ? "+" : "");
    lv_obj_set_style_bg_opa(s->candidate_prev, s->cpage && s->count ? 255 : 100,
                            0);
    lv_obj_set_style_bg_opa(s->candidate_next,
                            s->more && s->count && s->cpage < 127 ? 255 : 100,
                            0);
    for (unsigned i = 0; i < 5; i++)
      lv_label_set_text_fmt(s->choice_labels[i], "%u %s", i + 1,
                            i < (unsigned)s->count ? s->candidates[i].head
                                                   : "");
  }
}
static void submit(int kind) {
  State *s = state;
  if (!s->started) {
    say("后台任务启动失败，请退出重试。");
    return;
  }
  Job j = {.kind = kind,
           .id = (kind == AUDIO || kind == SAVE) ? s->id : ++s->id,
           .page = s->page,
           .zh = s->zh,
           .cursor = s->cursor};
  const char *text = kind == CANDIDATES ? s->py
                     : kind == AUDIO    ? (s->total ? s->entry.head : s->lookup)
                                        : s->lookup;
  snprintf(j.text, sizeof(j.text), "%s", text);
  if (kind == LOOKUP) {
    dict_clean_query(s->query[s->zh]);
    snprintf(j.text, sizeof(j.text), "%s", s->query[s->zh]);
    snprintf(s->lookup, sizeof(s->lookup), "%s", j.text);
  }
  if (!*j.text && kind != HISTORY && kind != FAVORITES) {
    say("请先输入查询词。");
    return;
  }
  if (kind == ONLINE || kind == EXAMPLES) {
    atomic_store(&s->cancel_network, true);
    s->offline_scroll = lv_obj_get_scroll_y(s->body);
    s->network_waiting = true;
    s->network_tick = lv_tick_get();
    s->network_id = j.id;
  }
  bool network = kind == ONLINE || kind == EXAMPLES;
  if (network && !s->net_started) {
    say("网络任务未启动，离线查询仍可使用。");
    return;
  }
  if (kind == CANDIDATES)
    s->candidates_pending = true;
  xQueueOverwrite(network ? s->net_jobs : s->jobs, &j);
  if (s->result && kind != HISTORY && kind != FAVORITES) {
    lv_group_focus_obj(s->body);
    lv_group_set_editing(lv_group_get_default(), true);
  }
  s->busy = true;
  say(kind == CANDIDATES ? "查找候选…"
      : kind == AUDIO    ? "播放中…"
      : kind == LOOKUP   ? "离线查询…"
      : kind == SAVE     ? "保存…"
                         : "联网查询，可返回取消显示…");
}
static void search_page(void) {
  State *s = state;
  ++s->id;
  s->result = false;
  s->online = false;
  s->book_visible = false;
  s->command = false;
  s->network_waiting = false;
  atomic_store(&s->cancel_network, true);
  lv_label_set_text(s->speak_label, "查词L");
  lv_label_set_text(s->save_label, "清空C");
  lv_obj_remove_flag(s->input, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s->book_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(s->content, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(s->content,
                    "输入英文或中文拼音，Enter 查词。\n$ 切换输入法；数字 1–5 "
                    "或轨迹球选词。\n麦克风键播放当前词；数字 0 "
                    "不发音。\n在线内容仅在点击后请求。");
  input_refresh();
  lv_group_focus_obj(s->input);
  lv_group_set_editing(lv_group_get_default(), false);
  if (s->zh && *s->py) {
    s->cursor = s->cursors[s->cpage];
    s->pending_selected = s->selected;
    s->edit_tick = lv_tick_get();
  }
  say("快捷键：# + 菜单字母");
}
static void choose(unsigned i) {
  State *s = state;
  if (i >= (unsigned)s->count)
    return;
  char *q = s->query[1];
  size_t n = strlen(q), m = strlen(s->candidates[i].head);
  if (n + m >= 192) {
    say("查询文本过长。");
    return;
  }
  memcpy(q + n, s->candidates[i].head, m + 1);
  s->py[0] = 0;
  s->count = 0;
  s->candidates_pending = false;
  s->selected = 0;
  s->pending_selected = 0;
  s->cursor = 0;
  s->cpage = 0;
  s->edit_tick = 0;
  ++s->id;
  input_refresh();
  lv_group_focus_obj(s->input);
  say("已选词；Enter 查词。");
}
static void remove_last(char *text) {
  size_t n = strlen(text);
  if (!n)
    return;
  do {
    n--;
  } while (n && ((unsigned char)text[n] & 0xc0) == 0x80);
  text[n] = 0;
}
static int shortcut_action(uint32_t key) {
  if (key >= 'a' && key <= 'z')
    key -= 32;
  switch (key) {
  case 'B':
    return 1;
  case 'I':
    return 2;
  case 'U':
    return 3;
  case 'D':
    return 4;
  case 'H':
    return 10;
  case 'F':
    return 11;
  case 'L':
    return 5;
  case 'P':
    return 12;
  case 'O':
    return 6;
  case 'E':
    return 7;
  case 'S':
    return 14;
  case 'C':
    return 13;
  case 'M':
    return 16;
  case 'X':
    return 17;
  case 'T':
    return 19;
  default:
    return -1;
  }
}
static void key(lv_event_t *e) {
  State *s = state;
  uint32_t k = lv_event_get_key(e);
  if (s->touch_panel) {
    if (k == 't' || k == 'T' || k == 'x' || k == 'X' || k == LV_KEY_BACKSPACE ||
        k == 27)
      perform(20);
    return;
  }
  lv_obj_t *target = lv_event_get_target_obj(e);
  UART_TRACE("[VibeDictionary] key=0x%02lX\n", (unsigned long)k);
  if (k == '#') {
    if (s->command) {
      s->command = false;
      if (!s->result && !s->zh) {
        size_t n = strlen(s->query[0]);
        if (n + 1 < 192) {
          s->query[0][n] = '#';
          s->query[0][n + 1] = 0;
          input_refresh();
        }
      }
    } else {
      s->command = true;
      say("命令：按菜单标注字母");
    }
    return;
  }
  if (s->command) {
    s->command = false;
    if (k == LV_KEY_BACKSPACE || k == 27) {
      say("已取消菜单命令。");
      return;
    }
    int action = shortcut_action(k);
    if ((action == 3 || action == 4) && !s->result)
      say("候选翻页：左 Shift 上一组，右 Shift 下一组。");
    else if (action >= 0)
      perform(action);
    else
      say("未匹配菜单快捷键。");
    return;
  }
  if (s->result) {
    int action = shortcut_action(k);
    if (action >= 0) {
      perform(action);
      return;
    }
  }
  /* This is independent of the numeric layer's ASCII '0'. */
  if (k == 0xf0) {
    if (s->result)
      submit(AUDIO);
    return;
  }
  if (k == '$') {
    s->zh = !s->zh;
    search_page();
    return;
  }
  if (!s->result && target == s->bar &&
      (k == LV_KEY_LEFT || k == LV_KEY_RIGHT || k == LV_KEY_UP ||
       k == LV_KEY_DOWN)) {
    if (s->candidates_pending || !s->count)
      return;
    int direction = k == LV_KEY_LEFT || k == LV_KEY_UP ? -1 : 1;
    int next = s->selected + direction;
    if (next < 0) {
      perform(3);
      return;
    }
    if (next >= s->count) {
      perform(4);
      return;
    }
    s->selected = next;
    candidate_selection();
    return;
  }
  if (s->result) {
    if (k == LV_KEY_BACKSPACE || k == 27) {
      if (s->online)
        result_page();
      else
        search_page();
    } else if (target == s->body && (k == LV_KEY_DOWN || k == LV_KEY_UP ||
                                     k == LV_KEY_LEFT || k == LV_KEY_RIGHT))
      scroll_text(k == LV_KEY_DOWN || k == LV_KEY_RIGHT ? 1 : -1, false);
    return;
  }
  bool candidate = target == s->bar;
  for (unsigned i = 0; i < 5; i++)
    if (target == s->choices[i])
      candidate = true;
  if (target != s->input && !candidate)
    return;
  if (s->zh && s->count && k >= '1' && k <= '5') {
    choose(k - '1');
    return;
  }
  if (k == LV_KEY_ENTER || k == 13) {
    if (target == s->bar && *s->py) {
      choose(s->selected);
      return;
    }
    if (target != s->input)
      return;
    if (s->zh && *s->py) {
      choose(0);
      return;
    }
    s->page = 0;
    submit(LOOKUP);
    return;
  }
  if (k == LV_KEY_BACKSPACE) {
    if (s->zh && *s->py)
      remove_last(s->py);
    else
      remove_last(s->query[s->zh]);
  } else if (k == ' ' && s->zh && *s->py) {
    choose(target == s->bar ? s->selected : 0);
    return;
  } else if (k >= 32 && k < 127) {
    char *q = s->zh ? s->py : s->query[0];
    size_t n = strlen(q), cap = s->zh ? sizeof(s->py) : 192;
    if (s->zh) {
      if (k >= 'A' && k <= 'Z')
        k += 32;
      if (k < 'a' || k > 'z')
        return;
    }
    if (n + 1 < cap) {
      q[n] = (char)k;
      q[n + 1] = 0;
    }
  } else
    return;
  ++s->id;
  s->count = 0;
  s->candidates_pending = false;
  s->selected = 0;
  s->pending_selected = 0;
  s->cursor = 0;
  s->cpage = 0;
  s->edit_tick = lv_tick_get();
  input_refresh();
}
static void clicked(lv_event_t *e) {
  intptr_t action = (intptr_t)lv_event_get_user_data(e);
  lv_indev_t *indev = lv_event_get_indev(e);
  /* Enter already submits on KEY; its CLICKED release must not reset query/id.
   */
  if (action == 9 && indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD)
    return;
  perform(action);
}
static void perform(intptr_t action) {
  State *s = state;
  if (action == 19) {
    touch_test();
    return;
  }
  if (action == 20) {
    if (s->touch_panel)
      lv_obj_delete(s->touch_panel);
    s->touch_panel = s->touch_info = NULL;
    lv_timer_set_period(s->timer, 50);
    lv_group_focus_obj(s->result ? s->body : s->input);
    return;
  }
  if (action == 18) {
    choose(s->selected);
    return;
  }
  if (action == 17) {
    atomic_store(&s->cancel_network, true);
    tt_app_stop();
    return;
  }
  if (action == 12) {
    if (s->result)
      submit(AUDIO);
    else
      say("请先查词，再发音。");
    return;
  }
  if (action == 13) {
    s->query[s->zh][0] = 0;
    s->py[0] = 0;
    s->count = 0;
    search_page();
    return;
  }
  if (action == 14) {
    if (s->result)
      submit(SAVE);
    else
      say("请先查词，再收藏。");
    return;
  }
  if (action == 16) {
    if (s->result && s->total > 1) {
      s->page = (s->page + 1) % s->total;
      submit(LOOKUP);
    }
    return;
  }
  if (action >= 200) {
    unsigned i = action - 200;
    if (i >= s->book_count)
      return;
    const char *word = s->book[i];
    s->zh = false;
    for (const unsigned char *p = (const unsigned char *)word; *p; p++)
      if (*p >= 128)
        s->zh = true;
    snprintf(s->query[s->zh], 192, "%s", word);
    s->py[0] = 0;
    search_page();
    s->page = 0;
    submit(LOOKUP);
    return;
  }
  if (action == 10 || action == 11) {
    s->edit_tick = 0;
    submit(action == 10 ? HISTORY : FAVORITES);
    return;
  }
  if (action >= 100) {
    choose(action - 100);
    return;
  }
  if (action == 1) {
    if (s->book_visible) {
      search_page();
      return;
    }
    if (s->online)
      result_page();
    else if (s->result)
      search_page();
    else
      tt_app_stop();
  }
  if (action == 2) {
    s->zh = !s->zh;
    search_page();
  }
  if (action == 3 || action == 4) {
    if (s->result) {
      scroll_text(action == 3 ? -1 : 1, true);
      lv_group_focus_obj(s->body);
      lv_group_set_editing(lv_group_get_default(), true);
    } else if (s->zh && *s->py) {
      if (s->candidates_pending || !s->count) {
        say("候选加载中，请稍候。");
        return;
      }
      if (action == 3 && s->cpage) {
        s->cpage--;
        s->pending_selected = 4;
        s->cursor = s->cursors[s->cpage];
        submit(CANDIDATES);
      }
      if (action == 4 && s->more && s->cpage < 127) {
        s->cpage++;
        s->pending_selected = 0;
        s->cursors[s->cpage] = s->cursor;
        submit(CANDIDATES);
      }
    }
  }
  if (action == 5) {
    if (s->result)
      submit(AUDIO);
    else if (s->zh && *s->py)
      choose(0);
    else {
      s->page = 0;
      submit(LOOKUP);
    }
  }
  if (action == 6 || action == 7) {
    if (s->result)
      submit(action == 6 ? ONLINE : EXAMPLES);
    else
      say("先查词，再选择在线功能。");
  }
  if (action == 8) {
    if (s->result)
      submit(SAVE);
    else {
      s->query[s->zh][0] = 0;
      s->py[0] = 0;
      s->count = 0;
      search_page();
    }
  }
  if (action == 9)
    search_page();
}
static void candidate_selection(void) {
  State *s = state;
  for (unsigned i = 0; i < 5; i++) {
    lv_obj_set_style_border_width(
        s->choices[i], i < (unsigned)s->count && (int)i == s->selected ? 2 : 0,
        0);
    lv_obj_set_style_border_color(s->choices[i], lv_color_hex(0xE56A3B), 0);
  }
  if (s->count > 0 && s->selected < s->count)
    lv_label_set_text_fmt(s->content, "%s", s->candidates[s->selected].head);
}
static void candidate_focused(lv_event_t *e) {
  State *s = state;
  unsigned i = (unsigned)(uintptr_t)lv_event_get_user_data(e);
  if (!s->result && i < (unsigned)s->count)
    lv_label_set_text_fmt(s->content, "候选 %u：%s\n拼音：%s", i + 1,
                          s->candidates[i].head, s->py);
}
static lv_obj_t *button(lv_obj_t *parent, const char *text, int action,
                        int width, lv_obj_t **label) {
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_set_size(b, width, 36);
  lv_obj_set_style_radius(b, 5, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(0xDDD8D1), 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(action == 5 ? 0xE56A3B : 0xEDE9E3),
                            0);
  lv_obj_set_style_text_color(
      b, lv_color_hex(action == 5 ? 0xFFFFFF : 0x302E2B), 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(0xD9C9BD), LV_STATE_PRESSED);
  lv_obj_set_style_outline_color(b, lv_color_hex(0xE56A3B), LV_STATE_FOCUSED);
  lv_obj_set_style_pad_all(b, 2, 0);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, clicked, LV_EVENT_CLICKED, (void *)(intptr_t)action);
  lv_obj_add_event_cb(b, key, LV_EVENT_KEY, NULL);
  if (action >= 100 && action < 105)
    lv_obj_add_event_cb(b, candidate_focused, LV_EVENT_FOCUSED,
                        (void *)(uintptr_t)(action - 100));
  if (label)
    *label = l;
  return b;
}
static void book_view(Result *r) {
  State *s = state;
  s->book_visible = true;
  s->edit_tick = 0;
  lv_group_set_editing(lv_group_get_default(), false);
  lv_label_set_text(s->speak_label, "查词L");
  lv_label_set_text(s->save_label, "清空C");
  s->result = false;
  s->book_count = r->count < 0 ? 0 : (unsigned)r->count;
  memcpy(s->book, r->book, sizeof(s->book));
  lv_obj_add_flag(s->bar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s->candidate_nav, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s->content, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(s->book_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clean(s->book_panel);
  button(s->book_panel, r->job.kind == HISTORY ? "查看生词本F" : "查看历史H",
         r->job.kind == HISTORY ? 11 : 10, LV_PCT(100), NULL);
  button(s->book_panel, "触屏测试T", 19, LV_PCT(100), NULL);
  for (unsigned i = s->book_count; i > 0; i--)
    button(s->book_panel, s->book[i - 1], 200 + i - 1, LV_PCT(100), NULL);
  lv_obj_scroll_to_y(s->body, 0, LV_ANIM_OFF);
  say(r->count < 0             ? "记录读取失败。"
      : r->job.kind == HISTORY ? "最近 20 条历史"
                               : "最近 20 个生词（完整记录保留在 SD）");
}
static void tick(lv_timer_t *timer) {
  (void)timer;
  State *s = state;
  poll_shift_page(s);
  if (s->touch_panel) {
    lv_point_t point = {0};
    bool pressed = false;
    if (s->touch_pointer) {
      lv_indev_get_point(s->touch_pointer, &point);
      pressed = lv_indev_get_state(s->touch_pointer) == LV_INDEV_STATE_PRESSED;
    }
    if (pressed && !s->touch_was_pressed)
      s->touch_presses++;
    s->touch_was_pressed = pressed;
    lv_label_set_text_fmt(s->touch_info, "%s\n指针%u %s %ld,%ld · 触点%u",
                          s->touch_details, s->touch_pointer_count,
                          pressed ? "按下" : "松开", (long)point.x,
                          (long)point.y, s->touch_presses);
  }
  Result *r;
  while (xQueueReceive(s->results, &r, 0) == pdTRUE) {
    if (r->job.id == s->id) {
      if (r->job.kind == ONLINE || r->job.kind == EXAMPLES)
        s->network_waiting = false;
      s->busy = false;
      if (r->job.kind == TOUCH_CHECK) {
        snprintf(s->touch_details, sizeof(s->touch_details), "%.95s",
                 r->message);
      } else if (r->job.kind == LOOKUP) {
        s->total = r->total;
        s->entry = r->entry;
        if (r->status < 0) {
          s->total = 0;
          lv_label_set_text(
              s->content,
              "SD 词库读取失败，请检查 cardputer/dictionary 文件。");
          say("数据读取失败。");
        } else
          result_page();
      } else if (r->job.kind == CANDIDATES) {
        s->candidates_pending = false;
        s->cursors[s->cpage] = r->job.cursor;
        s->count = r->count < 0 ? 0 : r->count;
        memcpy(s->candidates, r->candidates, sizeof(s->candidates));
        s->cursor = r->cursor;
        s->more = r->more;
        s->selected =
            s->count > 0 ? (s->pending_selected < s->count ? s->pending_selected
                                                           : s->count - 1)
                         : 0;
        input_refresh();
        candidate_selection();
        if (s->count > 0) {
          lv_group_focus_obj(s->bar);
          lv_group_set_editing(lv_group_get_default(), true);
        }
        char msg[64];
        snprintf(msg, sizeof(msg), "候选第 %u 页%s", s->cpage + 1,
                 s->more ? " · 可下一页" : "");
        say(r->count < 0 ? "拼音索引读取失败。" : msg);
      } else if (r->job.kind == ONLINE || r->job.kind == EXAMPLES) {
        s->online = true;
        lv_label_set_text(s->content, r->message);
        say(r->status ? "在线结果 · 返回保留离线词条"
                      : "在线请求失败 · 可返回");
        lv_obj_scroll_to_y(s->body, 0, LV_ANIM_OFF);
        lv_group_focus_obj(s->body);
        lv_group_set_editing(lv_group_get_default(), true);
      } else if (r->job.kind == HISTORY || r->job.kind == FAVORITES)
        book_view(r);
      else
        say(r->message);
    }
    free(r);
  }
  if (s->network_waiting && s->network_id == s->id &&
      lv_tick_elaps(s->network_tick) > 25000) {
    s->network_waiting = false;
    atomic_store(&s->cancel_network, true);
    ++s->id;
    s->online = true;
    lv_label_set_text(s->content, "在线请求超过 25 秒。\n请检查 "
                                  "Wi-"
                                  "Fi、时间与网络访问。\n离线查词仍可使用；返回"
                                  "可继续阅读。\n\n后台请求结束后可以重试。");
    say("联网超时，已停止等待结果。");
    UART_TRACE("[dict.net] UI watchdog expired\n");
  }
  if (!s->touch_panel && !s->result && s->zh && *s->py && s->edit_tick &&
      lv_tick_elaps(s->edit_tick) > 220) {
    s->edit_tick = 0;
    submit(CANDIDATES);
  }
}
static void *create(void) {
  State *s = allocate(sizeof(*s));
  if (s) {
    atomic_init(&s->stop, false);
    atomic_init(&s->cancel_network, false);
  }
  return s;
}
static void on_create(AppHandle app, void *data) {
  State *s = data;
  if (!s)
    return;
  s->app = app;
  size_t n = sizeof(s->assets);
  tt_app_get_assets_path(app, s->assets, &n);
  n = sizeof(s->user);
  tt_app_get_user_data_path(app, s->user, &n);
  const char *bases[] = {"/sdcard/cardputer/dictionary",
                         "/sd/cardputer/dictionary"};
  strcpy(s->base, bases[0]);
  for (unsigned i = 0; i < 2; i++) {
    char path[300];
    snprintf(path, sizeof(path), "%s/manifest.json", bases[i]);
    FILE *f = fopen(path, "rb");
    if (f) {
      fclose(f);
      strcpy(s->base, bases[i]);
      break;
    }
  }
  const char *slash = strchr(s->base + 1, '/');
  snprintf(s->trace_path, sizeof(s->trace_path), "%.*s/vibe-dictionary.log",
           slash ? (int)(slash - s->base) : (int)strlen(s->base), s->base);
  s->jobs = xQueueCreate(1, sizeof(Job));
  s->results = xQueueCreate(4, sizeof(Result *));
  s->net_jobs = xQueueCreate(1, sizeof(Job));
  if (s->jobs && s->results) {
    s->local_thread =
        thread_alloc_full("dictionary", 16384, worker, s, tskNO_AFFINITY);
    s->started = s->local_thread && thread_start(s->local_thread) == ERROR_NONE;
  }
  if (s->net_jobs && s->results) {
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) == 0 &&
        pthread_attr_setstacksize(&attr, 16384) == 0) {
      /* Tactility 0.7 Thread and IDF pthread both use FreeRTOS TLS slot 0.
       * pthread_create only reads its caller's configuration from that slot.
       * Do not let it interpret the kernel Thread pointer as a pthread list.
       * The new pthread owns clean TLS and is joined before App unload. */
      void *saved = pvTaskGetThreadLocalStoragePointer(NULL, 0);
      vTaskSetThreadLocalStoragePointer(NULL, 0, NULL);
      int error = pthread_create(&s->network_thread, &attr, net_worker, s);
      vTaskSetThreadLocalStoragePointer(NULL, 0, saved);
      s->net_started = error == 0;
    }
  }
}
static void on_show(AppHandle app, void *data, lv_obj_t *parent) {
  (void)app;
  State *s = data;
  if (!s)
    return;
  state = s;
  s->old_editing = lv_group_get_editing(lv_group_get_default());
  char path[512];
  snprintf(path, sizeof(path), "%s/font18.bin", s->assets);
  bool font_ok = sd_font_open(path);
  s->root = lv_obj_create(parent);
  lv_obj_set_size(s->root, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(s->root, 0, 0);
  lv_obj_set_style_border_width(s->root, 0, 0);
  lv_obj_set_flex_flow(s->root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s->root, 2, 0);
  lv_obj_set_style_text_font(s->root, sd_font_get(), 0);
  lv_obj_set_style_bg_color(s->root, lv_color_hex(0xF5F3EF), 0);
  lv_obj_set_style_text_color(s->root, lv_color_hex(0x302E2B), 0);
  lv_obj_t *h = lv_obj_create(s->root);
  lv_obj_set_size(h, LV_PCT(100), 36);
  lv_obj_set_style_bg_color(h, lv_color_hex(0xF5F3EF), 0);
  lv_obj_set_style_border_width(h, 0, 0);
  lv_obj_set_style_pad_all(h, 0, 0);
  lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
  lv_obj_remove_flag(h, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_column(h, 2, 0);
  button(h, "返B", 1, 40, NULL);
  button(h, "EN", 2, 46, &s->mode_label);
  button(h, "上页U", 3, 54, NULL);
  button(h, "下页D", 4, 54, NULL);
  button(h, "记录H", 10, 60, NULL);
  button(h, "X", 17, 38, NULL);
  s->input = lv_obj_create(s->root);
  lv_obj_set_size(s->input, LV_PCT(100), 32);
  lv_obj_set_style_pad_all(s->input, 2, 0);
  lv_obj_set_style_bg_color(s->input, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_color(s->input, lv_color_hex(0x302E2B), 0);
  lv_obj_set_style_border_color(s->input, lv_color_hex(0xE56A3B), 0);
  lv_obj_set_style_border_width(s->input, 1, 0);
  lv_obj_set_style_radius(s->input, 4, 0);
  lv_obj_add_flag(s->input, LV_OBJ_FLAG_CLICKABLE);
  lv_group_add_obj(lv_group_get_default(), s->input);
  s->input_label = lv_label_create(s->input);
  lv_obj_set_style_text_color(s->input_label, lv_color_hex(0x302E2B), 0);
  lv_obj_set_width(s->input_label, LV_PCT(100));
  lv_label_set_long_mode(s->input_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_add_event_cb(s->input, key, LV_EVENT_KEY, NULL);
  lv_obj_add_event_cb(s->input, clicked, LV_EVENT_CLICKED, (void *)9);
  s->bar = reader_create(s->root);
  lv_obj_set_size(s->bar, LV_PCT(100), 38);
  lv_obj_set_style_bg_color(s->bar, lv_color_hex(0xF5F3EF), 0);
  lv_obj_set_style_border_width(s->bar, 0, 0);
  lv_group_add_obj(lv_group_get_default(), s->bar);
  lv_obj_add_event_cb(s->bar, key, LV_EVENT_KEY, NULL);
  lv_obj_add_event_cb(s->bar, clicked, LV_EVENT_CLICKED, (void *)18);
  lv_obj_set_style_pad_all(s->bar, 0, 0);
  lv_obj_set_style_pad_column(s->bar, 1, 0);
  lv_obj_set_flex_flow(s->bar, LV_FLEX_FLOW_ROW);
  lv_obj_remove_flag(s->bar, LV_OBJ_FLAG_SCROLLABLE);
  for (unsigned i = 0; i < 5; i++) {
    s->choices[i] = button(s->bar, "", 100 + i, 61, &s->choice_labels[i]);
    lv_group_remove_obj(s->choices[i]);
  }
  s->candidate_nav = lv_obj_create(s->root);
  lv_obj_set_size(s->candidate_nav, LV_PCT(100), 36);
  lv_obj_set_style_bg_color(s->candidate_nav, lv_color_hex(0xF5F3EF), 0);
  lv_obj_set_style_text_color(s->candidate_nav, lv_color_hex(0x302E2B), 0);
  lv_obj_set_style_pad_all(s->candidate_nav, 0, 0);
  lv_obj_set_style_border_width(s->candidate_nav, 0, 0);
  lv_obj_set_style_pad_column(s->candidate_nav, 2, 0);
  lv_obj_set_flex_flow(s->candidate_nav, LV_FLEX_FLOW_ROW);
  lv_obj_remove_flag(s->candidate_nav, LV_OBJ_FLAG_SCROLLABLE);
  s->candidate_prev = button(s->candidate_nav, "左Shift", 3, 94, NULL);
  lv_obj_set_height(s->candidate_prev, 34);
  lv_obj_set_style_pad_ver(s->candidate_prev, 0, 0);
  s->candidate_page_label = lv_label_create(s->candidate_nav);
  lv_obj_set_style_text_color(s->candidate_page_label, lv_color_hex(0x302E2B),
                              0);
  lv_obj_set_width(s->candidate_page_label, 100);
  lv_label_set_text(s->candidate_page_label, "第 1 组");
  s->candidate_next = button(s->candidate_nav, "右Shift", 4, 94, NULL);
  lv_obj_set_height(s->candidate_next, 34);
  lv_obj_set_style_pad_ver(s->candidate_next, 0, 0);
  s->body = reader_create(s->root);
  lv_obj_set_width(s->body, LV_PCT(100));
  lv_obj_set_flex_grow(s->body, 1);
  lv_obj_set_style_pad_all(s->body, 4, 0);
  lv_obj_set_style_bg_color(s->body, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_color(s->body, lv_color_hex(0x302E2B), 0);
  lv_obj_set_style_border_width(s->body, 0, 0);
  lv_obj_set_style_radius(s->body, 3, 0);
  lv_group_add_obj(lv_group_get_default(), s->body);
  lv_obj_add_event_cb(s->body, key, LV_EVENT_KEY, NULL);
  s->content = lv_label_create(s->body);
  lv_obj_set_style_text_color(s->content, lv_color_hex(0x302E2B), 0);
  lv_obj_set_width(s->content, LV_PCT(100));
  lv_label_set_long_mode(s->content, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_line_space(s->content, 2, 0);
  s->book_panel = lv_obj_create(s->body);
  lv_obj_set_width(s->book_panel, LV_PCT(100));
  lv_obj_set_height(s->book_panel, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(s->book_panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(s->book_panel, 0, 0);
  lv_obj_set_style_bg_color(s->book_panel, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_color(s->book_panel, lv_color_hex(0x302E2B), 0);
  lv_obj_t *a = lv_obj_create(s->root);
  lv_obj_set_size(a, LV_PCT(100), 38);
  lv_obj_set_style_bg_color(a, lv_color_hex(0xF5F3EF), 0);
  lv_obj_set_style_border_width(a, 0, 0);
  lv_obj_set_style_pad_all(a, 0, 0);
  lv_obj_set_style_pad_column(a, 2, 0);
  lv_obj_set_flex_flow(a, LV_FLEX_FLOW_ROW);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_SCROLLABLE);
  button(a, "查词L", 5, 76, &s->speak_label);
  button(a, "在线O", 6, 76, NULL);
  button(a, "例句E", 7, 76, NULL);
  button(a, "清空C", 8, 76, &s->save_label);
  s->status = lv_label_create(s->root);
  lv_obj_set_style_text_color(s->status, lv_color_hex(0x302E2B), 0);
  lv_obj_set_width(s->status, LV_PCT(100));
  lv_label_set_long_mode(s->status, LV_LABEL_LONG_DOT);
  lv_obj_add_flag(s->status, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s->status, clicked, LV_EVENT_CLICKED, (void *)16);
  s->timer = lv_timer_create(tick, 50, NULL);
  UART_TRACE("[VibeDictionary] viewport=%ldx%ld font=%d\n",
             (long)lv_obj_get_content_width(parent),
             (long)lv_obj_get_content_height(parent), font_ok);
  search_page();
  if (!font_ok) {
    lv_obj_set_style_text_font(s->status, lv_font_get_default(), 0);
    say("Missing font18.bin: reinstall complete assets");
  }
}
static void on_hide(AppHandle app, void *data) {
  (void)app;
  State *s = data;
  if (!s)
    return;
  if (s->touch_panel) {
    lv_obj_delete(s->touch_panel);
    s->touch_panel = s->touch_info = NULL;
  }
  if (s->timer) {
    lv_timer_delete(s->timer);
    s->timer = NULL;
  }
  if (s->keyboard_i2c)
    restore_keyboard(s);
  ++s->id;
  atomic_store(&s->cancel_network, true);
  lv_group_set_editing(lv_group_get_default(), s->old_editing);
  s->root = NULL;
  s->status = NULL;
  sd_font_close();
}
static void destroy(void *data) {
  State *s = data;
  if (!s)
    return;
  atomic_store(&s->stop, true);
  if (s->started)
    thread_join(s->local_thread, portMAX_DELAY, pdMS_TO_TICKS(1));
  if (s->net_started)
    pthread_join(s->network_thread, NULL);
  if (s->local_thread)
    thread_free(s->local_thread);
  Result *r;
  if (s->results) {
    while (xQueueReceive(s->results, &r, 0) == pdTRUE)
      free(r);
    vQueueDelete(s->results);
  }
  if (s->jobs)
    vQueueDelete(s->jobs);
  if (s->net_jobs)
    vQueueDelete(s->net_jobs);
  if (state == s)
    state = NULL;
  free(s);
}
int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  tt_app_register((AppRegistration){.createData = create,
                                    .destroyData = destroy,
                                    .onCreate = on_create,
                                    .onShow = on_show,
                                    .onHide = on_hide});
  return 0;
}
