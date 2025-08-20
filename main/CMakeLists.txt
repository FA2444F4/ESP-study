
file(GLOB SRCS "main.c" "led_control.c" "uart_handler.c" "ble_handler.c" "*.c")

idf_component_register(
                    SRCS ${SRCS}
                    # SRCS
                    PRIV_REQUIRES 
                        spi_flash 
                        esp_driver_gpio 
                        led_strip 
                        esp_driver_uart 
                        nvs_flash
                        bt
                    REQUIRES esp_driver_ledc
                    INCLUDE_DIRS ".")


