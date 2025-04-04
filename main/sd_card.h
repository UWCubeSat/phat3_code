#pragma once

#include <esp_err.h>

// Creates a new, unused, empty file on the SD card, and
// sets `ret_fd` to its file descriptor.
//
// If more than 100 such files already exist on the SD card,
// this function will fail.
// The SD card must be formatted with FAT32.
esp_err_t init_sd_card(int* ret_fd);