#ifndef ST7789V2_DRIVER_H
#define ST7789V2_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

// 屏幕分辨率 (横屏模式)
#define ST7789_LCD_H_RES   320
#define ST7789_LCD_V_RES   240

/**
 * @brief 初始化ST7789V2屏幕驱动
 */
esp_err_t st7789v2_driver_init(void);

/**
 * @brief 将指定区域的颜色数据写入屏幕 (LVGL调用的核心函数)
 *
 * @param x1 起始X坐标
 * @param y1 起始Y坐标
 * @param x2 结束X坐标
 * @param y2 结束Y坐标
 * @param color_map 指向颜色数据缓冲区的指针
 */
void st7789v2_driver_draw_bitmap(int x1, int y1, int x2, int y2, const void *color_map);


void st7789v2_driver_fill_with_rect_test(void);
#endif // ST7789V2_DRIVER_H