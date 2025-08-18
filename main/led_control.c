#include "led_control.h"
#include "led_strip.h"
#include "esp_log.h"
#include <string.h>

#define BLINK_GPIO             8
#define LED_STRIP_LED_NUMBERS  1
#define LED_STRIP_RMT_RES_HZ   (10 * 1000 * 1000) // 10MHz

static const char *TAG = "led control"; 
static TaskHandle_t led_task_handle = NULL; // 用于控制任务的句柄
static led_strip_handle_t led_strip = NULL;
static led_run_mode_t s_current_run_mode=LED_APP_MODE;//开机默认app模式
static led_ble_status_t s_ble_status= LED_BLE_DISCONNECTED;

static struct{
    uint8_t r;
    uint8_t g;
    uint8_t b;
}s_custom_color={255,255,255};

//led任务
static void led_task(void *pvParameters){
    bool led_on_for_blink = false;
    while (1){
        switch (s_current_run_mode){
            case LED_APP_MODE:
                if(s_ble_status==LED_BLE_DISCONNECTED){//蓝牙未连接(蓝灯闪烁)
                    led_on_for_blink=!led_on_for_blink;
                    if(led_on_for_blink){
                        led_strip_set_pixel(led_strip,0,0,0,10);
                    }else{
                        led_strip_clear(led_strip);
                    }
                    led_strip_refresh(led_strip);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }else{//蓝牙已连接(蓝灯常亮)
                    led_strip_set_pixel(led_strip,0,0,0,10);
                    led_strip_refresh(led_strip);
                    vTaskSuspend(NULL);
                }
                break;
            case LED_CUSTOM_MODE:
                // --- 自定义模式逻辑 ---
                // 在自定义模式下，任务不需要自己做任何事，所有行为都由外部调用函数触发。
                // 因此直接挂起自身，等待被唤醒。
            vTaskSuspend(NULL);
                break;
            default:
                break;
        }
    }
    
}
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
    ESP_LOGI(TAG, "LED strip init.");
    led_strip_clear(led_strip);
    xTaskCreate(led_task,"led_task",2048,NULL,5,&led_task_handle);
}

//唤醒任务的辅助函数
static void kick_led_task(void){
    if(led_task_handle&&eTaskGetState(led_task_handle)==eSuspended){
        vTaskResume(led_task_handle);
    }
}

//设置led run mode
void led_control_set_run_mode(led_run_mode_t mode){
    if(s_current_run_mode==mode) return;
    ESP_LOGI(TAG, "Switching run mode to %s", (mode == LED_APP_MODE) ? "APP_MODE" : "CUSTOM_MODE");
    s_current_run_mode = mode;
    // 如果切换到自定义模式，先关灯，等待指令
    if (s_current_run_mode == LED_CUSTOM_MODE) {
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
    }
    kick_led_task(); // 唤醒任务以应用新模式
}

//设置ble状态
void led_control_set_ble_status(led_ble_status_t status){
    if(s_ble_status==status)return;
    s_ble_status=status;
    if(s_current_run_mode==LED_APP_MODE){
        kick_led_task();
    }
}

//设置颜色
void led_control_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (s_current_run_mode==LED_CUSTOM_MODE) {
        ESP_LOGI(TAG, "CUSTOM_MODE: Setting color to R:%d G:%d B:%d", red, green, blue);
        s_custom_color.r = red;
        s_custom_color.g = green;
        s_custom_color.b = blue;
        // 2. 设置 LED 硬件
        ESP_LOGI(TAG, "Setting color to R:%d G:%d B:%d", red, green, blue);
        // 注意 WS2812B 的 GRB 顺序
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, s_custom_color.r, s_custom_color.g, s_custom_color.b));
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    }else{
        ESP_LOGW(TAG, "Ignoring set_color command: Not in CUSTOM_MODE.");
    }
}

//开灯
void led_control_turn_on(void)
{
    if (s_current_run_mode==LED_CUSTOM_MODE) {
        ESP_LOGI(TAG, "Turning LED ON with last color");
        // 使用保存在 s_led_state 中的上一次颜色来点亮 LED
        led_control_set_color(s_custom_color.r, s_custom_color.g, s_custom_color.b);
    }else{
        ESP_LOGW(TAG, "Ignoring turn_on command: Not in CUSTOM_MODE.");
    }
}
//关灯
void led_control_turn_off(void)
{
    if (s_current_run_mode == LED_CUSTOM_MODE) {
        ESP_LOGI(TAG, "CUSTOM_MODE: Turning LED OFF");
        ESP_ERROR_CHECK(led_strip_clear(led_strip));
    } else {
        ESP_LOGW(TAG, "Ignoring turn_off command: Not in CUSTOM_MODE.");
    }
}

//命令处理函数
void led_cmd_handler(const char *command, const char *args,cmd_responder_t responder, void *context)
{
    //test_device_set_led_runmode=app/custom
    if(strcmp(command, "test_device_set_led_runmode")==0){
        if (args == NULL) {
            ESP_LOGE("LED_CMD", "Missing value for statu (on/off)");
            return;
        }
        if(strcmp(args, "app") == 0){
            led_control_set_run_mode(LED_APP_MODE);
        }else if(strcmp(args, "custom") == 0){
            led_control_set_run_mode(LED_CUSTOM_MODE);
        }
    }
    //test_device_set_led_statu=on/off
    else if (strcmp(command, "test_device_set_led_statu") == 0) {
        if (args == NULL) {
            ESP_LOGE("LED_CMD", "Missing value for statu (on/off)");
            return;
        }
        if (strcmp(args, "on") == 0) {
            led_control_turn_on();
        } else if (strcmp(args, "off") == 0) {
            led_control_turn_off();
        }
    } 
    //test_device_set_led_rgb=255,255,255
    else if (strcmp(command, "test_device_set_led_rgb") == 0) {
        if (args == NULL) {
            ESP_LOGE("LED_CMD", "Missing value for rgb (r,g,b)");
            return;
        }
        uint8_t r, g, b;
        if (sscanf(args, "%hhu,%hhu,%hhu", &r, &g, &b) == 3) {
            led_control_set_color(r, g, b);
        }
    } 
    else {
        ESP_LOGW("LED_CMD", "Unknown LED command: %s", command);
    }
}