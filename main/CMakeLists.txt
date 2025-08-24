
file(GLOB SRCS "main.c" "*.c")

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
                        led
                        uart
                        info
                        ble
                        screen
                        
                    REQUIRES esp_driver_ledc
                    INCLUDE_DIRS ".")


