#pragma once
#include <esp_err.h>

// Save a jpeg photo from the UCAM-III
// to the given directory.
esp_err_t ucam_save_photo(char* save_dir_path);

esp_err_t ucam_get_last_saved_photo(uint8_t** jpeg_buf_out, uint32_t* jpeg_size_out);