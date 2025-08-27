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

static const char *TAG = "LVGL_MANUAL";

// Pin definition (与之前的接线保持一致)
#define LCD_HOST        SPI2_HOST
#define PIN_NUM_SCLK    6
#define PIN_NUM_MOSI    7
#define PIN_NUM_RST     3
#define PIN_NUM_DC      2
#define PIN_NUM_CS      10
#define PIN_NUM_BCKL    4

// Screen Resolution
#define LCD_H_RES       240
#define LCD_V_RES       280

// LVGL Buffer settings
#define LVGL_BUF_SIZE         (LCD_H_RES * 40) // 缓冲区可以设置成屏幕高度的 1/10 到 1/2
#define LVGL_TICK_PERIOD_MS   10

// 全局变量
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_display_t *disp;
static SemaphoreHandle_t lvgl_mutex = NULL;

// 定义一个标准的事件回调函数
static void button_event_handler(lv_event_t * e)
{
    // 获取事件代码
    lv_event_code_t code = lv_event_get_code(e);
    // 从用户数据中获取之前传递的 info_label 对象
    lv_obj_t *info_label = (lv_obj_t *)lv_event_get_user_data(e);

    // 判断事件类型是否为“点击”
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI("LVGL_DEMO", "Button Clicked");
        // 更新标签的文本
        lv_label_set_text(info_label, "Button has been clicked!");
    }
}

// LVGL UI 创建函数 (与之前相同)
static void example_lvgl_ui(void) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -40);
    lv_obj_t *label_on_btn = lv_label_create(btn);
    lv_label_set_text(label_on_btn, "Click Me!");
    lv_obj_center(label_on_btn);
    lv_obj_t *info_label = lv_label_create(scr);
    lv_label_set_text(info_label, "Button waiting...");
    lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_event_cb(btn, button_event_handler, LV_EVENT_ALL, info_label);
}


/* 关键步骤 1: 创建 flush_cb 回调函数 */
// LVGL需要刷新屏幕时会调用此函数
static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(display);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // 将LVGL的像素数据通过驱动的 draw_bitmap 函数写入屏幕
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
    // 通知LVGL刷新完成
    lv_display_flush_ready(display);
}

/* 关键步骤 2: 创建 LVGL 的系统心跳 */
static void lvgl_tick_cb(void *arg)
{
    // 为LVGL提供心跳
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

/* 关键步骤 3: 创建 LVGL 主循环任务 */
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    while (1) {
        // 加锁，保护LVGL操作
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
        // 调用LVGL的任务处理器，它会处理所有LVGL相关的事件和重绘
        lv_timer_handler();
        // 解锁
        xSemaphoreGive(lvgl_mutex);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void st7789v2_driver_init(void)
{
    // --- 1. 初始化背光引脚 ---
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BCKL
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(PIN_NUM_BCKL, 1); // 点亮背光

    // --- 2. 初始化 SPI 总线 ---
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1, // MISO不使用
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LVGL_BUF_SIZE * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // --- 3. 创建 LCD Panel IO Handle ---
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_CS,
        .dc_gpio_num = PIN_NUM_DC,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000, // SPI 时钟频率, 40MHz
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,
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
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true); // 根据屏幕型号可能需要反色
    // 设置偏移量
    esp_lcd_panel_set_gap(panel_handle, 0, 20);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    // --- 6. 初始化 LVGL 核心库 ---
    ESP_LOGI(TAG, "Initialize LVGL");
    lv_init();

    // --- 7. 创建并配置 LVGL 显示对象 ---
    disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    
    // 分配绘图缓冲区
    // LVGL需要缓冲区来渲染内部图形, 然后通过 flush_cb 发送到屏幕
    void *buf1 = heap_caps_malloc(LVGL_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);

    // 声明一个 lv_draw_buf_t 结构体来管理缓冲区
    static lv_draw_buf_t draw_buf;
   // 正确的新代码
    lv_draw_buf_init(
        &draw_buf,                          // 要初始化的结构体指针
        LCD_H_RES,                          // 缓冲区的宽度 (等于屏幕宽度)
        LVGL_BUF_SIZE / LCD_H_RES,          // 缓冲区的高度 (总像素数 / 宽度)
        LV_COLOR_FORMAT_RGB565,             // 颜色格式
        0,                                  // 步长，设为0由LVGL自动计算
        buf1,                               // 指向实际内存的指针
        LVGL_BUF_SIZE * sizeof(lv_color_t)  // 内存的总大小 (字节单位)
    );

    // **将LVGL和硬件驱动链接起来**

    // 步骤 A: 设置渲染模式
    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);
    // 步骤 B: 设置绘图缓冲区 (使用已初始化的 draw_buf 结构体)
    lv_display_set_draw_buffers(disp, &draw_buf, NULL);
    // **将LVGL和硬件驱动链接起来**
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_user_data(disp, panel_handle); // 将panel_handle传递给flush_cb

    // --- 8. 启动 LVGL 的心跳定时器和主任务 ---
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    // 创建互斥锁和LVGL任务
    lvgl_mutex = xSemaphoreCreateMutex();
    xTaskCreate(lvgl_task, "LVGL", 4096, NULL, 6, NULL);

    // --- 9. 创建UI界面 ---
    ESP_LOGI(TAG, "Display LVGL UI");
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    example_lvgl_ui();
    xSemaphoreGive(lvgl_mutex);
}

