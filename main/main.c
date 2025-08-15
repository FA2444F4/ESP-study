#include "esp_log.h"
#include "led_control.h"
#include "uart_handler.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Application main started.");

    // 初始化 LED 模块
    led_control_init();
    
    // 初始化 UART 模块 (它会在内部创建自己的任务)
    uart_handler_init();

    ESP_LOGI(TAG, "All modules initialized.");
}