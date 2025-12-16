#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h" // ESP-IDF 5.x MCPWM 驱动
#include "esc_driver.h"

static const char *TAG = "ESC_DRIVER";

#define ESC_GPIO_PIN       18    // 你指定的 GPIO
#define ESC_PWM_RES_HZ     1000000 // PWM 计数器分辨率 1MHz (1 tick = 1us)，方便计算
#define ESC_PWM_FREQ_HZ    50      // 标准电调频率 50Hz (20ms 周期)
#define ESC_PWM_PERIOD_TICKS (ESC_PWM_RES_HZ / ESC_PWM_FREQ_HZ) // 20000 ticks

// 脉宽范围 (好盈天行者默认范围)
#define ESC_MIN_PULSE_US   1000 // 0% 油门
#define ESC_MAX_PULSE_US   2000 // 100% 油门

static mcpwm_cmpr_handle_t comparator = NULL; // 比较器句柄，用于调节占空比

void esc_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing ESC Driver on GPIO %d", ESC_GPIO_PIN);

    // 1. 配置定时器 (Timer)
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = ESC_PWM_RES_HZ,
        .period_ticks = ESC_PWM_PERIOD_TICKS,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // 2. 配置操作器 (Operator)
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t oper_config = {
        .group_id = 0, // 必须与 Timer 同一组
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &oper));
    
    // 连接 Timer 和 Operator
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    // 3. 配置比较器 (Comparator) - 这是我们用来改变占空比的核心
    mcpwm_comparator_config_t cmpr_config = {
        .flags.update_cmp_on_tez = true, // 在计数器归零时更新比较值
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &cmpr_config, &comparator));

    // 4. 配置发生器 (Generator) - 对应物理引脚
    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t gen_config = {
        .gen_gpio_num = ESC_GPIO_PIN,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_config, &generator));

    // 5. 设置 PWM 生成动作
    // 计数器达到比较值时拉低，计数器归零时拉高 (高电平时间 = 比较值)
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    // 6. 启用并启动定时器
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    // 7. 默认发送 0 油门信号 (1000us) 进行初始化/解锁
    // 电调上电通常需要接收几秒钟的最低油门信号才能正常工作
    esc_driver_set_throttle(0);
    
    ESP_LOGI(TAG, "ESC Driver initialized. Throttle set to 0%% (1000us).");
}

void esc_driver_set_throttle(int throttle_percent)
{
    if (comparator == NULL) return;

    // 限制范围 0-100
    if (throttle_percent < 0) throttle_percent = 0;
    if (throttle_percent > 100) throttle_percent = 100;

    // 映射: 0-100% -> 1000us-2000us
    // 公式: pulse = 1000 + (percent * 1000 / 100)
    uint32_t pulse_width_us = ESC_MIN_PULSE_US + (throttle_percent * (ESC_MAX_PULSE_US - ESC_MIN_PULSE_US) / 100);

    // 设置比较值 (因为分辨率是 1us/tick，所以直接填 us 值)
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, pulse_width_us));
    
    ESP_LOGD(TAG, "Set throttle: %d%%, Pulse: %lu us", throttle_percent, pulse_width_us);
}

// 命令解析回调
void esc_cmd_handler(const char *command, const char *args, cmd_responder_t responder, void *context)
{
    if (args == NULL) {
        if (responder) responder("Error: Missing speed value", context);
        return;
    }

    int speed = atoi(args);
    
    // 安全检查，防止输入错误导致电机疯转
    if (speed < 0 || speed > 100) {
        if (responder) responder("Error: Speed must be between 0 and 100", context);
        return;
    }

    esc_driver_set_throttle(speed);

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "OK: ESC Speed set to %d%%", speed);
    if (responder) responder(buffer, context);
    
    ESP_LOGI(TAG, "Command execute: Set ESC speed to %d", speed);
}