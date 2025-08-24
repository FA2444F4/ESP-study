#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "led_control.h"
#include "system_info.h"
#include "uart_handler.h"
#include "ble_handler.h"
#include "debug_utils.h"
// #include "lvgl_handler.h"
// #include "lvgl.h"
// #include "ssd1315_driver.h" 
#include "u8g2_handler.h"
static const char *TAG = "MAIN";


void app_main(void)
{
    ESP_LOGI(TAG, "Application main started.");

    //初始化
    system_info_init();
    led_control_init();
    uart_handler_init();
    ble_handler_init();
    debug_utils_init();

    // lvgl_handler_init();//ssd1315 oled spi error

    u8g2_handler_init();



    ESP_LOGI(TAG, "All modules initialized.");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

}