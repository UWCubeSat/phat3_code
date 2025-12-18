// Our headers
#include <sensors.h>
#include <ucam.h>
#include <gps.h>
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

#define SERIAL_DATA_FMT "DATA_PKT;%llu;%.2f;%.2f;%.2f;%lu;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%u;%.2f;%.2f;%.8f;%.8f;%s\n"

static sensors_data_t sensors_data;

void sensors_log_data_compact(sensors_data_t *d) {
    printf(SERIAL_DATA_FMT,
           d->millis_since_start,
           d->aht21_temperature, d->aht21_humidity,
           d->bmp180_temperature, d->bmp180_pressure,
           d->mpu6050_accel_x, d->mpu6050_accel_y, d->mpu6050_accel_z,
           d->mpu6050_rot_x, d->mpu6050_rot_y, d->mpu6050_rot_z,
           d->scd41_co2, d->scd41_temperature, d->scd41_humidity,
           d->gps_latitude, d->gps_longitude,
           d->gps_timestamp
    );
}

// TODO: Make this interrupt based? - ra01s library doesn't seem to support this though
static void radio_receive_task(void* pvParameters) {
    uint8_t bytes;
    TickType_t last_wake_time = xTaskGetTickCount();

    while (true) {
        bytes = radio_receive((uint8_t*) &sensors_data, sizeof(sensors_data));
        if (bytes == sizeof(sensors_data)) {
            sensors_log_data_compact(&sensors_data);
        } else if (bytes != 0) {
            ESP_LOGW(LOG_TAG, "Corrupted radio packet: expected %d bytes, got %d",
                sizeof(sensors_data), bytes);
        }
        // if bytes == 0, no data received

        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100)); 
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(radio_init());
    
    // Spawn threads
    xTaskCreate(radio_receive_task, "radio_receive_task", 4096, NULL, 2, NULL);
    
    ESP_LOGI(LOG_TAG, "System start successful.");
}
