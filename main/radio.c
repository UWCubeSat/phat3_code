#include "radio.h"
#include "sx126x_hal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <sx126x.h>

#define TAG "radio"

#define RF_FREQUENCY        915000000
#define TX_OUTPUT_POWER     22
#define LORA_BANDWIDTH      SX126X_LORA_BW_125
#define LORA_SPREADING_FACTOR SX126X_LORA_SF12
#define LORA_CODINGRATE     SX126X_LORA_CR_4_8
#define LORA_PREAMBLE_LENGTH 12
#define SX126X_RAMP_TIME_200_US 0x04
#define LORA_PAYLOAD_MAX_LEN 255

static const void* context = NULL;

extern sx126x_hal_status_t sx126x_hal_init(void);

esp_err_t lora_init(void) {
    static const void* context = NULL;

    sx126x_hal_init(); // Your implementation sets up GPIO + SPI

    sx126x_hal_reset(context);
    vTaskDelay(pdMS_TO_TICKS(20));
    sx126x_hal_wakeup(context);
    vTaskDelay(pdMS_TO_TICKS(20));

    sx126x_set_standby(context, SX126X_STANDBY_CFG_RC);
    sx126x_set_pkt_type(context, SX126X_PKT_TYPE_LORA);
    sx126x_set_lora_sync_word(context, 0xA4);
    sx126x_set_rf_freq(context, RF_FREQUENCY);
    sx126x_set_tx_params(context, TX_OUTPUT_POWER, SX126X_RAMP_TIME_200_US);

    sx126x_mod_params_lora_t mod_params = {
        .sf = LORA_SPREADING_FACTOR,
        .bw = LORA_BANDWIDTH,
        .cr = LORA_CODINGRATE,
        .ldro = 1
    };
    sx126x_set_lora_mod_params(context, &mod_params);

    sx126x_pkt_params_lora_t pkt_params = {
        .preamble_len_in_symb = LORA_PREAMBLE_LENGTH,
        .header_type = SX126X_LORA_PKT_EXPLICIT,
        .pld_len_in_bytes = LORA_PAYLOAD_MAX_LEN,
        .crc_is_on = true,
        .invert_iq_is_on = false
    };
    sx126x_set_lora_pkt_params(context, &pkt_params);

    ESP_LOGI(TAG, "LoRa radio initialized");

    return ESP_OK;
}

esp_err_t lora_transmit(const uint8_t* data, uint8_t len) {
    // Write payload to radio buffer
    sx126x_write_buffer(context, 0x00, data, len);

    // Set the IRQs
    sx126x_set_dio_irq_params(context, SX126X_IRQ_TX_DONE, SX126X_IRQ_TX_DONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE);

    // Tell the radio to transmit
    sx126x_set_tx(context, 0);  // 0 = transmit immediately with no timeout

    // Wait for TX to complete
    sx126x_irq_mask_t irq_status;
    do {
        sx126x_get_irq_status(context, &irq_status);
        vTaskDelay(pdMS_TO_TICKS(10));
    } while ((irq_status & SX126X_IRQ_TX_DONE) == 0);

    sx126x_clear_irq_status(context, SX126X_IRQ_ALL);

    ESP_LOGI(TAG, "LoRa TX complete: %.*s", len, data);
    return ESP_OK;
}

esp_err_t lora_receive(uint8_t* buffer, uint8_t* len_out) {
    if (!buffer || !len_out) return ESP_ERR_INVALID_ARG;

    sx126x_set_dio_irq_params(context,
        SX126X_IRQ_RX_DONE | SX126X_IRQ_TIMEOUT,
        SX126X_IRQ_RX_DONE | SX126X_IRQ_TIMEOUT,
        SX126X_IRQ_NONE, SX126X_IRQ_NONE);
    sx126x_set_rx(context, 0);  // 0 = timeout disabled

    sx126x_irq_mask_t irq_status;
    do {
        sx126x_get_irq_status(context, &irq_status);
        vTaskDelay(pdMS_TO_TICKS(10));
    } while ((irq_status & (SX126X_IRQ_RX_DONE | SX126X_IRQ_TIMEOUT)) == 0);

    sx126x_clear_irq_status(context, SX126X_IRQ_ALL);

    if (irq_status & SX126X_IRQ_TIMEOUT) {
        ESP_LOGW(TAG, "LoRa RX timeout");
        return ESP_ERR_TIMEOUT;
    }

    sx126x_rx_buffer_status_t rx_status;
    sx126x_get_rx_buffer_status(context, &rx_status);
    
    *len_out = rx_status.pld_len_in_bytes;
    sx126x_read_buffer(context, rx_status.buffer_start_pointer, buffer, *len_out);

    sx126x_read_buffer(context, rx_status.buffer_start_pointer, buffer, *len_out);
    ESP_LOGI(TAG, "LoRa RX received (%d bytes): %.*s", *len_out, *len_out, buffer);
    return ESP_OK;
}