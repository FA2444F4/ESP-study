#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"

#include "sg90_control.h"

static const char *TAG = "SERVO_CONTROL";

// 定义舵机和PWM的参数
#define SERVO_GPIO              12  // 舵机连接的GPIO引脚
#define SERVO_TIMER_RESOLUTION  1000000 // MCPWM定时器分辨率, 1MHz, 1 tick = 1us
#define SERVO_TIMER_FREQ_HZ     50      // PWM频率为50Hz
#define SERVO_PULSE_PERIOD_TICKS (SERVO_TIMER_RESOLUTION / SERVO_TIMER_FREQ_HZ) // 周期对应的ticks数 (20000)

#define SERVO_PULSE_MIN_US      500     // 0度对应的最小脉宽 (微秒)
#define SERVO_PULSE_MAX_US      2500    // 180度对应的最大脉宽 (微秒)
#define SERVO_ANGLE_MAX         180     // 最大角度

static mcpwm_cmpr_handle_t comparator = NULL;

/**
 * @brief 将角度 (0-180) 转换为MCPWM比较器需要的脉冲宽度值 (ticks)
 * @param angle_of_rotation 目标角度
 * @return uint32_t 脉冲宽度 (单位: ticks)
 */
static inline uint32_t angle_to_compare(int angle)
{
    // 线性映射: 将 [0, 180] 度的范围映射到 [500, 2500] us的脉宽范围
    return (angle * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US) / SERVO_ANGLE_MAX + SERVO_PULSE_MIN_US);
}



void sg90_control_init(){
    ESP_LOGI(TAG, "开始配置MCPWM用于舵机控制...");

    // 1. 创建MCPWM定时器
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMER_RESOLUTION,
        .period_ticks = SERVO_PULSE_PERIOD_TICKS,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // 2. 创建MCPWM操作器 (Operator)
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // 必须和定时器在同一个group
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

    // 3. 将定时器连接到操作器
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    // 4. 创建MCPWM比较器 (Comparator)
    comparator = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true, // 在定时器计数为0时更新比较值
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    // 5. 创建MCPWM生成器 (Generator) 并设置GPIO
    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = SERVO_GPIO,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    // 设置初始角度为90度 (中间位置)
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, angle_to_compare(90)));

    // 第6步: 设置生成器的动作 - 这是生成PWM波形的关键

    // 当定时器向上计数且计数器归零时，将GPIO拉高
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));

    // 当定时器向上计数且等于比较值时，将GPIO拉低
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    // 7. 启动定时器
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    ESP_LOGI(TAG, "MCPWM配置完成。");

    // 循环控制舵机转动
    // while (1) {
    //     ESP_LOGI(TAG, "转到 0 度");
    //     ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, angle_to_compare(0)));
    //     vTaskDelay(pdMS_TO_TICKS(1500)); // 等待舵机转动

    //     ESP_LOGI(TAG, "转到 90 度");
    //     ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, angle_to_compare(90)));
    //     vTaskDelay(pdMS_TO_TICKS(1500));

    //     ESP_LOGI(TAG, "转到 180 度");
    //     ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, angle_to_compare(180)));
    //     vTaskDelay(pdMS_TO_TICKS(1500));
    // }
}


void sg90_cmd_handler(const char *command, const char *args,cmd_responder_t responder, void *context){
    //命令 test_device_set_sg90_angle=90
    if (strcmp(command, "test_device_set_sg90_angle") == 0){
        if (args == NULL) {
            ESP_LOGE(TAG, "缺少角度参数");
            return;
        }
        int angle=atoi(args);
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, angle_to_compare(angle)));
        char buffer[128];
        snprintf(buffer,sizeof(buffer),"set angle:%d",angle);
        responder(buffer,context);
        return;
    }
}
 