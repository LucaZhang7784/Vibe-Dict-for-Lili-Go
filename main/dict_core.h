#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  char head[128], phonetic[256], definition[16384];
} DictEntry;
typedef struct {
  char head[64];
} DictCandidate;
/* Returns 1 on hit, 0 on miss, -1 on bad/missing data. Output is never
 * truncated. */
int dict_lookup(const char *base, bool chinese, const char *query,
                unsigned page, DictEntry *out, unsigned *total);
int dict_candidates(const char *base, const char *pinyin, uint32_t *cursor,
                    DictCandidate out[5], bool *more);
bool dict_audio_name(const char *word, char *out, size_t size);
void dict_stereo_frames(const int16_t *mono, size_t frames, int16_t *stereo);
int dict_scroll_target(int current, int maximum, int viewport, int direction,
                       bool page);
void dict_clean_query(char *query);
/* Locate validated PCM payload; caller streams exactly *bytes from this
 * position. */
int dict_wav_open(const char *path, void **file, uint32_t *bytes);
int dict_book_read(const char *directory, bool favorites, char rows[20][192]);
bool dict_book_save(const char *directory, bool favorites, const char *word);
