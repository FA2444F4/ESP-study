#include "led_control.h"
#include "led_strip.h"
#include "esp_log.h"

#define BLINK_GPIO             8
#define LED_STRIP_LED_NUMBERS  1
#define LED_STRIP_RMT_RES_HZ   (10 * 1000 * 1000) // 10MHz

static const char *TAG = "LED_CONTROL";
static led_strip_handle_t led_strip = NULL;

void led_control_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = LED_STRIP_LED_NUMBERS,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false,
    };
    
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "LED strip initialized.");

    led_strip_clear(led_strip);
}

void led_control_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (led_strip) {
        // WS2812B 是 GRB 顺序，所以参数要对应调整
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, green, red, blue));
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    }
}

void led_control_turn_on(void)
{
    // 点亮为白色
    led_control_set_color(255, 255, 255);
    ESP_LOGI(TAG, "LED turned ON");
}

void led_control_turn_off(void)
{
    if (led_strip) {
        ESP_ERROR_CHECK(led_strip_clear(led_strip));
        ESP_LOGI(TAG, "LED turned OFF");
    }
}