#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <scd41/scd4x_i2c.h>
#include <scd41/sensirion_i2c_hal.h>

#define SDA_PIN 42
#define SCL_PIN 41

static const char *TAG = "SCD41";

void app_main(void) {
    sensirion_i2c_hal_init(SDA_PIN, SCL_PIN);
    scd4x_init();

    int16_t err = scd4x_start_periodic_measurement();
    if (err) {
        ESP_LOGE(TAG, "SCD41 start measurement failed: %d", err);
        return;
    }

    ESP_LOGI(TAG, "SCD41 started.");

    while (1) {
        bool data_ready = false;
        err = scd4x_get_data_ready_flag(&data_ready);
        if (err) {
            ESP_LOGW(TAG, "Failed to get data-ready flag: %d", err);
            continue;
        }

        if (data_ready) {
            uint16_t co2 = 0;
            float temperature = 0, humidity = 0;
            err = scd4x_read_measurement(&co2, &temperature, &humidity);
            if (err == 0) {
                ESP_LOGI(TAG, "CO2: %u ppm", co2);
                ESP_LOGI(TAG, "Temp: %.2f °C", temperature);
                ESP_LOGI(TAG, "Humidity: %.2f %%", humidity);
            } else {
                ESP_LOGW(TAG, "Read failed: %d", err);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}