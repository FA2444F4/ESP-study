#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H
#include "cmd_defs.h"

// --- 模块内部使用的数据结构 ---
typedef enum {
    CMD_TYPE_CONNECT,
    CMD_TYPE_DISCONNECT,
    CMD_TYPE_SCAN,
} wifi_cmd_type_t;// 定义可以发送给 Wi-Fi 控制任务的命令类型

/**
 * @brief 初始化Wi-Fi处理模块
 * 此函数应在程序启动时被调用一次。
 */
void wifi_handler_init(void);

/**
 * @brief Wi-Fi相关命令的处理器
 * * @param command 命令字符串
 * @param args 参数字符串
 * @param responder 用于发送响应的回调函数
 * @param context 传递给回调函数的上下文指针
 */
void wifi_cmd_handler(const char *command, const char *args, cmd_responder_t responder, void *context);

#endif // WIFI_HANDLER_H