#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

SemaphoreHandle_t bin_sem;
void taskA(void* param){
    //释放信号量
    while (1)
    {
        xSemaphoreGive(bin_sem);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}
void taskB(void* param){
    //获取信号量
    while(1){
        if(pdTRUE==xSemaphoreTake(bin_sem,portMAX_DELAY)){
            ESP_LOGI("bin","获取信号量成功");
        }
    }
    
}

/* void app_main(void)
{
    bin_sem=xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(taskB,"send",2048,NULL,3,NULL,tskNO_AFFINITY);
    xTaskCreatePinnedToCore(taskA,"receive",2048,NULL,3,NULL,tskNO_AFFINITY);
    
} */
