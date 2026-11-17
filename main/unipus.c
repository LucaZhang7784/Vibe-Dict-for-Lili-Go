#include "unipus.h"
#include <mbedtls/cipher.h>
#include <stdio.h>
#include <string.h>

bool unipus_token(int64_t timestamp, char output[64]) {
  /* Official test credential; not a user secret or a production entitlement. */
  static const unsigned char key[] = "F7724780E4BB3C9FC0F1D88B8E106CC7";
  static const unsigned char iv[] = {1, 2,  3,  4,  5,  6,  7,  8,
                                     9, 10, 11, 12, 13, 14, 15, 16};
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  output[0] = 0;
  if (timestamp <= 0)
    return false;
  char plain[32];
  int length = snprintf(plain, sizeof(plain), "%lld", (long long)timestamp);
  if (length <= 0 || length >= (int)sizeof(plain))
    return false;
  unsigned char encrypted[64];
  size_t written = 0;
  /* Explicit PKCS7 blocks: the SDK exports CBC update but not padding setup. */
  size_t padded = ((size_t)length / 16 + 1) * 16;
  memset(plain + length, (int)(padded - (size_t)length),
         padded - (size_t)length);
  bool ok = false;
  mbedtls_cipher_context_t ctx;
  mbedtls_cipher_init(&ctx);
  const mbedtls_cipher_info_t *info =
      mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CBC);
  if (!info || mbedtls_cipher_setup(&ctx, info) ||
      mbedtls_cipher_setkey(&ctx, key, 256, MBEDTLS_ENCRYPT) ||
      mbedtls_cipher_set_iv(&ctx, iv, sizeof(iv)) ||
      mbedtls_cipher_reset(&ctx) ||
      mbedtls_cipher_update(&ctx, (const unsigned char *)plain, padded,
                            encrypted, &written))
    goto done;
  if (written != padded || written > 45)
    goto done;
  size_t out = 0;
  for (size_t i = 0; i < written; i += 3) {
    uint32_t bits = (uint32_t)encrypted[i] << 16;
    if (i + 1 < written)
      bits |= (uint32_t)encrypted[i + 1] << 8;
    if (i + 2 < written)
      bits |= encrypted[i + 2];
    output[out++] = alphabet[(bits >> 18) & 63];
    output[out++] = alphabet[(bits >> 12) & 63];
    output[out++] = i + 1 < written ? alphabet[(bits >> 6) & 63] : '=';
    output[out++] = i + 2 < written ? alphabet[bits & 63] : '=';
  }
  output[out] = 0;
  ok = true;
done:
  mbedtls_cipher_free(&ctx);
  return ok;
}
static const char *text(cJSON *json, const char *key) {
  cJSON *value = cJSON_GetObjectItemCaseSensitive(json, key);
  return cJSON_IsString(value) ? value->valuestring : "";
}
static void append(char *out, size_t cap, const char *value) {
  size_t n = strlen(out), size = strlen(value);
  if (n + size < cap)
    memcpy(out + n, value, size + 1);
}
static void examples_from(cJSON *senses, char *out, size_t cap,
                          unsigned *count) {
  cJSON *sense = NULL, *example = NULL;
  cJSON_ArrayForEach(sense, senses) {
    cJSON_ArrayForEach(example,
                       cJSON_GetObjectItemCaseSensitive(sense, "example")) {
      if (*count >= 5)
        return;
      if (!*text(example, "ex"))
        continue;
      (*count)++;
      append(out, cap, text(example, "ex"));
      append(out, cap, "\n");
      append(out, cap, text(example, "trans"));
      append(out, cap, "\n\n");
    }
  }
}
bool unipus_format(cJSON *json, bool examples, char *out, size_t cap) {
  if (!cap)
    return false;
  out[0] = 0;
  cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
  if (!cJSON_IsNumber(code))
    return false;
  if (code->valueint) {
    snprintf(out, cap,
             "外研接口返回 "
             "%d：%s\n试用授权或时间校验未通过时，请核对系统时间与授权。",
             code->valueint, text(json, "msg"));
    return false;
  }
  cJSON *value = cJSON_GetObjectItemCaseSensitive(json, "value");
  cJSON *list = cJSON_GetObjectItemCaseSensitive(value, "list");
  if (!cJSON_IsArray(list) || !cJSON_GetArraySize(list)) {
    snprintf(out, cap,
             "本次试用查询未返回词条。\n可返回继续阅读离线结果。\n\n来源：外研"
             "在线 · 官方试用");
    return true;
  }
  cJSON *entry = NULL, *pron = NULL, *category = NULL, *sense = NULL,
        *phrase = NULL;
  unsigned count = 0;
  cJSON_ArrayForEach(entry, list) {
    cJSON *word = cJSON_GetObjectItemCaseSensitive(entry, "wordValue");
    append(out, cap, text(word, "word"));
    append(out, cap, "\n");
    append(out, cap, text(entry, "dictName"));
    append(out, cap, "\n\n");
    cJSON_ArrayForEach(pron, cJSON_GetObjectItemCaseSensitive(word, "pron")) {
      append(out, cap, text(pron, "type"));
      append(out, cap, " ");
      append(out, cap, text(pron, "value"));
      append(out, cap, "\n");
    }
    cJSON_ArrayForEach(category,
                       cJSON_GetObjectItemCaseSensitive(word, "category")) {
      cJSON *senses = cJSON_GetObjectItemCaseSensitive(category, "sense");
      if (examples) {
        examples_from(senses, out, cap, &count);
        cJSON_ArrayForEach(phrase,
                           cJSON_GetObjectItemCaseSensitive(category, "phrase"))
            examples_from(cJSON_GetObjectItemCaseSensitive(phrase, "sense"),
                          out, cap, &count);
      } else {
        append(out, cap, text(category, "cat"));
        append(out, cap, "\n");
        cJSON_ArrayForEach(sense, senses) {
          append(out, cap, text(sense, "trans"));
          append(out, cap, "\n");
        }
        append(out, cap, "\n");
      }
    }
  }
  if (examples && !count)
    append(out, cap, "本词条没有可返回的例句。\n");
  append(out, cap, "\n来源：外研在线 · 官方试用\n");
  return true;
}
