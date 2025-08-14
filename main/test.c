#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

// 为了方便修改，我们将 LED 连接的 GPIO 引脚定义为一个宏
#define BLINK_GPIO  4

// 定义一个日志标签
static const char *TAG = "BLINK";

/* void app_main(void)
{
    ESP_LOGI(TAG, "Blink example started!");

    // --- GPIO 初始化步骤 ---

    // 1. 重置引脚，清除之前的设置
    gpio_reset_pin(BLINK_GPIO);
    
    // 2. 将 GPIO 设置为输出模式
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "GPIO configured. Starting the blink loop.");

    // --- 闪烁循环 ---
    while(1) {
        // 设置 GPIO 电平为高电平 (1)，点亮 LED
        ESP_LOGI(TAG, "Turning the LED ON");
        gpio_set_level(BLINK_GPIO, 1);
        
        // 延时 1000 毫秒 (1秒)
        // vTaskDelay 是 FreeRTOS 提供的标准延时函数，它会让任务休眠，非常高效
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 设置 GPIO 电平为低电平 (0)，熄灭 LED
        ESP_LOGI(TAG, "Turning the LED OFF");
        gpio_set_level(BLINK_GPIO, 0);

        // 再次延时 1000 毫秒 (1秒)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
} */