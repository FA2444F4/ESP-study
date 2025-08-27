#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"//(Vendor/厂商): 它包含了特定厂商芯片的驱动实现
#include "esp_lcd_panel_ops.h"//定义了一套通用的屏幕操作接口
#include "esp_heap_caps.h"//分配 DMA 内存,允许 SPI 硬件直接从内存中读取数据发送到屏幕
#include "driver/spi_master.h"//控制 ESP32 芯片上的 SPI 主机外设
#include "driver/gpio.h"
#include "esp_log.h"
#include "string.h"
#include "st7789v2_driver.h"

static const char *TAG = "ST7789_DRIVER";

#define LCD_HOST        SPI2_HOST
#define PIN_NUM_SCLK    6
#define PIN_NUM_MOSI    7
#define PIN_NUM_RST     3
#define PIN_NUM_DC      2
#define PIN_NUM_CS      10
#define PIN_NUM_BCKL    4

static esp_lcd_panel_handle_t panel_handle = NULL;

esp_err_t st7789v2_driver_init(void) {
    //背光配置
    gpio_config_t bk_gpio_config = { 
        .mode = GPIO_MODE_OUTPUT, 
        .pin_bit_mask = 1ULL << PIN_NUM_BCKL 
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    //配置SPI总线
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7789_LCD_H_RES * ST7789_LCD_V_RES * sizeof(uint16_t),
        //.max_transfer_sz设置了单次 SPI 传输的最大数据量（字节）
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    //创建Panel IO (将 SPI 总线与屏幕控制逻辑绑定,DC,CS)
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_CS,
        .dc_gpio_num = PIN_NUM_DC,
        .spi_mode = 0,// SPI 的时钟极性和相位
        //空闲时钟为低电平，称为时钟极性 CPOL = 0
        //在时钟的第一个跳变沿（即从低到高的上升沿）采样数据，称为时钟相位 CPHA = 0。
        //CPOL=0 和 CPHA=0 的组合，就是 SPI Mode 0
        .pclk_hz = 40 * 1000 * 1000,// SPI 时钟的具体速度
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,//驱动命令8位
        .lcd_param_bits = 8,//参数8位
    };
    //创建io_handle,设置打包命令和数据,自动控制DC,CS信号
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    //安装ST7789驱动
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(gpio_set_level(PIN_NUM_BCKL, 0));//关闭背光
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));//复位
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));//初始化命令序列（设置工作模式、电压、帧率等）。
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));//开启显示
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));//颜色反转,修改颜色
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));//竖屏模式变横屏模式
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));//镜像
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 20, 0));//设置偏移
    ESP_ERROR_CHECK(gpio_set_level(PIN_NUM_BCKL, 1));//开启背光

    ESP_LOGI(TAG, "ST7789 driver initialized successfully");
    return ESP_OK;
}

void st7789v2_driver_draw_bitmap(int x1, int y1, int x2, int y2, const void *color_map) {
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, color_map);
}

void st7789v2_driver_fill_with_rect_test(void)
{
    // --- 新增：清空整个屏幕为黑色 ---
    ESP_LOGI(TAG, "Clearing screen to black...");
    size_t line_buffer_size = ST7789_LCD_H_RES * sizeof(uint16_t);
    uint16_t *line_buffer = (uint16_t *)heap_caps_malloc(line_buffer_size, MALLOC_CAP_DMA);
    if (line_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for clear buffer");
        return;
    }
    // 将行缓冲区填满黑色 (0x0000 字节交换后还是 0x0000)
    memset(line_buffer, 0, line_buffer_size);
    // 逐行写入黑色
    for (int y = 0; y < ST7789_LCD_V_RES; y++) {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, ST7789_LCD_H_RES, y + 1, line_buffer));
    }
    free(line_buffer);


    // --- 开始绘制红色矩形 (代码保持不变) ---
    ESP_LOGI(TAG, "Drawing red rectangle...");
    int rect_w = 100;
    int rect_h = 50;
    int start_x = (ST7789_LCD_H_RES - rect_w) / 2;
    int start_y = (ST7789_LCD_V_RES - rect_h) / 2;
    int end_x = start_x + rect_w;
    int end_y = start_y + rect_h;
    uint16_t color_red_rgb565 = 0xF800;
    uint16_t color_red_swapped = (color_red_rgb565 << 8) | (color_red_rgb565 >> 8);

    size_t rect_buffer_size = rect_w * rect_h * sizeof(uint16_t);
    uint16_t *rect_buffer = (uint16_t *)heap_caps_malloc(rect_buffer_size, MALLOC_CAP_DMA);
    if (rect_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for rectangle buffer");
        return;
    }
    for (int i = 0; i < rect_w * rect_h; i++) {
        rect_buffer[i] = color_red_swapped;
    }
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, start_x, start_y, end_x, end_y, rect_buffer));
    free(rect_buffer);
}