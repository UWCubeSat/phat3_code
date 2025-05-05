#include "radio.h"
#include "sx126x_hal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_check.h>
#include <string.h>
#include <sx126x.h>

#define LOG_TAG "radio"

// TODO: Choose a frequency
#define RF_FREQUENCY        915000000
// TODO: How high does this go?
#define TX_OUTPUT_POWER     22
#define LORA_BANDWIDTH      SX126X_LORA_BW_125
#define LORA_SPREADING_FACTOR SX126X_LORA_SF12
#define LORA_CODINGRATE     SX126X_LORA_CR_4_8
#define LORA_PREAMBLE_LENGTH 12
#define LORA_RAMP_TIME SX126X_RAMP_200_US
#define LORA_PAYLOAD_MAX_LEN 255

static const void* context = NULL;

extern sx126x_hal_status_t sx126x_hal_init(void);

esp_err_t radio_init(void) {
    sx126x_status_t status;

    // Set up SPI
    status = sx126x_hal_init();
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio init fail");
    
    status = sx126x_hal_reset(context);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio init fail");
    vTaskDelay(pdMS_TO_TICKS(20));
    
    status = sx126x_hal_wakeup(context);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio init fail");
    vTaskDelay(pdMS_TO_TICKS(20));

    status = sx126x_set_standby(context, SX126X_STANDBY_CFG_RC);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio init fail");
    
    status = sx126x_set_pkt_type(context, SX126X_PKT_TYPE_LORA);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio init fail");
    
    status = sx126x_set_lora_sync_word(context, 0xA4);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio init fail");
    
    status = sx126x_set_rf_freq(context, RF_FREQUENCY);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio init fail");
    
    status = sx126x_set_tx_params(context, TX_OUTPUT_POWER, LORA_RAMP_TIME);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio init fail");

    sx126x_mod_params_lora_t mod_params = {
        .sf = LORA_SPREADING_FACTOR,
        .bw = LORA_BANDWIDTH,
        .cr = LORA_CODINGRATE,
        .ldro = 1
    };
    status = sx126x_set_lora_mod_params(context, &mod_params);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio init fail");

    sx126x_pkt_params_lora_t pkt_params = {
        .preamble_len_in_symb = LORA_PREAMBLE_LENGTH,
        .header_type = SX126X_LORA_PKT_EXPLICIT,
        .pld_len_in_bytes = LORA_PAYLOAD_MAX_LEN,
        .crc_is_on = true,
        .invert_iq_is_on = false
    };
    status = sx126x_set_lora_pkt_params(context, &pkt_params);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio init fail");

    return ESP_OK;
}

esp_err_t radio_transmit(const uint8_t* data, uint8_t len) {
    sx126x_status_t status;

    // Write payload to radio buffer
    status = sx126x_write_buffer(context, 0x00, data, len);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");

    // Set the IRQs
    status = sx126x_set_dio_irq_params(context, SX126X_IRQ_TX_DONE, SX126X_IRQ_TX_DONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");

    // Tell the radio to transmit
    status = sx126x_set_tx(context, 0);  // 0 = transmit immediately with no timeout
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");

    // Wait for TX to complete
    sx126x_irq_mask_t irq_status;
    do {
        status = sx126x_get_irq_status(context, &irq_status);
        ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");
        vTaskDelay(pdMS_TO_TICKS(10));
    } while ((irq_status & SX126X_IRQ_TX_DONE) == 0);

    status = sx126x_clear_irq_status(context, SX126X_IRQ_ALL);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");

    ESP_LOGI(LOG_TAG, "LoRa TX complete: %.*s", len, data);
    return ESP_OK;
}

esp_err_t radio_receive(uint8_t* buffer, uint8_t* len_out) {
    sx126x_status_t status;

    if (!buffer || !len_out) return ESP_ERR_INVALID_ARG;

    status = sx126x_set_dio_irq_params(context,
        SX126X_IRQ_RX_DONE | SX126X_IRQ_TIMEOUT,
        SX126X_IRQ_RX_DONE | SX126X_IRQ_TIMEOUT,
        SX126X_IRQ_NONE, SX126X_IRQ_NONE);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");
    
    status = sx126x_set_rx(context, 0);  // 0 = timeout disabled
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");

    sx126x_irq_mask_t irq_status;
    do {
        status = sx126x_get_irq_status(context, &irq_status);
        ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");
        vTaskDelay(pdMS_TO_TICKS(10));
    } while ((irq_status & (SX126X_IRQ_RX_DONE | SX126X_IRQ_TIMEOUT)) == 0);

    status = sx126x_clear_irq_status(context, SX126X_IRQ_ALL);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");

    if (irq_status & SX126X_IRQ_TIMEOUT) {
        ESP_LOGW(LOG_TAG, "LoRa RX timeout");
        return ESP_ERR_TIMEOUT;
    }

    sx126x_rx_buffer_status_t rx_status;
    status = sx126x_get_rx_buffer_status(context, &rx_status);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");
    
    *len_out = rx_status.pld_len_in_bytes;
    status = sx126x_read_buffer(context, rx_status.buffer_start_pointer, buffer, *len_out);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");

    status = sx126x_read_buffer(context, rx_status.buffer_start_pointer, buffer, *len_out);
    ESP_RETURN_ON_FALSE(status == 0, ESP_FAIL, LOG_TAG, "Radio transmit fail");

    ESP_LOGI(LOG_TAG, "LoRa RX received (%d bytes): %.*s", *len_out, *len_out, buffer);
    return ESP_OK;
}