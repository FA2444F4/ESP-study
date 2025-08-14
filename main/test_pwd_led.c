#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h" // 包含 esp_rom_delay_us 微秒延时函数

// --- 配置参数 ---

// LED 连接的 GPIO 引脚
#define BREATHING_GPIO      4

// PWM 分辨率，即亮度级别。255 代表 8 位分辨率。
#define PWM_MAX_DUTY        255

// 呼吸灯变化的步长，数值越大，呼吸速度越快
#define BREATHING_STEP      5

// 呼吸灯每一步变化之间的时间间隔（毫秒），数值越小，呼吸速度越快
#define BREATHING_DELAY_MS  20

// 软件 PWM 的频率 (Hz)。500Hz 对于人眼来说已经完全看不到闪烁
#define PWM_FREQUENCY       500


// 日志标签
static const char *TAG = "SOFT_PWM_BREATHING_LED";


/**
 * @brief 呼吸灯任务函数
 * @param pvParameters 未使用
 */
void breathing_led_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Task started on GPIO %d", BREATHING_GPIO);

    // 计算软件 PWM 的总周期（微秒）
    // 周期 T = 1 / 频率 f
    // 1 秒 = 1,000,000 微秒
    const uint32_t pwm_period_us = 1000000 / PWM_FREQUENCY;

    // 初始化亮度（占空比）和变化方向
    int duty_cycle = 0;
    int direction = BREATHING_STEP; // 初始方向：亮度增加

    while (1) {
        // --- 1. 更新亮度值 (占空比) ---
        duty_cycle += direction;

        // 如果亮度达到最大值，则改变方向为减少
        if (duty_cycle >= PWM_MAX_DUTY) {
            duty_cycle = PWM_MAX_DUTY;
            direction = -BREATHING_STEP; // 方向变为递减
        }
        // 如果亮度达到最小值，则改变方向为增加
        else if (duty_cycle <= 0) {
            duty_cycle = 0;
            direction = BREATHING_STEP; // 方向变为递增
        }

        // --- 2. 根据当前亮度值，生成一个周期的 PWM 波形 ---
        // 只有当亮度大于0时，才需要拉高电平
        if (duty_cycle > 0) {
            gpio_set_level(BREATHING_GPIO, 1); // LED 亮
            // 计算高电平持续时间
            uint32_t high_level_time_us = (pwm_period_us * duty_cycle) / PWM_MAX_DUTY;
            esp_rom_delay_us(high_level_time_us);
        }

        // 只要亮度小于最大值，就需要有低电平时间
        if (duty_cycle < PWM_MAX_DUTY) {
            gpio_set_level(BREATHING_GPIO, 0); // LED 灭
            // 计算低电平持续时间
            uint32_t low_level_time_us = pwm_period_us - ((pwm_period_us * duty_cycle) / PWM_MAX_DUTY);
            esp_rom_delay_us(low_level_time_us);
        }

        // --- 3. 控制呼吸速度 ---
        // 在完成多个 PWM 周期后进行一次短暂的、能让出 CPU 的延时
        // 这个延时决定了呼吸的快慢。如果直接用上面的微秒延时，呼吸会快到看不见。
        vTaskDelay(pdMS_TO_TICKS(BREATHING_DELAY_MS));
    }
}


/* void app_main(void)
{
    ESP_LOGI(TAG, "Initializing GPIO...");

    // 配置 GPIO
    gpio_reset_pin(BREATHING_GPIO);
    gpio_set_direction(BREATHING_GPIO, GPIO_MODE_OUTPUT);

    // 创建呼吸灯任务
    xTaskCreate(
        breathing_led_task,   // 任务函数
        "breathing_led_task", // 任务名称
        2048,                 // 任务堆栈大小
        NULL,                 // 传递给任务的参数
        5,                    // 任务优先级
        NULL                  // 任务句柄
    );
} */