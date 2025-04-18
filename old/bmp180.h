#pragma once

esp_err_t i2c_master_init(void);

int16_t read_raw_temperature(void);

int32_t read_raw_pressure(void);