#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h> // for uint8_t

/**
 * @brief 初始化 LED 灯带
 */
void led_control_init(void);

/**
 * @brief 点亮 LED 为指定的 RGB 颜色
 *
 * @param red   红色分量 (0-255)
 * @param green 绿色分量 (0-255)
 * @param blue  蓝色分量 (0-255)
 */
void led_control_set_color(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 以最大亮度点亮 LED (白色)
 */
void led_control_turn_on(void);

/**
 * @brief 关闭 LED
 */
void led_control_turn_off(void);

#endif // LED_CONTROL_H