#pragma once
#include <mpu6050.h>

// All data from a combined sensor reading.
typedef struct {
    float aht21_temperature;
    float aht21_humidity;
    float bmp180_temperature;
    uint32_t bmp180_pressure;
    float mpu6050_accel_x;
    float mpu6050_accel_y;
    float mpu6050_accel_z;
    float mpu6050_rot_x;
    float mpu6050_rot_y;
    float mpu6050_rot_z;
    uint16_t scd41_co2;
    float scd41_temperature;
    float scd41_humidity;
} sensors_data_t;

// Initializes all the sensors on the PHAT-3 board.
esp_err_t init_sensors(void);

// Reads all data from all sensors.
//
// Must wait at least 5 seconds between calls.
esp_err_t read_all_sensor_data(sensors_data_t* sensor_data_ret);