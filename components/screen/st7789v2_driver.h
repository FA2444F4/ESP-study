#ifndef ST7789V2_DRIVER_H
#define ST7789V2_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

// 屏幕分辨率
#define ST7789_LCD_H_RES   240
#define ST7789_LCD_V_RES   280

/**
 * @brief 初始化ST7789V2屏幕驱动
 * * @return esp_err_t ESP_OK 表示成功, 其他表示失败
 */
esp_err_t st7789v2_driver_init(void);

/**
 * @brief 使用指定颜色填充整个屏幕
 * * @param color 16位的RGB565颜色值
 */
void st7789v2_driver_fill(uint16_t color);

#endif // ST7789V2_DRIVER_H