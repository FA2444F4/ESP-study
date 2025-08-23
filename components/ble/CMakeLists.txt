file(GLOB SRCS "ble_handler.c" "*.c")

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
                        esp_driver_ledc
                        led
                        info
                        tool
                    INCLUDE_DIRS ".")
