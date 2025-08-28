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





































#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "lvgl.h"
#include "st7789v2_driver.h" // 引入您的底层驱动头文件
#include "lvgl_handler.h"

static const char *TAG = "LVGL_HANDLER";

// LVGL 任务和互斥锁
static TaskHandle_t lvgl_task_handle = NULL;
static SemaphoreHandle_t lvgl_mutex = NULL;

// LVGL 显示对象和绘图缓冲区
static lv_display_t *display;//显示对象,物理显示设备
#define LVGL_DRAW_BUF_SIZE (ST7789_LCD_H_RES * 20) // 缓冲区大小，建议为屏幕宽度的10-40倍
static lv_color_t *draw_buf1 = NULL;//双缓冲交替渲染
static lv_color_t *draw_buf2 = NULL;


/* LVGL Flush Callback: 将LVGL的绘图缓冲区内容发送到屏幕 */
static void lvgl_flush_cb(
    lv_display_t *disp//显示对象
    , const lv_area_t *area//要刷新到屏幕上的矩形区域的坐标 (x1, y1, x2, y2)。
    , uint8_t *px_map)//渲染好的颜色数据缓冲区
{

    // --- 1. 计算需要刷新的像素总数 ---
    // lv_area_get_size 是一个便捷函数，用于计算 area 区域内的像素总数
    uint32_t area_size_px = lv_area_get_size(area);

    // --- 2. (新增) 执行字节序交换 ---
    // 在将数据发送给驱动之前，对整个缓冲区进行高低字节交换
    // 这个函数会直接修改 px_map 指向的内存区域
    lv_draw_sw_rgb565_swap(px_map, area_size_px);

    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2 + 1;
    int32_t y2 = area->y2 + 1;//LVGL 的坐标体系是闭区间的
    st7789v2_driver_draw_bitmap(x1, y1, x2, y2, px_map);

    // 重要！通知LVGL刷新已完成
    lv_display_flush_ready(disp);//解除占用该缓冲区
}

/* LVGL Tick Callback: 为LVGL提供系统节拍 */
static uint32_t tick_get_cb(void)
{
    // ESP-IDF 推荐的方式
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* LVGL 主处理任务 */
static void lvgl_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    while (1) {
        // 尝试获取互斥锁，等待时间为 portMAX_DELAY (永久)
        if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
            // 调用 LVGL 的主处理函数
            lv_timer_handler();//这个函数负责处理所有与时间相关的任务，包括重绘屏幕、执行动画、处理输入设备事件等。必须周期性地调用它
            // 释放互斥锁
            xSemaphoreGive(lvgl_mutex);
        }
        // 任务延时
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* 创建一个简单的 LVGL UI 示例 */
void lvgl_handler_create_ui(void)
{
    // 获取当前活动的屏幕
    lv_obj_t *scr = lv_screen_active();

    // 创建一个标签控件
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hello from LVGLHA");

    // 将标签居中显示
    lv_obj_center(label);
}

void lv_example_get_started_1(void)
{
    lv_obj_t *scr = lv_screen_active();
    
    // 把背景色设置放在最前面
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003a57), LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hello Font Test");
    lv_obj_center(label);

    // --- 开始诊断和修复代码 ---
    
    // 1. 创建一个全新的、独立的样式
    static lv_style_t style_test_font;
    lv_style_init(&style_test_font);

    // 2. 在这个样式中，设置它需要的所有属性
    
    // **第1步：设置正确的字体**
    lv_style_set_text_font(&style_test_font, &lv_font_montserrat_14);
    
    // **第2步（关键修复）：也为它设置一个文本颜色**
    // 这样它就不用去继承父控件的颜色了
    lv_style_set_text_color(&style_test_font, lv_color_white()); // 或者 lv_color_hex(0xFFFFFF)
    
    // 3. 将这个“完整”的样式应用到我们的标签上
    lv_obj_add_style(label, &style_test_font, 0);

    ESP_LOGI("FONT_TEST", "Explicitly setting font AND color to the label.");
    
    // --- 结束诊断和修复代码 ---
}


