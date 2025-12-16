file(GLOB SRCS "esc_driver.c" "*.c")

idf_component_register(
                    SRCS ${SRCS}
                    REQUIRES driver esp_common tool
                    INCLUDE_DIRS ".")