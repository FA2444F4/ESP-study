#include <stdio.h>



#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

QueueHandle_t queue_handle=NULL;
typedef struct{
    int value;
}queue_data_t;

void taskA(void* param){
    queue_data_t data;
    while(1){
        if(pdTRUE==xQueueReceive(queue_handle,&data,pdMS_TO_TICKS(100))){
            ESP_LOGI("queue","接收数据%d",data.value);
        }
    }
    
}
void taskB(void* param){
    queue_data_t data;
    memset(&data,0,sizeof(queue_data_t));
    while (1)
    {
        xQueueSend(queue_handle,&data,data.value++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}
/* void app_main(void)
{
    queue_handle=xQueueCreate(10,sizeof(queue_data_t));
    xTaskCreatePinnedToCore(taskB,"send",2048,NULL,3,NULL,tskNO_AFFINITY);
    xTaskCreatePinnedToCore(taskA,"receive",2048,NULL,3,NULL,tskNO_AFFINITY);
    
} */
