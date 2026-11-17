#include "dict_core.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Explicit ASCII avoids imported libc ctype data relocation in external apps.
 */
static unsigned ascii_lower(unsigned c) {
  return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}
static bool ascii_space(unsigned c) {
  return c == ' ' || (c >= '\t' && c <= '\r');
}

void dict_clean_query(char *query) {
  size_t first = 0, last = strlen(query);
  while (first < last && ascii_space((unsigned char)query[first]))
    first++;
  while (last > first && ascii_space((unsigned char)query[last - 1]))
    last--;
  memmove(query, query + first, last - first);
  query[last - first] = 0;
}
void dict_stereo_frames(const int16_t *mono, size_t frames, int16_t *stereo) {
  for (size_t i = 0; i < frames; i++)
    stereo[2 * i] = stereo[2 * i + 1] = mono[i] / 3;
}
int dict_scroll_target(int current, int maximum, int viewport, int direction,
                       bool page) {
  int step = page ? (viewport > 24 ? viewport - 24 : 24) : 24;
  int target = current + (direction > 0 ? step : -step);
  if (target < 0)
    target = 0;
  if (maximum < 0)
    maximum = 0;
  return target > maximum ? maximum : target;
}
static uint32_t u32(const unsigned char *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static unsigned u16(const unsigned char *p) {
  return p[0] | ((unsigned)p[1] << 8);
}
static bool path_join(char *dst, size_t n, const char *base, const char *leaf) {
  int count = snprintf(dst, n, "%s/%s", base, leaf);
  return count > 0 && (size_t)count < n;
}
static bool record(FILE *f, uint32_t i, unsigned size, unsigned char *out) {
  return i <= LONG_MAX / size && fseek(f, (long)i * size, SEEK_SET) == 0 &&
         fread(out, 1, size, f) == size;
}
static int lower(FILE *f, unsigned size, unsigned width, const char *key,
                 uint32_t *pos, uint32_t *count) {
  if (fseek(f, 0, SEEK_END))
    return -1;
  long length = ftell(f);
  if (length < 0 || length % size || strlen(key) >= width)
    return -1;
  *count = (uint32_t)(length / size);
  uint32_t lo = 0, hi = *count;
  unsigned char buf[135];
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (!record(f, mid, size, buf))
      return -1;
    buf[width] = 0;
    if (strcmp((char *)buf, key) < 0)
      lo = mid + 1;
    else
      hi = mid;
  }
  *pos = lo;
  return 0;
}
static bool utf8_valid(const unsigned char *s, size_t n) {
  for (size_t i = 0; i < n;) {
    unsigned char c = s[i++];
    if (c < 128)
      continue;
    unsigned extra = c >= 0xc2 && c <= 0xdf   ? 1
                     : c >= 0xe0 && c <= 0xef ? 2
                     : c >= 0xf0 && c <= 0xf4 ? 3
                                              : 99;
    if (extra == 99 || i + extra > n)
      return false;
    if ((c == 0xe0 && s[i] < 0xa0) || (c == 0xed && s[i] >= 0xa0) ||
        (c == 0xf0 && s[i] < 0x90) || (c == 0xf4 && s[i] >= 0x90))
      return false;
    while (extra--)
      if ((s[i++] & 0xc0) != 0x80)
        return false;
  }
  return true;
}
static bool field(char *dst, size_t capacity, const char *begin, size_t len) {
  if (len >= capacity)
    return false;
  if (!utf8_valid((const unsigned char *)begin, len))
    return false;
  memmove(dst, begin, len);
  dst[len] = 0;
  return true;
}
int dict_lookup(const char *base, bool chinese, const char *query,
                unsigned page, DictEntry *out, unsigned *total) {
  *total = 0;
  memset(out, 0, sizeof(*out));
  char key[105], path[512];
  size_t len = strlen(query);
  unsigned width = chinese ? 60 : 104, size = width + 6;
  if (!len)
    return 0;
  if (len >= width)
    return 0;
  for (size_t i = 0; i <= len; i++)
    key[i] = !chinese && (unsigned char)query[i] < 128
                 ? (char)ascii_lower((unsigned char)query[i])
                 : query[i];
  if (!path_join(path, sizeof(path), base, chinese ? "zh-en.idx" : "en-zh.idx"))
    return -1;
  FILE *idx = fopen(path, "rb");
  if (!idx)
    return -1;
  uint32_t start, count;
  int status = -1;
  if (lower(idx, size, width, key, &start, &count))
    goto done;
  unsigned char buf[134], selected[134];
  bool found = false;
  for (uint32_t i = start; i < count; i++) {
    if (!record(idx, i, size, buf))
      goto done;
    /* Key terminator may not overwrite the six-byte offset/length tail. */
    if (memcmp(buf, key, len) || buf[len] != 0)
      break;
    if ((*total)++ == page) {
      memcpy(selected, buf, size);
      found = true;
    }
  }
  status = 0;
  if (!found)
    goto done;
  uint32_t offset = u32(selected + width);
  unsigned length = u16(selected + width + 4);
  if (length < 2 || length >= sizeof(out->definition)) {
    status = -1;
    goto done;
  }
  if (!path_join(path, sizeof(path), base,
                 chinese ? "zh-en.dat" : "en-zh.dat")) {
    status = -1;
    goto done;
  }
  FILE *dat = fopen(path, "rb");
  if (!dat) {
    status = -1;
    goto done;
  }
  /* Reuse the caller's result storage; definitions are moved left after the
   * separate head/phonetic fields have been copied. No 16 KiB stack scratch. */
  char *raw = out->definition;
  bool ok = fseek(dat, 0, SEEK_END) == 0;
  long end = ok ? ftell(dat) : -1;
  ok = end >= 0 && offset <= (uint32_t)end &&
       length <= (uint32_t)end - offset &&
       fseek(dat, (long)offset, SEEK_SET) == 0 &&
       fread(raw, 1, length, dat) == length;
  fclose(dat);
  status = -1;
  if (!ok)
    goto done;
  char *a = memchr(raw, 0, length);
  if (!a)
    goto done;
  size_t left = length - (size_t)(a + 1 - raw);
  char *b = memchr(a + 1, 0, left);
  if (!b)
    goto done;
  if (!field(out->head, sizeof(out->head), raw, a - raw) ||
      !field(out->phonetic, sizeof(out->phonetic), a + 1, b - a - 1) ||
      !field(out->definition, sizeof(out->definition), b + 1,
             length - (size_t)(b + 1 - raw)))
    goto done;
  status = 1;
done:
  fclose(idx);
  return status;
}
int dict_candidates(const char *base, const char *pinyin, uint32_t *cursor,
                    DictCandidate out[5], bool *more) {
  *more = false;
  memset(out, 0, 5 * sizeof(*out));
  size_t len = strlen(pinyin);
  if (!len || len >= 68)
    return 0;
  for (size_t i = 0; i < len; i++)
    if (pinyin[i] < 'a' || pinyin[i] > 'z')
      return -1;
  char path[512];
  if (!path_join(path, sizeof(path), base, "zh-en.pinyin.idx"))
    return -1;
  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;
  uint32_t start, count;
  int n = 0;
  if (lower(f, 134, 68, pinyin, &start, &count)) {
    n = -1;
    goto done;
  }
  if (*cursor > start)
    start = *cursor;
  if (start > count) {
    n = -1;
    goto done;
  }
  unsigned char raw[134];
  for (uint32_t i = start; i < count; i++) {
    if (!record(f, i, 134, raw)) {
      n = -1;
      goto done;
    }
    if (memcmp(raw, pinyin, len))
      break;
    if (!memchr(raw + 68, 0, 60)) {
      n = -1;
      goto done;
    }
    bool duplicate = false;
    for (int j = 0; j < n; j++)
      if (!strcmp(out[j].head, (char *)raw + 68))
        duplicate = true;
    if (duplicate)
      continue;
    if (n == 5) {
      *cursor = i;
      *more = true;
      goto done;
    }
    size_t head_length = strlen((char *)raw + 68);
    if (!field(out[n].head, sizeof(out[n].head), (char *)raw + 68,
               head_length)) {
      n = -1;
      goto done;
    }
    n++;
  }
  *cursor = count;
done:
  fclose(f);
  return n;
}
bool dict_audio_name(const char *word, char *out, size_t size) {
  size_t n = strlen(word);
  if (!n || n + 6 > size)
    return false;
  char name[128];
  if (n >= sizeof(name))
    return false;
  for (size_t i = 0; i < n; i++) {
    unsigned c = (unsigned char)word[i];
    if (c >= 128)
      return false;
    c = ascii_lower(c);
    name[i] =
        ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) ? (char)c : '_';
  }
  name[n] = 0;
  bool reserved =
      !strcmp(name, "aux") || !strcmp(name, "con") || !strcmp(name, "nul") ||
      !strcmp(name, "prn") ||
      (n == 4 && (!strncmp(name, "com", 3) || !strncmp(name, "lpt", 3)) &&
       name[3] >= '1' && name[3] <= '9');
  return snprintf(out, size, "%s%s.wav", reserved ? "_" : "", name) > 0;
}
int dict_wav_open(const char *path, void **file, uint32_t *bytes) {
  *file = NULL;
  *bytes = 0;
  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;
  int result = -2;
  unsigned char h[16];
  bool fmt = false;
  if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) ||
      memcmp(h + 8, "WAVE", 4))
    goto fail;
  if (fseek(f, 0, SEEK_END))
    goto fail;
  long end = ftell(f);
  if (end < 12 || fseek(f, 12, SEEK_SET))
    goto fail;
  while (ftell(f) >= 0 && ftell(f) <= end - 8) {
    if (fread(h, 1, 8, f) != 8)
      goto fail;
    uint32_t n = u32(h + 4);
    long payload = ftell(f);
    if (n > (uint32_t)(end - payload))
      goto fail;
    if (!memcmp(h, "fmt ", 4)) {
      if (n < 16 || fread(h, 1, 16, f) != 16)
        goto fail;
      fmt = u16(h) == 1 && u16(h + 2) == 1 && u32(h + 4) == 48000 &&
            u16(h + 12) == 2 && u16(h + 14) == 16;
    } else if (!memcmp(h, "data", 4)) {
      if (!fmt) {
        result = -3;
        goto fail;
      }
      if (!n || n % 2)
        goto fail;
      *file = f;
      *bytes = n;
      return 0;
    }
    if ((uint64_t)payload + n + (n & 1) > (uint64_t)end ||
        fseek(f, payload + (long)n + (n & 1), SEEK_SET))
      goto fail;
  }
