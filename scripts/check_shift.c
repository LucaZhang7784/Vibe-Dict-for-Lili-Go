#include "shift_paging.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  uint8_t matrix[5] = {0}, previous = 0;
  assert(shift_page_edge(matrix, &previous) == 0);
  matrix[1] = 0x40;
  assert(shift_page_edge(matrix, &previous) == -1);
  for (unsigned i = 0; i < 20; i++)
    assert(shift_page_edge(matrix, &previous) == 0);
  matrix[1] = 0;
  assert(shift_page_edge(matrix, &previous) == 0);
  matrix[2] = 0x08;
  assert(shift_page_edge(matrix, &previous) == 1);
  assert(shift_page_edge(matrix, &previous) == 0);
  matrix[1] = 0x40;
  assert(shift_page_edge(matrix, &previous) == 0);
  for (unsigned col = 0; col < 5; col++) {
    for (unsigned row = 0; row < 7; row++) {
      memset(matrix, 0, sizeof(matrix));
      previous = 0;
      matrix[col] = 1 << row;
      int expected = col == 1 && row == 6 ? -1 : col == 2 && row == 3 ? 1 : 0;
      assert(shift_page_edge(matrix, &previous) == expected);
    }
  }
  memset(matrix, 0, sizeof(matrix));
  previous = 0;
  matrix[1] = 0x40;
  matrix[3] = 1;
  assert(shift_page_edge(matrix, &previous) == 0);
  matrix[3] = 0;
  assert(shift_page_edge(matrix, &previous) == 0);
  matrix[4] = 0xff;
  previous = 0;
  assert(shift_page_edge(matrix, &previous) == 0 && previous == 0);
  puts("Shift paging: matrix positions, press/hold/release, chords and invalid "
       "frames passed");
}
