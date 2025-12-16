#ifndef ESC_DRIVER_H
#define ESC_DRIVER_H

#include "esp_err.h"
#include "cmd_defs.h" // 为了使用 cmd_handler_t 定义

/**
 * @brief 初始化电调 (ESC) 驱动
 * 使用 MCPWM 输出 50Hz 脉冲
 */
void esc_driver_init(void);

/**
 * @brief 设置电调油门
 * @param throttle_percent 油门百分比 (0 - 100)
 */
void esc_driver_set_throttle(int throttle_percent);

/**
 * @brief 命令行处理函数
 * 格式: test_device_set_esc_speed=50
 */
void esc_cmd_handler(const char *command, const char *args, cmd_responder_t responder, void *context);

#endif // ESC_DRIVER_H