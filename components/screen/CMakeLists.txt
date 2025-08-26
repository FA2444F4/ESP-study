file(GLOB SRCS  "*.c")

idf_component_register(
                    SRCS ${SRCS}
                    # SRCS
                    PRIV_REQUIRES spi_flash 
                        esp_driver_gpio 
                        esp_driver_spi
                        lvgl
                        u8g2
                        esp_lcd
                    INCLUDE_DIRS ".")