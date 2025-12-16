#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "led_control.h"
#include "system_info.h"
#include "uart_handler.h"
#include "ble_handler.h"
#include "debug_utils.h"

// #include "ssd1315_driver.h" 
#include "u8g2_handler.h"
#include "st7789v2_driver.h"
#include "lvgl_handler.h"
#include "mpu6050_handler.h"
#include "wifi_handler.h"
#include "sg90_control.h"
#include "esc_driver.h"
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
    wifi_handler_init();
    // mpu6050_handler_init();
    sg90_control_init();

    esc_driver_init(); // <--- 2. 初始化电调
    // 注意：好盈电调上电初始化时，必须听到电调发出 "Beep-Beep" (代表电池节数) 
    // 然后一声长 "Beep" (代表油门归零完成) 后，才能发送转动指令。
    // esc_driver_init 默认会输出 0 油门信号来触发这个流程。

    // u8g2_handler_init();//ssd1315 oled spi ok
    // st7789v2_driver_init(); st7789v2 lcd spi ok
    // st7789v2_driver_fill_with_rect_test(); st7789v2 lcd spi ok
    // lvgl_handler_init();

    ESP_LOGI(TAG, "All modules initialized.");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

}