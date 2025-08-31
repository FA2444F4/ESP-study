file(GLOB SRCS  "*.c")

idf_component_register(
                    SRCS ${SRCS}
                    # SRCS
                    PRIV_REQUIRES 
                        spi_flash 
                        esp_driver_gpio 
                        esp_driver_uart 
                        nvs_flash
                        info
                        tool
                        esp_wifi
                        esp_event
                        sensor
                        json
                    INCLUDE_DIRS ".")
