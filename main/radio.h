#pragma once

#include <stdint.h>
#include <esp_err.h>

// TODO

esp_err_t radio_init(void);

esp_err_t radio_send(uint8_t *data, int16_t len);

uint8_t radio_receive(uint8_t *data, int16_t len);