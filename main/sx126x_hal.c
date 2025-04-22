#include <sx126x_hal.h>

// This file implements the functions from the
// above library file ^.

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_log.h>
#include <string.h>

#define PIN_NUM_MISO 11
#define PIN_NUM_MOSI 10
#define PIN_NUM_CLK  9
#define PIN_NUM_CS   8
#define PIN_NUM_RST  12
#define PIN_NUM_BUSY 13

static const char* TAG = "sx126x_hal";

static spi_device_handle_t spi_handle;

sx126x_hal_status_t sx126x_hal_init(void) {
    esp_err_t ret;

    // Initialize GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_RST) | (1ULL << PIN_NUM_BUSY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_direction(PIN_NUM_BUSY, GPIO_MODE_INPUT);

    // Initialize SPI
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };

    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init SPI bus: %s", esp_err_to_name(ret));
        return SX126X_HAL_STATUS_ERROR;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 1,
    };

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return SX126X_HAL_STATUS_ERROR;
    }

    return SX126X_HAL_STATUS_OK;
}

static void wait_while_busy(void) {
    while (gpio_get_level(PIN_NUM_BUSY) == 1) {
        // tight loop, consider adding vTaskDelay if needed
    }
}

sx126x_hal_status_t sx126x_hal_write(const void* context, const uint8_t* command, const uint16_t command_length,
                                     const uint8_t* data, const uint16_t data_length) {
    wait_while_busy();

    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = (command_length + data_length) * 8,
        .tx_buffer = NULL,
        .rx_buffer = NULL,
    };

    uint8_t buffer[command_length + data_length];
    memcpy(buffer, command, command_length);
    memcpy(buffer + command_length, data, data_length);
    trans.tx_buffer = buffer;

    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI write failed: %s", esp_err_to_name(ret));
        return SX126X_HAL_STATUS_ERROR;
    }

    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_read(const void* context, const uint8_t* command, const uint16_t command_length,
                                    uint8_t* data, const uint16_t data_length) {
    wait_while_busy();

    spi_transaction_t cmd_trans = {
        .flags = 0,
        .length = command_length * 8,
        .tx_buffer = command,
        .rx_buffer = NULL,
    };
    spi_transaction_t data_trans = {
        .flags = 0,
        .length = data_length * 8,
        .tx_buffer = NULL,
        .rx_buffer = data,
    };

    esp_err_t ret = spi_device_transmit(spi_handle, &cmd_trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI read (cmd) failed: %s", esp_err_to_name(ret));
        return SX126X_HAL_STATUS_ERROR;
    }

    wait_while_busy();

    ret = spi_device_transmit(spi_handle, &data_trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI read (data) failed: %s", esp_err_to_name(ret));
        return SX126X_HAL_STATUS_ERROR;
    }

    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_reset(const void* context) {
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_wakeup(const void* context) {
    uint8_t wake_cmd = 0xC0;
    sx126x_hal_write(context, &wake_cmd, 1, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    return SX126X_HAL_STATUS_OK;
}
