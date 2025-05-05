// Our headers
#include <sensors.h>
#include <ucam.h>
#include <gt_u7.h>
#include <sd_card.h>
#include <radio.h>

// ESP-IDF headers
#include <stdio.h>
#include <driver/gpio.h>
#include <soc/gpio_sig_map.h>
#include <esp_check.h>
#include <fcntl.h>
#include <unistd.h>
#include <esp_task.h>

// Tag used for logging
#define LOG_TAG "main"

static char photo_dir[64];
static char sensor_csv_path[64];
static sensors_data_t sensors_data;


static void sensor_task(void* pvParameters) {
    esp_err_t err;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (true) {
        err = sensors_read(&sensors_data);
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        
        err = sensors_save_to_csv(&sensors_data, sensor_csv_path);
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        
        err = radio_transmit((uint8_t*) &sensors_data, sizeof(sensors_data));
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10000));
    }
}

static void camera_task(void* pvParameters) {
    esp_err_t err;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (true) {
        err = ucam_save_photo(photo_dir);
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10000)); 
    }
}

void app_main(void) {
    // Create files on SD card
    int res;
    char sd_path[64];
    ESP_ERROR_CHECK(sd_card_mount(sd_path, sizeof(sd_path)));
    res = snprintf(photo_dir, sizeof(photo_dir), "%s/img/", sd_path);
    if (res == -1 || res >= sizeof(photo_dir)) {
        ESP_LOGE(LOG_TAG, "Couldn't create folder path");
        abort();
    }
    res = snprintf(sensor_csv_path, sizeof(sensor_csv_path), "%s/data.csv", sd_path);
    if (res == -1 || res >= sizeof(sensor_csv_path)) {
        ESP_LOGE(LOG_TAG, "Couldn't create folder path");
        abort();
    }

    ESP_ERROR_CHECK(sensors_init());
    ESP_ERROR_CHECK(radio_init());
    
    // Spawn threads
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 1, NULL);
    xTaskCreate(camera_task, "camera_task", 4096, NULL, 1, NULL);
    
    ESP_LOGI(LOG_TAG, "System start successful.");
}
