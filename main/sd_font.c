#include "sd_font.h"
#include <esp_heap_caps.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  uint32_t cp, offset, advance;
} Glyph;
static FILE *file;
static Glyph *index_table;
static uint32_t count;
static lv_font_t font;
typedef struct {
  uint32_t gid;
  uint8_t pixels[480];
} CachedGlyph;
static CachedGlyph *cache;
static bool descriptor(const lv_font_t *f, lv_font_glyph_dsc_t *d, uint32_t cp,
                       uint32_t next) {
  (void)f;
  (void)next;
  if (cp == '\n' || cp == '\r')
    return false;
  uint32_t lo = 0, hi = count;
  while (lo < hi) {
    uint32_t m = lo + (hi - lo) / 2;
    if (index_table[m].cp < cp)
      lo = m + 1;
    else
      hi = m;
  }
  if (lo >= count || index_table[lo].cp != cp) {
    /* Explicit missing-glyph box, never silently remove a character. */
    memset(d, 0, sizeof(*d));
    d->adv_w = 20;
    d->box_w = 20;
    d->box_h = 24;
    d->gid.index = UINT32_MAX;
  } else {
    memset(d, 0, sizeof(*d));
    d->adv_w = index_table[lo].advance;
    d->box_w = 20;
    d->box_h = 24;
    d->gid.index = lo;
  }
  d->ofs_y = -4;
  d->format = LV_FONT_GLYPH_FORMAT_A8;
  d->stride = 20;
  return true;
}
static const void *bitmap(lv_font_glyph_dsc_t *d, lv_draw_buf_t *draw) {
  unsigned slot = d->gid.index % 64;
  if (cache[slot].gid != d->gid.index) {
    uint8_t *pixels = cache[slot].pixels;
    memset(pixels, 0, 480);
    if (d->gid.index < count) {
      if (fseek(file, index_table[d->gid.index].offset, SEEK_SET) ||
          fread(pixels, 1, 480, file) != 480) {
        for (unsigned y = 3; y < 21; y++)
          for (unsigned x = 2; x < 18; x++)
            if (x == 2 || x == 17 || y == 3 || y == 20)
              pixels[y * 20 + x] = 255;
      }
    } else
      for (unsigned y = 3; y < 21; y++)
        for (unsigned x = 2; x < 18; x++)
          if (x == 2 || x == 17 || y == 3 || y == 20)
            pixels[y * 20 + x] = 255;
    cache[slot].gid = d->gid.index;
  }
  if (d->req_raw_bitmap)
    return cache[slot].pixels;
  if (!draw)
    return NULL;
  for (unsigned y = 0; y < 24; y++)
    memcpy(draw->data + y * draw->header.stride, cache[slot].pixels + y * 20,
           20);
  return draw;
}
bool sd_font_open(const char *path) {
  sd_font_close();
  file = fopen(path, "rb");
  if (!file)
    return false;
  uint32_t header[4];
  if (fread(header, 1, 16, file) != 16 || memcmp(header, "VDF1", 4) ||
      !header[1] || header[1] > 100000 || header[2] != 20 || header[3] != 24)
    goto fail;
  count = header[1];
  index_table = heap_caps_malloc(count * sizeof(Glyph),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!index_table)
    index_table = malloc(count * sizeof(Glyph));
  if (!index_table || fread(index_table, sizeof(Glyph), count, file) != count ||
      fseek(file, 0, SEEK_END))
    goto fail;
  long end = ftell(file);
  for (uint32_t i = 0; i < count; i++)
    if ((i && index_table[i].cp <= index_table[i - 1].cp) ||
        index_table[i].advance > 20 ||
        index_table[i].offset < 16 + count * sizeof(Glyph) || end < 480 ||
        index_table[i].offset > (uint32_t)(end - 480))
      goto fail;
  cache =
      heap_caps_calloc(64, sizeof(*cache), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!cache)
    cache = calloc(64, sizeof(*cache));
  if (!cache)
    goto fail;
  for (unsigned i = 0; i < 64; i++)
    cache[i].gid = UINT32_MAX - 1;
  memset(&font, 0, sizeof(font));
  font.line_height = 24;
  font.base_line = 4;
  font.get_glyph_dsc = descriptor;
  font.get_glyph_bitmap = bitmap;
  return true;
fail:
  sd_font_close();
  return false;
}
const lv_font_t *sd_font_get(void) {
  return file ? &font : lv_font_get_default();
}
void sd_font_close(void) {
  if (file)
    fclose(file);
  file = NULL;
  free(index_table);
  index_table = NULL;
  free(cache);
  cache = NULL;
  count = 0;
}
