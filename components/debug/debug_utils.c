#include "debug_utils.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "hal/gpio_hal.h"
#include "freertos/semphr.h"

static const char *TAG = "DEBUG_UTILS";

static SemaphoreHandle_t start_signal;//启动监控信号量
static volatile bool is_monitoring_active=false;//停止监控标志
static const int monitored_gpios[]={2,3,4,5,6,7,8,9,10,11};
#define NUM_MONITORED_GPIOS (sizeof(monitored_gpios) / sizeof(monitored_gpios[0]))

/**
 * @brief 打印当前所有任务的状态列表
 * @note vTaskList 会直接打印到一个缓冲区，我们捕获它并用 responder 回传
 */
static void print_task_list(cmd_responder_t responder, void *context)
{
    // // 为任务列表准备一个足够大的缓冲区
    // char *task_list_buffer = (char *)malloc(1024);
    // if (task_list_buffer == NULL) {
    //     responder("Error: Malloc failed for task list.", context);
    //     return;
    // }
    
    // // 获取任务列表信息
    // vTaskList(task_list_buffer);
    
    // // 通过 responder 发送回去
    // // 我们需要逐行发送，因为 BLE 的 MTU 限制
    // char *line = strtok(task_list_buffer, "\n");
    // while (line != NULL) {
    //     responder(line, context);
    //     line = strtok(NULL, "\n");
    //     // 在发送多行时，给蓝牙一点喘息时间
    //     vTaskDelay(pdMS_TO_TICKS(20));
    // }

    // free(task_list_buffer);
}

/**
 * @brief 打印系统堆内存信息
 */
static void print_heap_info(cmd_responder_t responder, void *context)
{
    char buffer[128];
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    snprintf(buffer, sizeof(buffer), "Heap Info:\n  Free: %zu bytes\n  Min Free Ever: %zu bytes", free_heap, min_free_heap);
    responder(buffer, context);
}


/**
 * @brief 打印所有有效 GPIO 引脚的当前电平状态
 */
static void print_gpio_levels(cmd_responder_t responder, void *context)
{
    char buffer[128];
    
    // 打印一个标题
    snprintf(buffer, sizeof(buffer), "--- GPIO Level Dump ---");
    responder(buffer, context);
    vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延时，确保上一条消息发送出去

    // 遍历芯片所有的可能引脚号
    for (int i = 0; i < GPIO_NUM_MAX; i++) {
        // 使用宏检查当前引脚号是否是一个有效的、可用的 GPIO
        if (GPIO_IS_VALID_GPIO(i)) {
            // 获取该 GPIO 的当前数字电平 (0 或 1)
            int level = gpio_get_level(i);
            snprintf(buffer, sizeof(buffer), "GPIO[%02d]: Level: %d", i, level);
            responder(buffer, context);
            
            // 增加一个小延时，防止消息刷屏太快，尤其是在BLE通知时
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    snprintf(buffer, sizeof(buffer), "--- End GPIO Level Dump ---");
    gpio_dump_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);//查看所有管脚的配置状态
    responder(buffer, context);
}

/**
 * @brief GPIO 监控任务
 */
static void gpio_monitor_task(void *pvParameters){
     ESP_LOGI(TAG, "Task created and waiting for start signal.");
     int last_levels[NUM_MONITORED_GPIOS];
     //初始化电平状态
     for (int i = 0; i < NUM_MONITORED_GPIOS; i++) {
        last_levels[i]=gpio_get_level(monitored_gpios[i]);
     }
     while (1){
        xSemaphoreTake(start_signal,portMAX_DELAY);//堵塞,等待获取启动信号量
        ESP_LOGI(TAG, "Monitoring started!");
        while(is_monitoring_active){
            for (int i = 0; i < NUM_MONITORED_GPIOS; i++) {
                int current_level = gpio_get_level(monitored_gpios[i]);
                if (current_level != last_levels[i]) {
                    last_levels[i] = current_level;
                    printf("GPIO_MONITOR_EVENT: Pin %d changed to %d\r\n", monitored_gpios[i], current_level);
                }
            }
            // 轮询间隔
            vTaskDelay(pdMS_TO_TICKS(50));
        }
     }
     ESP_LOGI(TAG, "Monitoring stopped. Waiting for next start signal.");
     
}

void debug_utils_init(void){
    start_signal = xSemaphoreCreateBinary();
    for (int i = 0; i < NUM_MONITORED_GPIOS; i++) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << monitored_gpios[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE, // 启用内部上拉，防止引脚浮空
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);
    }
    xTaskCreate(gpio_monitor_task, "gpio_monitor_task", 4096, NULL, 5, NULL);
}

// --- 命令处理函数 ---

void debug_utils_cmd_handler(const char *command, const char *args, cmd_responder_t responder, void *context)
{
    //debug_task_list
    if (strcmp(command, "debug_task_list") == 0) {
        print_task_list(responder, context);
    } 
    //debug_heap_status
    else if (strcmp(command, "debug_heap_status") == 0) {
        print_heap_info(responder, context);
    } 
    //debug_dump_gpio
    else if (strcmp(command, "debug_dump_gpio") == 0) {
        print_gpio_levels(responder, context);
    } 
    //debug_gpio_monitor_start
    else if (strcmp(command, "debug_gpio_monitor_start") == 0) {
        if (!is_monitoring_active) {
            is_monitoring_active = true;
            xSemaphoreGive(start_signal); // 发送“启动”信号，唤醒任务
            responder("OK: GPIO monitoring started. Events will be printed to UART.", context);
        } else {
            responder("Warning: Monitoring is already active.", context);
        }
    } 
    //debug_gpio_monitor_end
    else if (strcmp(command, "debug_gpio_monitor_end") == 0) {
        if (is_monitoring_active) {
            is_monitoring_active = false; // 设置“停止”标志
            responder("OK: GPIO monitoring stopped.", context);
        } else {
            responder("Warning: Monitoring is not active.", context);
        }
    }
    else {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Error: Unknown debug command '%s'", command);
        responder(buffer, context);
    }
}