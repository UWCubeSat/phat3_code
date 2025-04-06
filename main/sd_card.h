#pragma once

#include <esp_err.h>

// Creates a new, unused, empty directory on the SD card, and
// returns the path to it.
//
// If more than 100 such folders already exist on the SD card,
// this function will fail.
// The SD card must be formatted with FAT32.
esp_err_t sd_card_mount(char* ret_dir_path, size_t ret_dir_path_size);

// Unmounts the SD card filesystem.
esp_err_t sd_card_unmount(void);