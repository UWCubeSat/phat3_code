#pragma once
#include <sx126x_hal.h>

#include <esp_err.h>


// Initialize the radio
esp_err_t radio_init(void);

// Transmit `data`
esp_err_t radio_transmit(const uint8_t* data, uint8_t len);

// Fills `buffer` with the next packet
esp_err_t radio_receive(uint8_t* buffer, uint8_t* len_out);