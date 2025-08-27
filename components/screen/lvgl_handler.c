// #include "lvgl_handler.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/semphr.h"
// #include "esp_timer.h"
// #include "esp_log.h"
// #include "esp_heap_caps.h"
// #include "lvgl.h"

// // 关键：包含我们自己的底层驱动头文件
// #include "st7789v2_driver.h"

// static const char *TAG = "LVGL_HANDLER";

// // LVGL 核心组件
// static lv_display_t *disp;
// static lv_draw_buf_t draw_buf;
// static SemaphoreHandle_t lvgl_mutex = NULL;

// // 定义LVGL的绘图缓冲区大小
// #define LVGL_BUF_PIXELS (ST7789_LCD_H_RES * 40)

// // LVGL的 "flush_cb" 回调函数，这是连接LVGL和底层驱动的桥梁
// static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
// {
//     // 调用我们底层驱动的绘图函数
//     st7789v2_driver_draw_bitmap(area->x1, area->y1, area->x2, area->y2, px_map);
//     // 通知LVGL刷新完成
//     lv_display_flush_ready(display);
// }

// // LVGL的 "tick" 回调函数，为LVGL提供心跳
// static void lvgl_tick_cb(void *arg)
// {
//     lv_tick_inc(10); // 10ms
// }

// // LVGL的主任务，负责处理所有LVGL的事件和重绘
// static void lvgl_task(void *arg)
// {
//     ESP_LOGI(TAG, "Starting LVGL task");
//     while (1) {
//         xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
//         lv_timer_handler();
//         xSemaphoreGive(lvgl_mutex);
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }

// // ---- UI 创建部分 ----
// static void create_ui(void)
// {
//     lv_obj_t *scr = lv_scr_act(); // 获取当前屏幕

//     // 创建一个标签 (Label)
//     lv_obj_t *label = lv_label_create(scr);
//     lv_label_set_text(label, "Hello, ESP32-C6!");
//     lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);

//     // 将标签居中对齐
//     lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
// }


// // --- 公共初始化函数 ---
// void lvgl_handler_init(void)
// {
//     // 1. 初始化底层屏幕驱动
//     ESP_LOGI(TAG, "Initializing screen driver...");
//     st7789v2_driver_init();

//     // 2. 初始化LVGL库
//     ESP_LOGI(TAG, "Initializing LVGL...");
//     lv_init();

//     // 3. 为LVGL分配绘图缓冲区 (必须使用DMA内存)
//     void *buf1 = heap_caps_malloc(LVGL_BUF_PIXELS * sizeof(lv_color_t), MALLOC_CAP_DMA);
//     assert(buf1);

//     // 4. 初始化LVGL的绘图缓冲区
//     lv_draw_buf_init(&draw_buf, ST7789_LCD_H_RES, LVGL_BUF_PIXELS / ST7789_LCD_H_RES,
//                      LV_COLOR_FORMAT_RGB565, 0, buf1, LVGL_BUF_PIXELS * sizeof(lv_color_t));
    
//     // 5. 创建并配置LVGL显示设备
//     ESP_LOGI(TAG, "Creating LVGL display...");
//     disp = lv_display_create(ST7789_LCD_H_RES, ST7789_LCD_V_RES);
//     lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);
//     lv_display_set_flush_cb(disp, lvgl_flush_cb);
//     lv_display_set_draw_buffers(disp, &draw_buf, NULL);

//     // 6. 创建LVGL心跳定时器和主任务
//     const esp_timer_create_args_t lvgl_tick_timer_args = {
//         .callback = &lvgl_tick_cb, .name = "lvgl_tick"
//     };
//     esp_timer_handle_t lvgl_tick_timer = NULL;
//     ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
//     ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 10 * 1000)); // 10ms

//     lvgl_mutex = xSemaphoreCreateMutex();
//     xTaskCreate(lvgl_task, "LVGL", 4096, NULL, 6, NULL);

//     // 7. 创建UI
//     ESP_LOGI(TAG, "Creating UI...");
//     xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
//     create_ui();
//     xSemaphoreGive(lvgl_mutex);
// }







































