#include "uart_handler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "led_control.h"
#include "cmd_parser.h"
#include "cmd_parser.h"

#define UART_NUM          UART_NUM_0 // 使用与 USB 监视器相同的 UART0
#define UART_RX_BUF_SIZE  (128)

static const char *TAG = "UART_HANDLER";

// --- UART 专用的响应器函数 ---
static void uart_responder(const char *response_str, void *context)
{
    // UART 的响应器很简单，就是直接打印
    printf("%s\r\n", response_str);
}

//接收任务
static void uart_rx_task(void *pvParameters)
{
    uint8_t *data = (uint8_t *) malloc(UART_RX_BUF_SIZE + 1);

    while (1) {
        // 读取 UART 数据，此函数会阻塞直到有数据进来
        int len = uart_read_bytes(UART_NUM, data, UART_RX_BUF_SIZE, pdMS_TO_TICKS(1000));
        
        if (len > 0) {
            // 移除末尾可能的回车换行符
            while (len > 0 && (data[len-1] == '\r' || data[len-1] == '\n')) {
                len--;
            }
            data[len] = '\0'; // 添加字符串结束符

            if(len > 0) {
                // 将接收到的完整命令交给命令解析器处理
                cmd_parser_process_line((char*)data,uart_responder,NULL);
            }
        }
    }
    free(data);
}


void uart_handler_init(void)
{
    // 注意：ESP-IDF 的日志功能已经初始化了 UART0。
    // 如果我们只是想从这个 UART 读取，通常不需要再进行完整的配置，
    // 只需要安装驱动并创建读取任务即可。
    // 如果日志功能没有被使用，则需要完整的 uart_param_config 等配置。
    
    // 安装 UART 驱动
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_RX_BUF_SIZE * 2, 0, 0, NULL, 0));

    // 创建任务来处理 UART 数据
    xTaskCreate(uart_rx_task, "uart_rx_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "UART handler initialized and task started.");
}