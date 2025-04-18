#include <driver/uart.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <esp_check.h>
#include <esp_log.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <nmea.h>
#include <gpgll.h>
#include <gpgsa.h>
#include <gpvtg.h>
#include <gptxt.h>
#include <gpgsv.h>

#define LOG_TAG "gt-u7"

#define UART_BUF_SIZE (4 * UART_HW_FIFO_LEN(UART_NUM_1))

#define GPIO_TX 7
#define GPIO_RX 6

esp_err_t gps_get_sample(void) {
    esp_err_t ret;
    int res;
    uint8_t reply[6];

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

    ESP_RETURN_ON_ERROR(uart_flush_input(UART_NUM_2), LOG_TAG, "Couldn't flush input.");

    char read_bytes[256];
    while (true) {
        //uart_enable_pattern_det_baud_intr(UART_NUM_2, "$", 00100100, );
        //int position = uart_pattern_get_pos(UART_NUM_2);
        int len = uart_read_bytes(UART_NUM_2, read_bytes, sizeof(read_bytes) - 1, pdMS_TO_TICKS(10000));
        read_bytes[len] = '\0';  // Null-terminate

        char* message_start = memchr(read_bytes, '$', sizeof(read_bytes) - 1);
        if (message_start == NULL) {
            ESP_LOGE(LOG_TAG, "FOO WHAT ARE YOU DOING!");
            abort();
        }

        char* message_end = memchr(message_start + 1, '$', sizeof(read_bytes) - (message_start - read_bytes));
        if (message_end == NULL) {
            ESP_LOGE(LOG_TAG, "FOO WHAT ARE YOU DOING!");
            abort();
        }

        *message_end = '\0';
        
        ESP_LOGI(LOG_TAG, "Length of the read bytes: %d", len);
        ESP_LOGI(LOG_TAG, "Here's the string: %s", message_start);

        esp_log_buffer_hex(LOG_TAG, message_start, message_end - message_start);
        
        // Parse...
        nmea_s *data = nmea_parse(message_start, message_end - message_start, 0);

        if(NULL == data) {
            printf("Failed to parse sentence!\n");
        }

        if (NMEA_GPGLL == data->type) {
            nmea_gpgll_s *gpgll = (nmea_gpgll_s *) data;

            printf("GPGLL Sentence\n");
            printf("Longitude:\n");
            printf("  Degrees: %d\n", gpgll->longitude.degrees);
            printf("  Minutes: %f\n", gpgll->longitude.minutes);
            printf("  Cardinal: %c\n", (char) gpgll->longitude.cardinal);
            printf("Latitude:\n");
            printf("  Degrees: %d\n", gpgll->latitude.degrees);
            printf("  Minutes: %f\n", gpgll->latitude.minutes);
            printf("  Cardinal: %c\n", (char) gpgll->latitude.cardinal);
        }

        if (NMEA_GPGSA == data->type) {
            nmea_gpgsa_s *gpgsa = (nmea_gpgsa_s *) data;

            printf("GPGSA Sentence:\n");
            printf("\tMode: %c\n", gpgsa->mode);
            printf("\tFix:  %d\n", gpgsa->fixtype);
            printf("\tPDOP: %.2lf\n", gpgsa->pdop);
            printf("\tHDOP: %.2lf\n", gpgsa->hdop);
            printf("\tVDOP: %.2lf\n", gpgsa->vdop);
        }

        if (NMEA_GPVTG == data->type) {
            nmea_gpvtg_s *gpvtg = (nmea_gpvtg_s *) data;

            printf("GPVTG Sentence:\n");
            printf("\tTrack [deg]:   %.2lf\n", gpvtg->track_deg);
            printf("\tSpeed [kmph]:  %.2lf\n", gpvtg->gndspd_kmph);
            printf("\tSpeed [knots]: %.2lf\n", gpvtg->gndspd_knots);
        }

        if (NMEA_GPTXT == data->type) {
            nmea_gptxt_s *gptxt = (nmea_gptxt_s *) data;

            printf("GPTXT Sentence:\n");
            printf("\tID: %d %d %d\n", gptxt->id_00, gptxt->id_01, gptxt->id_02);
            printf("\t%s\n", gptxt->text);
        }

        if (NMEA_GPGSV == data->type) {
            nmea_gpgsv_s *gpgsv = (nmea_gpgsv_s *) data;

            printf("GPGSV Sentence:\n");
            printf("\tNum: %d\n", gpgsv->sentences);
            printf("\tID:  %d\n", gpgsv->sentence_number);
            printf("\tSV:  %d\n", gpgsv->satellites);
            printf("\t#1:  %d %d %d %d\n", gpgsv->sat[0].prn, gpgsv->sat[0].elevation, gpgsv->sat[0].azimuth, gpgsv->sat[0].snr);
            printf("\t#2:  %d %d %d %d\n", gpgsv->sat[1].prn, gpgsv->sat[1].elevation, gpgsv->sat[1].azimuth, gpgsv->sat[1].snr);
            printf("\t#3:  %d %d %d %d\n", gpgsv->sat[2].prn, gpgsv->sat[2].elevation, gpgsv->sat[2].azimuth, gpgsv->sat[2].snr);
            printf("\t#4:  %d %d %d %d\n", gpgsv->sat[3].prn, gpgsv->sat[3].elevation, gpgsv->sat[3].azimuth, gpgsv->sat[3].snr);
        }

        nmea_free(data);
    }


}