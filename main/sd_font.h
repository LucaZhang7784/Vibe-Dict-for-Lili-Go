#pragma once
#include <lvgl.h>
bool sd_font_open(const char *path);
const lv_font_t *sd_font_get(void);
void sd_font_close(void);
