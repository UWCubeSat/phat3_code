idf_component_register(
    SRCS "ucam.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_uart esp_timer
)