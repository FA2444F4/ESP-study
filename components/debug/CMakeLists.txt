file(GLOB SRCS "debug_utils.c" "*.c")

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
                        tool
                    INCLUDE_DIRS ".")
