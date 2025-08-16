#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h> // for uint8_t

//初始化led
void led_control_init(void);
//设置rgb颜色并打开led
void led_control_set_color(uint8_t red, uint8_t green, uint8_t blue);
//打开led
void led_control_turn_on(void);
//关闭led
void led_control_turn_off(void);
//命令处理
void led_cmd_handler(const char *command, const char *args);

#endif // LED_CONTROL_H