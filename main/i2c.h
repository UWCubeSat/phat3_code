#pragma once
#include <driver/i2c_master.h>
#include <esp_err.h>

#define I2C_TIMEOUT 1000

// Returns a `ret_handle` to I2C slave with `address_7bit`
esp_err_t i2c_add_device(uint16_t address_7bit, i2c_master_dev_handle_t *ret_handle);