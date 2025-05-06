#include <sensors.h>
#include <esp_check.h>
#include <driver/i2c_master.h>
#include <esp_timer.h>
#include <errno.h>
#include <string.h>

#include <aht.h>
#include <bmp180.h>
#include <mpu6050.h>
#include <scd4x.h>

#include <gt_u7.h>

// Tag used for logging
#define LOG_TAG "sensors"

const i2c_port_t I2C_PORT = I2C_NUM_1;
const gpio_num_t SDA = GPIO_NUM_42;
const gpio_num_t SCL = GPIO_NUM_41;

static aht_t aht_dev;
static bmp180_dev_t bmp180_dev;
static mpu6050_dev_t mpu6050_dev;
static i2c_dev_t scd41_dev;

// Temporarily stores CSV line before
// appended to file
static char csv_line_buf[2048];


esp_err_t sensors_init(void) {
    esp_err_t err;

    // // Initialize the I2C driver
    // i2c_config_t i2c_conf = {
    //     .mode = I2C_MODE_MASTER,
    //     .sda_io_num = SDA,
    //     .scl_io_num = SCL,
    //     .sda_pullup_en = GPIO_PULLUP_ENABLE,
    //     .scl_pullup_en = GPIO_PULLUP_ENABLE,
    //     .master.clk_speed = 10 * 1000,
    //     .clk_flags = 0,  // use default
    // };

    // err = i2c_param_config(I2C_PORT, &i2c_conf);
    // ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init I2C driver");

    // err = i2c_driver_install(I2C_PORT, i2c_conf.mode, 0, 0, 0);
    // ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init I2C driver");

    err = i2cdev_init();
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init I2C library");

    // Initialize the sensors

    err = aht_init_desc(&aht_dev, AHT_I2C_ADDRESS_GND, I2C_PORT, SDA, SCL);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    err = aht_init(&aht_dev);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    
    bool is_busy, is_calibrated;
    err = aht_get_status(&aht_dev, &is_busy, &is_calibrated);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    ESP_RETURN_ON_FALSE(is_busy == false, ESP_FAIL, LOG_TAG, "Couldn't init sensor");

    err = bmp180_init_desc(&bmp180_dev, I2C_PORT, SDA, SCL);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    err = bmp180_init(&bmp180_dev);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");


    err = mpu6050_init_desc(&mpu6050_dev, MPU6050_I2C_ADDRESS_LOW, I2C_PORT, SDA, SCL);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    err = mpu6050_init(&mpu6050_dev);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    // TODO: Add more mpu6050 initialization/calibration?
    // TODO: Add mpu6050 averaging or quicker polling?

    // TODO: Get the CO2 sensor to work below.

    // err = scd4x_init_desc(&scd41_dev, I2C_PORT, SDA, SCL);
    // ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    // vTaskDelay(pdMS_TO_TICKS(1000)); // TODO: is this needed?
    // err = scd4x_start_periodic_measurement(&scd41_dev);
    // ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    // // TODO: calibrate?

    return ESP_OK;
}

esp_err_t sensors_read(sensors_data_t* sensor_data_ret) {
    esp_err_t err;

    uint64_t millis = esp_timer_get_time() / 1000;
    sensor_data_ret->millis_since_start = millis;

    err = aht_get_data(
        &aht_dev,
        &(sensor_data_ret->aht21_temperature),
        &(sensor_data_ret->aht21_humidity)
    );
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't read sensor data");

    err = bmp180_measure(
        &bmp180_dev,
        &(sensor_data_ret->bmp180_temperature),
        &(sensor_data_ret->bmp180_pressure),
        BMP180_MODE_ULTRA_HIGH_RESOLUTION
    );
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't read sensor data");

    mpu6050_acceleration_t mpu6050_accel;
    mpu6050_rotation_t mpu6050_rot;
    err = mpu6050_get_motion(
        &mpu6050_dev,
        &mpu6050_accel,
        &mpu6050_rot
    );
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't read sensor data");
    sensor_data_ret->mpu6050_accel_x = mpu6050_accel.x;
    sensor_data_ret->mpu6050_accel_y = mpu6050_accel.y;
    sensor_data_ret->mpu6050_accel_z = mpu6050_accel.z;
    sensor_data_ret->mpu6050_rot_x = mpu6050_rot.x;
    sensor_data_ret->mpu6050_rot_y = mpu6050_rot.y;
    sensor_data_ret->mpu6050_rot_z = mpu6050_rot.z;

    // err = scd4x_read_measurement(
    //     &scd41_dev,
    //     &(sensor_data_ret->scd41_co2),
    //     &(sensor_data_ret->scd41_temperature),
    //     &(sensor_data_ret->scd41_humidity)
    // );
    // ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't read sensor data");

    // gt_u7_data_t gps_data;
    // err = gt_u7_get_location(&gps_data);
    // ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't read sensor data");
    // sensor_data_ret->gps_latitude = gps_data.gps_latitude;
    // sensor_data_ret->gps_longitude = gps_data.gps_longitude;
    // strncpy(
    //     sensor_data_ret->gps_timestamp,
    //     gps_data.gps_timestamp,
    //     sizeof(sensor_data_ret->gps_timestamp)
    // );

    return ESP_OK;
}

esp_err_t sensors_save_to_csv(const sensors_data_t* sensor_data, const char* csv_path) {
    int res;

    // Write sensor data to string
    res = snprintf(
        csv_line_buf, sizeof(csv_line_buf), CSV_FMT_LINE,
        SENSOR_DATA_EXPAND(*sensor_data)
    );
    if (res == -1 || res >= sizeof(csv_line_buf)) {
        return ESP_ERR_NO_MEM;
    }

    FILE* file = fopen(csv_path, "a");
    ESP_RETURN_ON_FALSE(file != NULL, ESP_FAIL, LOG_TAG, "Couldn't append to CSV file. %s", strerror(errno));

    // If we just created this file, write the header
    if (ftell(file) == 0) {
        res = fwrite(CSV_HEADER_LINE, sizeof(CSV_HEADER_LINE) - 1, 1, file);
        ESP_RETURN_ON_FALSE(res == 1, ESP_FAIL, LOG_TAG, "CSV file err: %s", strerror(errno));
    }

    // Write the line to the file
    res = fwrite(csv_line_buf, res, 1, file);
    ESP_RETURN_ON_FALSE(res == 1, ESP_FAIL, LOG_TAG, "CSV file err: %s", strerror(errno));

    res = fclose(file);
    ESP_RETURN_ON_FALSE(res == 0, ESP_FAIL, LOG_TAG, "CSV file err: %s", strerror(errno));

    return ESP_OK;
}

void sensors_log_data(const sensors_data_t* sensor_data) {
    ESP_LOGI(LOG_TAG, PRETTY_FMT_LINE, SENSOR_DATA_EXPAND(*sensor_data));
}