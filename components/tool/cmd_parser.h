#ifndef CMD_PARSER_H
#define CMD_PARSER_H

#include "cmd_defs.h" // 包含通用定义

/**
 * @brief 处理一整行命令，并通过响应器返回结果
 * @param line 指向命令字符串
 * @param responder 用于回传响应的函数
 * @param context 调用者上下文
 */
void cmd_parser_process_line(char *line, cmd_responder_t responder, void *context);

#endif // CMD_PARSER_H