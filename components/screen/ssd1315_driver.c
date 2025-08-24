#include "ssd1315_driver.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_log.h"

#define OLED_HOST   SPI2_HOST
#define PIN_NUM_MOSI 21
#define PIN_NUM_SCLK 19
#define PIN_NUM_CS   18
#define PIN_NUM_DC   11
#define PIN_NUM_RST  10

static spi_device_handle_t spi;
static const char *TAG_DRIVER = "Ultimate_Final_Driver";

// 底层函数：发送命令
static void disp_spi_send_cmd(uint8_t cmd) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    gpio_set_level(PIN_NUM_DC, 0);
    spi_device_polling_transmit(spi, &t);
}

// 底层函数：发送数据
static void disp_spi_send_data(const uint8_t *data, size_t len) {
    if (len == 0) return;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    gpio_set_level(PIN_NUM_DC, 1);
    spi_device_polling_transmit(spi, &t);
}

void ssd1315_init(void) {
    // GPIO 和 SPI 初始化
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI, .sclk_io_num = PIN_NUM_SCLK, .miso_io_num = -1,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 0,
    };
    spi_bus_initialize(OLED_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000, .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
    };
    spi_bus_add_device(OLED_HOST, &devcfg, &spi);

    // 硬件复位
    gpio_set_level(PIN_NUM_RST, 0); vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RST, 1); vTaskDelay(pdMS_TO_TICKS(100));

    // --- 采用与成功自检程序完全匹配的初始化序列 ---
    const uint8_t init_cmds[] = {
        0xAE,       // Display OFF
        0xD5, 0x80, // Set Clock Divide Ratio
        0xA8, 0x3F, // Set Multiplex Ratio
        0xD3, 0x00, // Set Display Offset
        0x40,       // Set Display Start Line
        0x8D, 0x14, // Charge Pump Setting
        // **关键: 确立水平寻址模式 (Horizontal Addressing Mode)**
        // **这与您成功的自检程序完全一致**
        0x20, 0x00, 
        0xA1,       // Set Segment Re-map to REVERSED (X-axis mirror)
        0xC8,       // Set COM Output Scan Direction to REVERSED (Y-axis mirror)
        0xDA, 0x12, // Set COM Pins Hardware Configuration
        0x81, 0xFF, // Set Contrast (0xFF for max brightness)
        0xD9, 0xF1, // Set Pre-charge Period
        0xDB, 0x40, // Set VCOMH Deselect Level
        0xA4,       // Set Entire Display ON to follow RAM content
        0xA6,       // Set Normal Display (not inverted)
        0xAF,       // Display ON
    };
    for (int i = 0; i < sizeof(init_cmds); i++) {
        disp_spi_send_cmd(init_cmds[i]);
    }
    ESP_LOGI(TAG_DRIVER, "Driver initialized with Horizontal Mode & Reversed Scan.");
}


// ======================================================================
//             ↓↓↓ 这是模仿成功自检逻辑的最终 flush 函数 ↓↓↓
// ======================================================================
void ssd1315_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    uint32_t len = w * h / 8; // LVGL 在1位单色模式下，缓冲区总字节数

    // 1. 设置列地址范围 (Column address)
    disp_spi_send_cmd(0x21);
    disp_spi_send_cmd(area->x1);
    disp_spi_send_cmd(area->x2);

    // 2. 设置页地址范围 (Page address)
    disp_spi_send_cmd(0x22);
    disp_spi_send_cmd(area->y1 / 8);
    disp_spi_send_cmd(area->y2 / 8);

    // 3. 将整个区域的像素数据一次性发送出去
    // 这与您成功的自检程序逻辑完全相同
    disp_spi_send_data(px_map, len);

    // 4. 通知 LVGL 刷新已完成
    lv_display_flush_ready(disp);
}