# idf_component_register(SRCS "cmd_parser.c" "led_control.c" "uart_handler.c" "test_pwd_led.c" "test_led.c" "test_queue.c" "test_task.c"
#                        PRIV_REQUIRES spi_flash
#                        INCLUDE_DIRS "")

# 自动发现模式：自动查找当前目录下的所有 .c 文件
file(GLOB SRCS "*.c")

idf_component_register(
                    SRCS ${SRCS}
                    # SRCS "test_rgb_led.c"
                    PRIV_REQUIRES spi_flash esp_driver_gpio led_strip esp_driver_uart
                    REQUIRES esp_driver_ledc
                    INCLUDE_DIRS ".")

