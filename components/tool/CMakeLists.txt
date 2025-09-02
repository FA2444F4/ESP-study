file(GLOB SRCS  "*.c")

idf_component_register(
                    SRCS ${SRCS}
                    # SRCS
                    PRIV_REQUIRES spi_flash 
                        esp_driver_gpio 
                        led_strip 
                        esp_driver_uart 
                        nvs_flash
                        bt
                        esp_driver_ledc
                        info
                        led
                        wifi
                        sensor
                    REQUIRES debug
                    INCLUDE_DIRS ".")
