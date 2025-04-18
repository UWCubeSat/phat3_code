#include <esp_check.h>
#include <i2c.h>
#include <timer.h>

#define LOG_TAG "aht21"

static i2c_master_dev_handle_t device;

const uint8_t init_command[3] = {0xBE, 0x08, 0x00};

const uint8_t check_status_command[1] = {0x71};

esp_err_t init_aht21(void) {
    esp_err_t ret;
    ret = i2c_add_device(0x38, &device);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't add I2C device");

    // Based on: http://aosong.com/userfiles/files/media/AHT21%20%E8%8B%B1%E6%96%87%E7%89%88%E8%AF%B4%E6%98%8E%E4%B9%A6%20A1%2020201222.pdf
    
    ret = i2c_master_transmit(device, init_command, sizeof(init_command), 1000);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't send i2c command.");

    return ESP_OK;
}

esp_err_t check_aht21_status(void) {
    esp_err_t ret = i2c_master_transmit(device, check_status_command, sizeof(check_status_command), 1000);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't send i2c command.");

    uint8_t response_buffer[1];

    ret = i2c_master_transmit_receive(
        device,
        check_status_command,
        sizeof(check_status_command),
        response_buffer,
        sizeof(response_buffer),
        5
    );
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't send i2c command.");


}