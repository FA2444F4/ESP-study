#include "debug_utils.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "hal/gpio_hal.h"

static const char *TAG = "DEBUG_UTILS";

// --- 内部调试函数 ---

/**
 * @brief 打印当前所有任务的状态列表
 * @note vTaskList 会直接打印到一个缓冲区，我们捕获它并用 responder 回传
 */
static void print_task_list(cmd_responder_t responder, void *context)
{
    // 为任务列表准备一个足够大的缓冲区
    char *task_list_buffer = (char *)malloc(1024);
    if (task_list_buffer == NULL) {
        responder("Error: Malloc failed for task list.", context);
        return;
    }
    
    // 获取任务列表信息
    vTaskList(task_list_buffer);
    
    // 通过 responder 发送回去
    // 我们需要逐行发送，因为 BLE 的 MTU 限制
    char *line = strtok(task_list_buffer, "\n");
    while (line != NULL) {
        responder(line, context);
        line = strtok(NULL, "\n");
        // 在发送多行时，给蓝牙一点喘息时间
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    free(task_list_buffer);
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


// --- 命令处理函数 ---

void debug_utils_cmd_handler(const char *command, const char *args, cmd_responder_t responder, void *context)
{
    if (strcmp(command, "debug_task_list") == 0) {
        print_task_list(responder, context);
    } else if (strcmp(command, "debug_heap_status") == 0) {
        print_heap_info(responder, context);
    } else if (strcmp(command, "debug_dump_gpio") == 0) {
        print_gpio_levels(responder, context);
    } else {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Error: Unknown debug command '%s'", command);
        responder(buffer, context);
    }
}