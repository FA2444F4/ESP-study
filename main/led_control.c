#include "led_control.h"
#include "led_strip.h"
#include "esp_log.h"

#define BLINK_GPIO             8
#define LED_STRIP_LED_NUMBERS  1
#define LED_STRIP_RMT_RES_HZ   (10 * 1000 * 1000) // 10MHz

static const char *TAG = "led控制"; 
static led_strip_handle_t led_strip = NULL;
typedef struct{
    uint8_t r;
    uint8_t g;
    uint8_t b;
}led_color_state_t;

// 初始化时默认颜色为白色，但灯是灭的
static led_color_state_t s_led_state = {255, 255, 255};

//初始化
void led_control_init(void)
{
    //strip配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = LED_STRIP_LED_NUMBERS,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    //rmt配置
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false,
    };
    
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "LED strip 初始化.");

    led_strip_clear(led_strip);
}
//设置颜色
void led_control_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (led_strip) {
        // 1. 更新内部状态
        s_led_state.r = red;
        s_led_state.g = green;
        s_led_state.b = blue;
        // 2. 设置 LED 硬件
        ESP_LOGI(TAG, "Setting color to R:%d G:%d B:%d", red, green, blue);
        // 注意 WS2812B 的 GRB 顺序
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, s_led_state.g, s_led_state.r, s_led_state.b));
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        }
}
//开灯
void led_control_turn_on(void)
{
    // 点亮为白色
    led_control_set_color(255, 255, 255);
    ESP_LOGI(TAG, "LED turned ON");
}
//关灯
void led_control_turn_off(void)
{
    if (led_strip) {
        ESP_ERROR_CHECK(led_strip_clear(led_strip));
        ESP_LOGI(TAG, "LED turned OFF");
    }
}