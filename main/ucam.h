#pragma once
#include <esp_err.h>

// Save a jpeg photo from the UCAM-III
// to the given directory.
esp_err_t ucam_save_photo(char* save_dir_path);