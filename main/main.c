#include "esp_log.h"
#include "led_control.h"
#include "system_info.h"
#include "uart_handler.h"
#include "ble_handler.h"

static const char *TAG = "MAIN";


void app_main(void)
{
    ESP_LOGI(TAG, "Application main started.");

    //初始化
    system_info_init();
    led_control_init();
    uart_handler_init();
    ble_handler_init();
    ESP_LOGI(TAG, "All modules initialized.");
}