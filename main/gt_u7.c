#include <gt_u7.h>
#include <driver/uart.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <esp_check.h>
#include <esp_log.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <nmea.h>
#include <gpgll.h>
#include <gpgsa.h>
#include <gpvtg.h>
#include <gptxt.h>
#include <gpgsv.h>

#define LOG_TAG "gt-u7"

#define UART_BUF_SIZE 1024

#define GPIO_TX 7
#define GPIO_RX 6

// Buffer for receiving NMEA from GPS.
// A max NMEA sentence is 82 characters,
// so this buffer can hold 2.
static char read_buf[256];

// Fills `read_buf` with the next NMEA sentence string
static esp_err_t read_next_nmea_sentence(void) {
    // Get to the start of the next sentence
    read_buf[0] = '\0';
    while (read_buf[0] != '$') {
        int len = uart_read_bytes(UART_NUM_2, &read_buf, 1, pdMS_TO_TICKS(2000));
        ESP_RETURN_ON_FALSE(len == 1, ESP_ERR_TIMEOUT, LOG_TAG, "Timed out reading from GPS");
    }

    int bytes_read = 1;
    while (read_buf[bytes_read - 1] != '\n') {
        int len = uart_read_bytes(UART_NUM_2, read_buf + bytes_read, 1, pdMS_TO_TICKS(2000));
        ESP_RETURN_ON_FALSE(len == 1, ESP_ERR_TIMEOUT, LOG_TAG, "Timed out reading from GPS");
        bytes_read += 1;
        if (bytes_read >= sizeof(read_buf) - 1) {
            ESP_LOGE(LOG_TAG, "Couldn't read NMEA sentence from GPS");
            return ESP_ERR_NO_MEM;
        }
    }

    read_buf[bytes_read] = '\0';
    return ESP_OK;
}

esp_err_t gt_u7_get_location(gt_u7_data_t* gt_u7_data_ret) {
    esp_err_t ret;

    // Start the driver if it hasn't been already
    if (!uart_is_driver_installed(UART_NUM_2)) {
        uart_config_t uart_config = {
            .baud_rate = 9600,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
            .stop_bits = UART_STOP_BITS_1,
        };
        ret = uart_driver_install(UART_NUM_2, UART_BUF_SIZE, UART_BUF_SIZE, 10, NULL, 0);
        ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't install UART driver");
        ret = uart_param_config(UART_NUM_2, &uart_config);
        ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't set UART config");
        ret = uart_set_pin(UART_NUM_2, GPIO_TX, GPIO_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't configure UART GPIO pins");
    }

    // Read the next NMEA_GPGLL sentence
    nmea_s* nmea_data = NULL;
    ESP_RETURN_ON_ERROR(uart_flush_input(UART_NUM_2), LOG_TAG, "Couldn't flush input.");
    while (nmea_data == NULL || nmea_data->type != NMEA_GPGLL) {
        nmea_free(nmea_data);
        ESP_RETURN_ON_ERROR(read_next_nmea_sentence(), LOG_TAG, "Couldn't read NMEA sentence");
        nmea_data = nmea_parse(read_buf, strlen(read_buf), 1);
        ESP_RETURN_ON_FALSE(nmea_data, ESP_FAIL, LOG_TAG, "Couldn't parse NMEA sentence");

    }

    nmea_gpgll_s* gpgll_data = (nmea_gpgll_s *) nmea_data;
    gt_u7_data_ret->gps_latitude = gpgll_data->latitude.degrees + gpgll_data->latitude.minutes / 60;
    gt_u7_data_ret->gps_longitude = gpgll_data->longitude.degrees + gpgll_data->longitude.minutes / 60;

    if (gpgll_data->latitude.cardinal == 'S') {
        gt_u7_data_ret->gps_latitude *= -1;
    } else if (gpgll_data->latitude.cardinal == 'N') {
        gt_u7_data_ret->gps_latitude *= 1;
    } else {
        ESP_LOGE(LOG_TAG, "Couldn't read latitude from GPS");
        return ESP_FAIL;
    }

    if (gpgll_data->longitude.cardinal == 'W') {
        gt_u7_data_ret->gps_longitude *= -1;
    } else if (gpgll_data->longitude.cardinal == 'E') {
        gt_u7_data_ret->gps_longitude *= 1;
    } else {
        ESP_LOGE(LOG_TAG, "Couldn't read longitude from GPS");
        return ESP_FAIL;
    }

    size_t res = strftime(
        gt_u7_data_ret->gps_timestamp,
        sizeof(gt_u7_data_ret->gps_timestamp),
        "%Y-%m-%dT%H:%M:%SZ", &gpgll_data->time);
    
    ESP_RETURN_ON_FALSE(res != 0, ESP_ERR_NO_MEM, LOG_TAG, "strftime error: %s", strerror(errno));

    return ESP_OK;

    // printf("GPGLL Sentence\n");
    // printf("Longitude:\n");
    // printf("  Degrees: %d\n", gpgll->longitude.degrees);
    // printf("  Minutes: %f\n", gpgll->longitude.minutes);
    // printf("  Cardinal: %c\n", (char) gpgll->longitude.cardinal);
    // printf("Latitude:\n");
    // printf("  Degrees: %d\n", gpgll->latitude.degrees);
    // printf("  Minutes: %f\n", gpgll->latitude.minutes);
    // printf("  Cardinal: %c\n", (char) gpgll->latitude.cardinal);
}