// Our headers
#include <sensors.h>
#include <ucam.h>
#include <gt_u7.h>
#include <sd_card.h>

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

void app_main(void)
{
    ESP_ERROR_CHECK(gps_get_sample());


}