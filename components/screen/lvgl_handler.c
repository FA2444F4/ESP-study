#include "lvgl_handler.h"
#include "ssd1315_driver.h"
#include "lvgl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "LVGL_HANDLER";

#define LVGL_BUFFER_HEIGHT 16
#define LVGL_BUFFER_SIZE (SSD1315_WIDTH * LVGL_BUFFER_HEIGHT)

static void lv_tick_task(void *arg) {
    (void)arg;
    lv_tick_inc(10);
}

// ======================================================================
//             ↓↓↓ 这是本次修改的核心：创建坐标系校准图案 ↓↓↓
// ======================================================================
static void create_diagnostic_ui(void) {
    lv_obj_t *scr = lv_screen_active();
    // 将屏幕背景设置为黑色
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // --- 绘制一个白色的实心矩形在左上角 ---
    lv_obj_t *topLeftRect = lv_obj_create(scr);
    lv_obj_set_size(topLeftRect, 30, 16); // 尺寸 30x16
    lv_obj_align(topLeftRect, LV_ALIGN_TOP_LEFT, 5, 5); // 坐标 (5,5)
    lv_obj_set_style_bg_color(topLeftRect, lv_color_white(), 0);
    lv_obj_set_style_border_width(topLeftRect, 0, 0);

    // --- 绘制一条贯穿屏幕中间的水平线 ---
    static lv_point_precise_t line_points[] = { {0, 32}, {127, 32} };
    lv_obj_t *line_h = lv_line_create(scr);
    lv_line_set_points(line_h, line_points, 2);
    lv_obj_set_style_line_color(line_h, lv_color_white(), 0);

    // --- 绘制一条贯穿屏幕中间的垂直线 ---
    static lv_point_precise_t line_points_v[] = { {64, 0}, {64, 63} };
    lv_obj_t *line_v = lv_line_create(scr);
    lv_line_set_points(line_v, line_points_v, 2);
    lv_obj_set_style_line_color(line_v, lv_color_white(), 0);

    // --- 在右下角绘制文字 "OK" ---
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "OK");
    lv_obj_align(label, LV_ALIGN_BOTTOM_RIGHT, -10, -5);

    ESP_LOGI(TAG, "Diagnostic UI created.");
}


void lvgl_handler_init(void) {
    ssd1315_init();
    ESP_LOGI(TAG, "Hardware driver initialized.");

    lv_init();
    ESP_LOGI(TAG, "LVGL core initialized.");

    lv_display_t *disp = lv_display_create(SSD1315_WIDTH, SSD1315_HEIGHT);
    ESP_LOGI(TAG, "LVGL display created.");
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_I1);
    
    void *buf1 = heap_caps_malloc(LVGL_BUFFER_SIZE, MALLOC_CAP_DMA);
    assert(buf1 != NULL);
    lv_display_set_buffers(disp, buf1, NULL, LVGL_BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    lv_display_set_flush_cb(disp, ssd1315_flush);

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000));
    ESP_LOGI(TAG, "LVGL tick timer started.");

    // 调用我们新的诊断UI创建函数
    create_diagnostic_ui();
}