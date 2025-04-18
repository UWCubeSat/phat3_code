#include <stdio.h>
#include <i2c.h>

#define I2C_MASTER_SCL_IO 42    // GPIO for SCL
#define I2C_MASTER_SDA_IO 41    // GPIO for SDA
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_NUM I2C_NUM_0
#define BMP180_ADDRESS 0x77
#define BMP180_REG_CONTROL 0xF4
#define BMP180_REG_RESULT 0xF6
#define BMP180_COMMAND_TEMPERATURE 0x2E
#define BMP180_COMMAND_PRESSURE0 0x34

static esp_err_t i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static int16_t read_raw_temperature() {
    uint8_t data[2];
    uint8_t command[] = {BMP180_REG_CONTROL, BMP180_COMMAND_TEMPERATURE};
    i2c_master_write_to_device(I2C_MASTER_NUM, BMP180_ADDRESS, command, sizeof(command), 1000 / portTICK_RATE_MS);
    vTaskDelay(5 / portTICK_RATE_MS); // Wait for measurement
    i2c_master_read_from_device(I2C_MASTER_NUM, BMP180_ADDRESS, data, 2, 1000 / portTICK_RATE_MS);
    return (data[0] << 8) | data[1];
}

static int32_t read_raw_pressure() {
    uint8_t data[3];
    uint8_t command[] = {BMP180_REG_CONTROL, BMP180_COMMAND_PRESSURE0};
    i2c_master_write_to_device(I2C_MASTER_NUM, BMP180_ADDRESS, command, sizeof(command), 1000 / portTICK_RATE_MS);
    vTaskDelay(8 / portTICK_RATE_MS); // Wait for measurement
    i2c_master_read_from_device(I2C_MASTER_NUM, BMP180_ADDRESS, data, 3, 1000 / portTICK_RATE_MS);
    return ((data[0] << 16) | (data[1] << 8) | data[2]) >> (8 - 0);
}
