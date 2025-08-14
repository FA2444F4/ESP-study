#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

void taskA(void* param){
    while (1){
        
        ESP_LOGI("main","helloworld");
        // vTaskDelay(500/portTICK_PERIOD_MS);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
}

// void app_main(void)
// {
//     // xTaskCreatePinnedToCore(taskA,"helloworld",2048,NULL,3,NULL,0);
//     xTaskCreatePinnedToCore(taskA,"helloworld",2048,NULL,3,NULL,tskNO_AFFINITY);
// }
