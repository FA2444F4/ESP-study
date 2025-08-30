file(GLOB SRCS  "*.c")

idf_component_register(
                    SRCS ${SRCS}
                    # SRCS
                    PRIV_REQUIRES 
                        spi_flash 
                        esp_driver_gpio 
                        esp_driver_uart 
                        esp_driver_i2c
                        tool
                        driver
                    INCLUDE_DIRS ".")
