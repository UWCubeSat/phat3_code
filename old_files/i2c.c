#include <driver/i2c_master.h>
#include <esp_check.h>

#define LOG_TAG "i2c"

static i2c_master_bus_handle_t i2c_master_handle = NULL;

esp_err_t i2c_add_device(uint16_t address_7bit, i2c_master_dev_handle_t *ret_handle) {
    esp_err_t ret;

    // initialize the bus if it isn't already
    if (i2c_master_handle == NULL) {
        i2c_master_bus_config_t i2c_mst_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = -1,
            .scl_io_num = 41,
            .sda_io_num = 42,
            .glitch_ignore_cnt = 7,
        };
    
        ret = i2c_new_master_bus(&i2c_mst_config, &i2c_master_handle);
        ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't create new master bus.");
    }
    
    // Configure device
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address_7bit,
        .scl_speed_hz = 100000,
    };

    // add the device
    ret = i2c_master_bus_add_device(i2c_master_handle, &dev_cfg, ret_handle);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't add device to bus");

    return ESP_OK;
}