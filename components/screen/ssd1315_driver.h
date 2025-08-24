#pragma once

#include "lvgl.h" // 因为 flush 函数的参数需要 lvgl 的类型

// 公开显示屏的尺寸，这样其他文件就可以使用
#define SSD1315_WIDTH  128
#define SSD1315_HEIGHT 64

// 公开函数声明
void ssd1315_init(void);
void ssd1315_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void ssd1315_draw_test_pattern(void);