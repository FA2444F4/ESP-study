#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"

//通道8
#define BLINK_GPIO             8
//led灯数量
#define LED_STRIP_LED_NUMBERS  1
//RMT 分辨率 
#define LED_STRIP_RMT_RES_HZ   (10 * 1000 * 1000) // 10MHz
// 日志标签
static const char *TAG = "led";
//灯带句柄
led_strip_handle_t led_strip=NULL;

    
void taskLed(void* param){
    uint8_t brightness = 0;
    bool increasing = true; // true 表示亮度增加, false 表示亮度减少

    while (1) {
        // --- 1. 更新亮度值 ---
        if (increasing) {
            brightness++;
            if (brightness == 255) {
                increasing = false; // 达到最大亮度，方向反转
            }
        } else {
            brightness--;
            if (brightness == 0) {
                increasing = true;  // 达到最小亮度，方向反转
            }
        }

        // --- 2. 设置 LED 颜色 ---
        // 我们让蓝色通道的亮度变化，实现蓝色呼吸灯
        // led_strip_set_pixel(句柄, 灯珠索引, 红色, 绿色, 蓝色)
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0,brightness/30, brightness/30, brightness/30));

        // --- 3. 刷新灯带 ---
        // 将设置的颜色数据真正地发送给 LED
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        
        // --- 4. 控制呼吸速度 ---
        // 延时一小段时间来控制亮度变化的快慢
        vTaskDelay(pdMS_TO_TICKS(10)); // 延时 10 毫秒
    }
        
    
}

/* void app_main(void)
{
    ESP_LOGI(TAG, "初始化 LED strip...");

    // --- LED Strip 初始化步骤 ---

    // 1. 配置 LED 灯带
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,              // GPIO 引脚
        .max_leds = LED_STRIP_LED_NUMBERS,       // 灯珠数量
        .led_model = LED_MODEL_WS2812,             // LED 型号
        .color_component_format={
            .format.r_pos=0,//r位置
            .format.g_pos=1,//g位置
            .format.b_pos=2,//b位置
            .format.num_components=3//颜色通道数
        },
        .flags.invert_out = false,                 // 是否反转输出信号
    };

    // 2. 配置 RMT 后端
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,            // 时钟源,自动选择合适时钟源RMT_CLK_SRC_DEFAULT
        .resolution_hz = LED_STRIP_RMT_RES_HZ,   // RMT 分辨率,默认10MHz
        .mem_block_symbols=0,//一个 RMT 通道一次可以容纳多少个 RMT 符号。设置为 0 将回退到使用默认大小
        .flags.with_dma = false,                   // 是否使用 DMA
    };

    
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config//灯带配置
                                            , &rmt_config//rmt配置
                                            , &led_strip));//灯带句柄
    ESP_LOGI(TAG, "LED strip 初始化成功.");

    // 清空灯带（将所有灯设置为黑色/关闭）
    ESP_ERROR_CHECK(led_strip_clear(led_strip));

    // --- 呼吸灯循环 ---
    ESP_LOGI(TAG, "开始呼吸灯循环...");
    
    //分配任务
    xTaskCreatePinnedToCore(taskLed,"helloworld",2048,NULL,3,NULL,tskNO_AFFINITY);
} */