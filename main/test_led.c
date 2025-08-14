#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define LED_GPIO GPIO_NUM_4
void led_run_task(void* param){
    int gpio_level=0;
    while (1){
        gpio_level=gpio_level?0:1;
        ESP_LOGI("main","led%s",gpio_level==1?"on":"off");
        gpio_set_level(LED_GPIO,gpio_level);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}

/* void app_main(void)
{
   gpio_config_t led_cfg={
    .pin_bit_mask=(1<<LED_GPIO),
    .pull_up_en=GPIO_PULLUP_DISABLE,
    .pull_down_en=GPIO_PULLDOWN_DISABLE,
    .mode=GPIO_MODE_OUTPUT,
    .intr_type=GPIO_INTR_DISABLE,
   };
   gpio_config(&led_cfg);

   

//    xTaskCreatePinnedToCore(led_run_task,"led",2048,NULL,3,NULL,tskNO_AFFINITY);

    
} */