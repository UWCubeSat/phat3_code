idf_component_register(
    SRCS "sensors.c"
    INCLUDE_DIRS "."
    REQUIRES esp-idf-lib__mpu6050 esp_timer esp-idf-lib__aht esp-idf-lib__bmp180 esp-idf-lib__scd4x gps
)