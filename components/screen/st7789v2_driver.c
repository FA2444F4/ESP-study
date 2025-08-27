#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "lvgl.h"

#include "hal/lcd_types.h"
#include "esp_lcd_types.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#include "esp_heap_caps.h"



static const char *TAG = "ST7789_DRIVER";

// 引脚定义
#define LCD_HOST        SPI2_HOST
#define PIN_NUM_SCLK    6
#define PIN_NUM_MOSI    7
#define PIN_NUM_RST     3
#define PIN_NUM_DC      2
#define PIN_NUM_CS      10
#define PIN_NUM_BCKL    4

// 全局静态变量，保存panel handle
static esp_lcd_panel_handle_t panel_handle = NULL;

esp_err_t st7789v2_driver_init(void) {
    // --- 1. 初始化背光引脚 ---
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BCKL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(PIN_NUM_BCKL, 1); // 点亮背光

    // --- 2. 初始化 SPI 总线 ---
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7789_LCD_H_RES * ST7789_LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // --- 3. 创建 LCD Panel IO Handle ---
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_CS,
        .dc_gpio_num = PIN_NUM_DC,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // --- 4. 创建 ST7789 LCD 控制器驱动 ---
    ESP_LOGI(TAG, "Install ST7789 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    // --- 5. 初始化并开启屏幕 ---
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 20)); // Y轴偏移
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "ST7789 driver initialized successfully");
    return ESP_OK;
}

void st7789v2_driver_fill(uint16_t color) {
    // 为了节省内存，我们分配一行大小的缓冲区
    size_t buffer_size = ST7789_LCD_H_RES * sizeof(uint16_t);
    uint16_t *buffer = (uint16_t *)malloc(buffer_size);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for fill buffer");
        return;
    }

    // 将缓冲区填满指定的颜色
    for (int i = 0; i < ST7789_LCD_H_RES; i++) {
        buffer[i] = color;
    }

    // 逐行写入屏幕
    for (int y = 0; y < ST7789_LCD_V_RES; y++) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, ST7789_LCD_H_RES, y + 1, buffer);
    }

    free(buffer);
}
