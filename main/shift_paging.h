#pragma once
#include <stdint.h>

/* LilyGO C3 raw matrix: columns 1/2, rows 6/3 are left/right Shift.
 * Only rising edges page: holding a key must not skip candidate groups. */
static inline int shift_page_edge(const uint8_t matrix[5], uint8_t *previous) {
  for (unsigned i = 0; i < 5; i++)
    if (matrix[i] & 0x80)
      return 0;
  uint8_t held = ((matrix[1] >> 6) & 1) | (((matrix[2] >> 3) & 1) << 1);
  uint8_t pressed = held & ~*previous;
  *previous = held;
  if (held == 3 || matrix[0] || (matrix[1] & ~0x40) || (matrix[2] & ~0x08) ||
      matrix[3] || matrix[4])
    return 0;
  return pressed == 1 ? -1 : pressed == 2 ? 1 : 0;
}
