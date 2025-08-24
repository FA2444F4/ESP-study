
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h" // 引入 esp_rom_delay_us 的头文件
#include "u8g2.h"

// --- 配置您的 GPIO 引脚 ---
#define PIN_SCLK 19
#define PIN_MOSI 21
#define PIN_CS   18
#define PIN_DC   11
#define PIN_RST  10

// --- U8g2 的硬件抽象层 (HAL) ---
static spi_device_handle_t spi_handle;
static u8g2_t u8g2;


// U8g2 的回调函数：用于处理 GPIO 和延时
uint8_t u8x8_gpio_and_delay_esp32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            // 初始化 CS, DC, RST 引脚
            gpio_reset_pin(PIN_DC);
            gpio_reset_pin(PIN_RST);
            gpio_reset_pin(PIN_CS);
            gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
            gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
            gpio_set_direction(PIN_CS, GPIO_MODE_OUTPUT);
            break;
        case U8X8_MSG_GPIO_CS:
            gpio_set_level(PIN_CS, arg_int);
            break;
        case U8X8_MSG_GPIO_DC:
            gpio_set_level(PIN_DC, arg_int);
            break;
        case U8X8_MSG_GPIO_RESET:
            gpio_set_level(PIN_RST, arg_int);
            break;
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;
        case U8X8_MSG_DELAY_NANO:
            esp_rom_delay_us(arg_int == 0 ? 0 : (arg_int / 1000) + 1);
            break;
        default:
            return 0; // 表示消息未处理
    }
    return 1;
}

// U8g2 的回调函数：用于处理 SPI 通信
// (已根据报错全面修正)
uint8_t u8x8_byte_4wire_hw_spi_esp32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_BYTE_SEND: {
            spi_transaction_t trans;
            memset(&trans, 0, sizeof(trans));
            trans.length = arg_int * 8; // arg_int 是字节数
            trans.tx_buffer = arg_ptr;
            spi_device_polling_transmit(spi_handle, &trans);
            break;
        }
        case U8X8_MSG_BYTE_INIT:
            // SPI 总线已在外部初始化，此处无需操作
            break;
        case U8X8_MSG_BYTE_SET_DC:
            gpio_set_level(PIN_DC, arg_int);
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            gpio_set_level(PIN_CS, 0);
            u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_DELAY_NANO, 100, NULL); // 使用一个固定的短延时
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_DELAY_NANO, 100, NULL); // 使用一个固定的短延时
            gpio_set_level(PIN_CS, 1);
            break;
        default:
            return 0;
    }
    return 1;
}

const char *string_array[] = {
        "C", 
        "S", 
        "徐",
        "瑜",
        "禧"
};
int num_strings=sizeof(string_array) / sizeof(string_array[0]);
static void u8g2_task(void *pvParameters) {
    int line_move_count = 0; 
    int string_index = 0;
    
    while(1) {
        const char *current_string = string_array[string_index];
        u8g2_uint_t string_width = u8g2_GetUTF8Width(&u8g2, current_string);
        u8g2_uint_t x_pos = (u8g2_GetDisplayWidth(&u8g2) - string_width) / 2;
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawHLine(&u8g2, 0, line_move_count % 64, 128);
        u8g2_DrawUTF8(&u8g2, x_pos, 40, current_string);
        u8g2_SendBuffer(&u8g2);
        line_move_count++;
        if (line_move_count % 5 == 0) {
            string_index = (string_index + 1) % num_strings;
        }
        vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}

void u8g2_handler_init(void) {
    

    // 1. 初始化 SPI 总线 (与之前相同)
    spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1024,
    };
    spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 7,
    };
    spi_bus_add_device(SPI2_HOST, &dev_config, &spi_handle);

    // 2. 初始化 U8g2
    // 修正：U8g2 内部已经包含了回调函数的原型，我们直接使用 u8x8 提供的标准回调函数名称
    // u8g2_Setup_ssd1315_128x64_noname_f(u8g2, rotation, byte_cb, gpio_and_delay_cb)
    u8g2_Setup_ssd1315_128x64_noname_f(
        &u8g2, 
        U8G2_R0, //无旋转,横向
        u8x8_byte_4wire_hw_spi_esp32, //回调,spi通信
        u8x8_gpio_and_delay_esp32//回调,gpio和延时
    );
    
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    // --- 3. 开始绘图 (与之前相同) ---
   
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_wqy16_t_gb2312);
    u8g2_DrawUTF8(&u8g2, 50, 24, "Hello"); 
    u8g2_DrawUTF8(&u8g2, 50, 52, "World");
    vTaskDelay(pdMS_TO_TICKS(1000));

    u8g2_SendBuffer(&u8g2); // 将内部缓冲区的数据发送到屏幕

    printf("Display initialized and text drawn.\n");

    xTaskCreate(u8g2_task,"u8g2_task",2048,NULL,5,NULL);
}