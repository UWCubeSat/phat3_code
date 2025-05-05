#pragma once

#include <esp_err.h>

typedef struct {
    double gps_latitude;
    double gps_longitude;
    char gps_timestamp[30];
} gt_u7_data_t;

// Gets the current location from the GT-U7 GPS.
esp_err_t gt_u7_get_location(gt_u7_data_t* gt_u7_data_ret);