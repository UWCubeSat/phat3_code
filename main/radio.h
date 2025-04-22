#pragma once
#include <sx126x_hal.h>


// Initializes the GPIO pins used for the sx126x.
// Note: implemented in sx126x_hal.c instead of radio.c
sx126x_hal_status_t sx126x_hal_init(void);