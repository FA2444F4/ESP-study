#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include "cmd_defs.h" // 引入我们通用的命令定义

/**
 * @brief 用于处理所有'dbg_'开头的调试命令
 */
void debug_utils_cmd_handler(const char *command, const char *args, cmd_responder_t responder, void *context);

#endif // DEBUG_UTILS_H