#define CANVAS_WIDTH  150
#define CANVAS_HEIGHT 150
void lv_example_canvas_9(void)
{
    /*Create a buffer for the canvas*/
    LV_DRAW_BUF_DEFINE_STATIC(draw_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_ARGB8888);
    LV_DRAW_BUF_INIT_STATIC(draw_buf);

    /*Create a canvas and initialize its palette*/
    lv_obj_t * canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_draw_buf(canvas, &draw_buf);
    lv_canvas_fill_bg(canvas, lv_color_hex3(0xccc), LV_OPA_COVER);
    lv_obj_center(canvas);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_triangle_dsc_t tri_dsc;
    lv_draw_triangle_dsc_init(&tri_dsc);
    tri_dsc.p[0].x = 10;
    tri_dsc.p[0].y = 10;
    tri_dsc.p[1].x = 100;
    tri_dsc.p[1].y = 30;
    tri_dsc.p[2].x = 50;
    tri_dsc.p[2].y = 100;

    tri_dsc.grad.stops_count = 2;
    tri_dsc.grad.dir = LV_GRAD_DIR_VER;
    tri_dsc.grad.stops[0].color = lv_color_hex(0xff0000);
    tri_dsc.grad.stops[0].frac = 64;    /*Start at 25%*/
    tri_dsc.grad.stops[0].opa = LV_OPA_COVER;
    tri_dsc.grad.stops[1].color = lv_color_hex(0x0000ff);
    tri_dsc.grad.stops[1].opa = LV_OPA_TRANSP;
    tri_dsc.grad.stops[1].frac = 3 * 64;    /*End at 75%*/

    tri_dsc.opa = 128;  /*Set the overall opacity to 50%*/

    lv_draw_triangle(&layer, &tri_dsc);

    lv_canvas_finish_layer(canvas, &layer);
}




/* LVGL 初始化函数 */
void lvgl_handler_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL...");

    // 1. 调用底层驱动初始化屏幕
    ESP_ERROR_CHECK(st7789v2_driver_init());

    // 2. 初始化 LVGL 库
    lv_init();

    // 3. 创建互斥锁，用于多线程保护
    lvgl_mutex = xSemaphoreCreateMutex();
    if (lvgl_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL mutex");
        return;
    }

    // 4. 设置 Tick 回调函数
    lv_tick_set_cb(tick_get_cb);

    // 5. 创建 LVGL 显示对象
    display = lv_display_create(ST7789_LCD_H_RES, ST7789_LCD_V_RES);
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        return;
    }

    // 6. 分配绘图缓冲区 (使用 DMA-capable 内存)
    draw_buf1 = heap_caps_malloc(LVGL_DRAW_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    draw_buf2 = heap_caps_malloc(LVGL_DRAW_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (draw_buf1 == NULL || draw_buf2 == NULL) {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffers");
        // 清理已分配的资源
        if(draw_buf1) free(draw_buf1);
        if(draw_buf2) free(draw_buf2);
        return;
    }

    // 7. 关联缓冲区和显示对象
    lv_display_set_buffers(display, draw_buf1, draw_buf2, LVGL_DRAW_BUF_SIZE * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // **重要**: LVGL v9.4 需要明确设置颜色格式
    // ST7789 通常是 RGB565。esp_lcd 组件会自动处理字节序问题。
    // 如果颜色显示不正确，可以尝试 LV_COLOR_FORMAT_RGB565_SWAP。
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    // 8. 设置 Flush 回调函数
    lv_display_set_flush_cb(display, lvgl_flush_cb);

    // 9. 创建并启动 LVGL 主任务，固定在核心1上运行以获得更好的性能
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 4096, NULL, 5, &lvgl_task_handle, 0);

    ESP_LOGI(TAG, "LVGL initialized successfully.");

    // 10. 创建您的 UI
    // 需要在获取互斥锁后调用，以确保线程安全
    if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
        // lvgl_handler_create_ui();
        // lv_example_get_started_1();
        // lv_example_canvas_7();
        lv_example_canvas_9();
        xSemaphoreGive(lvgl_mutex);
    }
}

