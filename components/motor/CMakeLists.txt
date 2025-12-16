file(GLOB SRCS  "*.c")

idf_component_register(
                    SRCS ${SRCS}
                    # SRCS
                    PRIV_REQUIRES 
                        spi_flash 
                        esp_driver_gpio 
                        esp_driver_spi
                        driver
                        tool
                    INCLUDE_DIRS ".")