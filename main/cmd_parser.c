#include "cmd_parser.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"

// 引入模块
#include "led_control.h"


static const char *TAG = "CMD_PARSER";

// 定义命令处理的函数指针类型
typedef void (*cmd_handler_t)(const char *command, const char *args);

// 定义命令表中的每一项
typedef struct {
    const char *command_prefix; // 命令前缀，例如 "test_device_set_led"
    cmd_handler_t handler;      // 指向处理该类命令的函数
} cmd_entry_t;


// --- 命令分发表 ---
static const cmd_entry_t cmd_table[] = {
    {"test_device_set_led", led_cmd_handler},
    // {"test_device_set_wifi", wifi_cmd_handler},     // 示例
    // {"test_device_get",      system_info_handler}, // 示例
    {NULL, NULL} // 表结束的标记
};

//根据输入指令调用命令分发表的handler
void cmd_parser_process_line(char *line)
{
    char *command = line;
    char *args = NULL;

    // 解析命令和参数
    // 检查是否存在 '=' 分隔符
    args = strchr(line, '=');//strchr返回'='出现的指针
    if (args != NULL) {
        *args = '\0'; // 分割命令和参数 //args是'='内存地址的指针,*args是那一块内存
        args++;      // 指向参数的开头 //指针往后移一位,跳过'='
    }
    //已分离出command命令和args参数

    // 遍历命令表，寻找匹配的处理器
    for (int i = 0; cmd_table[i].command_prefix != NULL; i++) {
        if (strncmp(command, cmd_table[i].command_prefix, strlen(cmd_table[i].command_prefix)) == 0) {
            // 找到了匹配的前缀，调用对应的处理函数
            cmd_table[i].handler(command, args);
            return; // 处理完毕，退出
        }
    }

    ESP_LOGW(TAG, "Unknown command prefix: %s", command);
}