fail:
  fclose(f);
  return result;
}

int dict_book_read(const char *directory, bool favorites, char rows[20][192]) {
  char path[512], line[194];
  if (!path_join(path, sizeof(path), directory,
                 favorites ? "favorites.txt" : "history.txt"))
    return -1;
  FILE *f = fopen(path, "rb");
  if (!f)
    return errno == ENOENT ? 0 : -1;
  int count = 0;
  while (fgets(line, sizeof(line), f)) {
    size_t n = strlen(line);
    if (!n || line[n - 1] != '\n') {
      int c;
      while ((c = fgetc(f)) != EOF && c != '\n') {
      }
      continue;
    }
    line[--n] = 0;
    if (n && line[n - 1] == '\r')
      line[--n] = 0;
    if (!n || n >= 192)
      continue;
    if (count == 20) {
      memmove(rows, rows + 1, 19 * 192);
      count--;
    }
    strcpy(rows[count++], line);
  }
  bool error = ferror(f);
  fclose(f);
  return error ? -1 : count;
}
bool dict_book_save(const char *directory, bool favorites, const char *word) {
  if (!*word || strlen(word) >= 192 || strchr(word, '\n') || strchr(word, '\r'))
    return false;
  char dir[512];
  if (strlen(directory) >= sizeof(dir))
    return false;
  strcpy(dir, directory);
  for (char *p = dir + 1; *p; p++)
    if (*p == '/') {
      *p = 0;
      if (mkdir(dir, 0775) && errno != EEXIST)
        return false;
      *p = '/';
    }
  if (mkdir(dir, 0775) && errno != EEXIST)
    return false;
  char rows[20][192];
  int count = dict_book_read(directory, favorites, rows);
  if (count < 0)
    return false;
  char path[512], temp[520];
  if (!path_join(path, sizeof(path), directory,
                 favorites ? "favorites.txt" : "history.txt"))
    return false;
  snprintf(temp, sizeof(temp), "%s.tmp", path);
  FILE *f = fopen(temp, "wb");
  if (!f)
    return false;
  bool ok = true;
  if (favorites) {
    FILE *old = fopen(path, "rb");
    if (old) {
      char line[194];
      while (fgets(line, sizeof(line), old)) {
        size_t n = strlen(line);
        if (!n || line[n - 1] != '\n') {
          ok = false;
          break;
        }
        line[n - 1] = 0;
        if (strcmp(line, word) && fprintf(f, "%s\n", line) < 0)
          ok = false;
      }
      if (ferror(old))
        ok = false;
      fclose(old);
    }
  } else {
    /* ponytail: retain the latest 20 history items; use a paged journal for a
     * larger history. */
    for (int i = count == 20 ? 1 : 0; i < count; i++)
      if (strcmp(rows[i], word) && fprintf(f, "%s\n", rows[i]) < 0)
        ok = false;
  }
  if (fprintf(f, "%s\n", word) < 0)
    ok = false;
  if (fclose(f))
    ok = false;
  if (!ok)
    return false;
  return rename(temp, path) == 0;
}
