#include "cmd_parser.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include <ctype.h> // 用于 isspace 函数
// 引入模块
#include "led_control.h"
#include "system_info.h"
#include "debug_utils.h"


static const char *TAG = "CMD_PARSER";


/**
 * @brief 移除字符串头部和尾部的空白字符
 * @param str 要处理的字符串
 * @return 指向处理后字符串开头的指针
 */
static char* trim_whitespace(char *str)
{
    char *end;

    // 移除头部的空白字符
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == 0) { // 如果字符串全是空白
        return str;
    }

    // 移除尾部的空白字符
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    // 写入新的字符串结束符
    *(end + 1) = '\0';

    return str;
}

// 定义命令表中的每一项
typedef struct {
    const char *command_prefix; // 命令前缀，例如 "test_device_set_led"
    cmd_handler_t handler;      // 指向处理该类命令的函数
} cmd_entry_t;


// --- 命令分发表 ---
static const cmd_entry_t cmd_table[] = {
    {"test_device_set_led", led_cmd_handler},
    {"test_device_set_sn",  system_info_cmd_handler},
    {"test_device_get_sn",  system_info_cmd_handler},
    {"debug_",  debug_utils_cmd_handler},
    {NULL, NULL} // 表结束的标记
};

//根据输入指令调用命令分发表的handler
void cmd_parser_process_line(char *line,cmd_responder_t responder, void *context)
{
    line = trim_whitespace(line);

    if (strlen(line) == 0) {
        return; // 如果是空命令，直接返回
    }
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
            cmd_table[i].handler(command, args,responder,context);
            return; // 处理完毕，退出
        }
    }
    // 如果找不到命令，也通过 responder 返回错误信息
    if(responder){
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Error: Unknown command '%s'", command);
        responder(buffer, context);
    }

}