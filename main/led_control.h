#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h> // for uint8_t
#include "cmd_defs.h"

// 定义两种运行模式
typedef enum{
    LED_APP_MODE,   // 应用模式：由蓝牙连接状态自动控制
    LED_CUSTOM_MODE // 自定义模式：由外部命令手动控制
}led_run_mode_t;

// 定义蓝牙连接状态（用于 App Mode）
typedef enum {
    LED_BLE_DISCONNECTED, // 蓝牙未连接状态 (闪烁)
    LED_BLE_CONNECTED,    // 蓝牙已连接状态 (常亮)
} led_ble_status_t;

//初始化led
void led_control_init(void);
//设置rgb颜色并打开led
void led_control_set_color(uint8_t red, uint8_t green, uint8_t blue);
//打开led
void led_control_turn_on(void);
//关闭led
void led_control_turn_off(void);
//设置ble状态
void led_control_set_ble_status(led_ble_status_t status);
//设置led run mode
void led_control_set_run_mode(led_run_mode_t mode);
//命令处理
void led_cmd_handler(const char *command, const char *args,cmd_responder_t responder, void *context);

#endif // LED_CONTROL_H