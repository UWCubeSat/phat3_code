#pragma once
#include <mpu6050.h>
#include <inttypes.h>

// All data from a combined sensor reading.
typedef struct {
    uint64_t millis_since_start;
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

    double gps_latitude;
    double gps_longitude;
    char gps_timestamp[30];
} sensors_data_t;

// Header at the top of sensor CSV file
#define CSV_HEADER_LINE \
"millis_since_start, " \
"aht21_temperature, aht21_humidity, " \
"bmp180_temperature, bmp180_pressure, " \
"mpu6050_accel_x, mpu6050_accel_y, mpu6050_accel_z, " \
"mpu6050_rot_x, mpu6050_rot_y, mpu6050_rot_z, " \
"scd41_co2, scd41_temperature, scd41_humidity, " \
"gps_latitude, gps_longitude, gps_timestamp\n"

// Helper-macro for passing in sensor_data fields
// to format functions.
#define SENSOR_DATA_EXPAND(sensor_data) \
(sensor_data).millis_since_start, \
(sensor_data).aht21_temperature, \
(sensor_data).aht21_humidity, \
(sensor_data).bmp180_temperature, \
(sensor_data).bmp180_pressure, \
(sensor_data).mpu6050_accel_x, \
(sensor_data).mpu6050_accel_y, \
(sensor_data).mpu6050_accel_z, \
(sensor_data).mpu6050_rot_x, \
(sensor_data).mpu6050_rot_y, \
(sensor_data).mpu6050_rot_z, \
(sensor_data).scd41_co2, \
(sensor_data).scd41_temperature, \
(sensor_data).scd41_humidity,\
(sensor_data).gps_latitude, \
(sensor_data).gps_longitude, \
(sensor_data).gps_timestamp

// CSV file data line format string
#define CSV_FMT_LINE \
"%" PRIu64 ", " \
"%.9g, %.9g " \
"%.9g, %" PRIu32 ", " \
"%.9g, %.9g, %.9g, " \
"%.9g, %.9g, %.9g, " \
"%" PRIu16 ", %.9g, %.9g, " \
"%.10g, %.10g, %s\n"

// Format string for pretty-printing
#define PRETTY_FMT_LINE \
"---- SENSOR DATA ----\n" \
"millis_since_start: %" PRIu64 "\n" \
"aht21_temperature: %.9g\n" \
"aht21_humidity: %.9g\n" \
"bmp180_temperature: %.9g\n" \
"bmp180_pressure: %" PRIu32 "\n" \
"mpu6050_accel_x: %.9g\n" \
"mpu6050_accel_y: %.9g\n" \
"mpu6050_accel_z: %.9g\n" \
"mpu6050_rot_x: %.9g\n" \
"mpu6050_rot_y: %.9g\n" \
"mpu6050_rot_z: %.9g\n" \
"scd41_co2: %" PRIu16 "\n" \
"scd41_temperature: %.9g\n" \
"scd41_humidity: %.9g\n" \
"gps_latitude: %.9g\n" \
"gps_longitude: %.9g\n" \
"gps_timestamp: %s\n" \
"---------------------\n"

// Initializes all the sensors on the PHAT-3 board.
esp_err_t sensors_init(void);

// Reads all data from all sensors.
//
// Must wait at least 5 seconds between calls.
esp_err_t sensors_read(sensors_data_t* sensor_data_ret);

// Appends a line containing `sensor_data` to the file called `csv_path`.
// If there's no file called `csv_path`, creates one, and adds the proper CSV header.
esp_err_t sensors_save_to_csv(const sensors_data_t* sensor_data, const char* csv_path);

// Pretty-prints `sensor_data` using `ESP_LOGI`.
void sensors_log_data(const sensors_data_t* sensor_data);
