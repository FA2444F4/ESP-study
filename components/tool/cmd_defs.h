#ifndef CMD_DEFS_H
#define CMD_DEFS_H

/**
 * @brief 定义一个“响应器”函数指针类型。
 * 所有命令的执行结果都将通过这个函数回传。
 * @param response_str 指向要回传的响应字符串
 * @param context 指向调用者提供的上下文信息（例如，BLE连接句柄）
 */
typedef void (*cmd_responder_t)(const char *response_str, void *context);

/**
 * @brief 定义命令处理函数的指针类型。
 * @param command 完整的命令字符串
 * @param args 参数字符串 (如果存在)
 * @param responder 用于回传响应的函数
 * @param context 调用者上下文
 */
typedef void (*cmd_handler_t)(const char *command, const char *args, cmd_responder_t responder, void *context);

#endif // CMD_DEFS_H