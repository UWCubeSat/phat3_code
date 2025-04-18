#include <sensors.h>
#include <esp_check.h>
#include <driver/i2c_master.h>

#include <aht.h>
#include <bmp180.h>
#include <mpu6050.h>
#include <scd4x.h>

// Tag used for logging
#define LOG_TAG "sensors"

const i2c_port_t I2C_PORT = I2C_NUM_1;
const gpio_num_t SDA = GPIO_NUM_42;
const gpio_num_t SCL = GPIO_NUM_41;

static aht_t aht_dev;
static bmp180_dev_t bmp180_dev;
static mpu6050_dev_t mpu6050_dev;
static i2c_dev_t scd41_dev;



esp_err_t init_sensors(void) {
    esp_err_t err;

    err = aht_init_desc(&aht_dev, AHT_I2C_ADDRESS_VCC, I2C_PORT, SDA, SCL);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    err = aht_init(&aht_dev);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    
    bool is_busy, is_calibrated;
    err = aht_get_status(&aht_dev, &is_busy, &is_calibrated);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    ESP_RETURN_ON_FALSE(is_busy == false, ESP_FAIL, LOG_TAG, "Couldn't init sensor");
    ESP_RETURN_ON_FALSE(is_busy == true, ESP_FAIL, LOG_TAG, "Couldn't init sensor");


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


    err = scd4x_init_desc(&scd41_dev, I2C_PORT, SDA, SCL);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    err = scd4x_start_periodic_measurement(&scd41_dev);
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't init sensor");
    // TODO: calibrate?

    return ESP_OK;
}

esp_err_t read_all_sensor_data(sensors_data_t* sensor_data_ret) {
    esp_err_t err;

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

    err = scd4x_read_measurement(
        &scd41_dev,
        &(sensor_data_ret->scd41_co2),
        &(sensor_data_ret->scd41_temperature),
        &(sensor_data_ret->scd41_humidity)
    );
    ESP_RETURN_ON_ERROR(err, LOG_TAG, "Couldn't read sensor data");

    return ESP_OK;
}

esp_err_t save_sensor_data_csv(const sensors_data_t* sensor_data, FILE* data_csv) {
    // todo: implement this!
    return ESP_FAIL;
}