#pragma once

#include "esp_err.h"

// Initializes I2C and the SCD41 sensor
esp_err_t scd41_init(void);

// Checks if new data is ready
esp_err_t scd41_data_ready(bool *ready);

// Reads CO2 (ppm), temperature (°C), and humidity (%RH)
esp_err_t scd41_read(uint16_t *co2, float *temperature, float *humidity);

