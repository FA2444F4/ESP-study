#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

SemaphoreHandle_t bin_sem;
void taskA(void* param){
    while (1)
    {
        // xSemaphoreTake(bin_sem,portMAX_DELAY);

        // xSemaphoreGive(bin_sem);
    }
    
}
void taskB(void* param){
    while(1){
        // xSemaphoreTake(bin_sem,portMAX_DELAY);
            
        // xSemaphoreGive(bin_sem);
    }
    
}

// void app_main(void)
// {
//     bin_sem=xSemaphoreCreateMutex();
//     xTaskCreatePinnedToCore(taskB,"send",2048,NULL,3,NULL,tskNO_AFFINITY);
//     xTaskCreatePinnedToCore(taskA,"receive",2048,NULL,3,NULL,tskNO_AFFINITY);
    
// }
