#include <driver/uart.h>
#include <esp_check.h>
#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <errno.h>

#define LOG_TAG "ucam"

const int UART_BUF_SIZE = (4 * UART_HW_FIFO_LEN(UART_NUM_1));
const int GPIO_TX = 39;
const int GPIO_RX = 40;

// Documentation: https://resources.4dsystems.com.au/datasheets/accessories/uCAM-III/#command-set

// Reset the camera
const uint8_t CMD_RESET[6] = {0xAA, 0x08, 0x00, 0x00, 0x00, 0x00};

// Wake up the camera and sync the baud rate
const uint8_t CMD_SYNC[6] = {0xAA, 0x0D, 0x00, 0x00, 0x00, 0x00};
const uint8_t ACK_SYNC[6] = {0xAA, 0x0E, 0x0D, 0x00, 0x00, 0x00};

// Initialize the camera
const uint8_t CMD_INIT[6] =  {0xAA, 0x01, 0x00, 0x07, 0x07, 0x07};

// Set package size to 512 bytes
const uint8_t CMD_PACKSIZE[6] =  {0xAA, 0x06, 0x08, 0x00, 0x02, 0x00};

// Take a photograph
const uint8_t CMD_SNAPSHOT[6] =  {0xAA, 0x05, 0x00, 0x00, 0x00, 0x00};

// Request the taken photograph
const uint8_t CMD_GET_PIC[6] =  {0xAA, 0x04, 0x01, 0x00, 0x00, 0x00};

// Beginning of accept response
const uint8_t RESP_ACK[2] = {0xAA, 0x0E};

// Beginning of no-accept response
const uint8_t RESP_NAK[3] = {0xAA, 0x0F, 0x00};

// Beginning of get picture response
const uint8_t RESP_DATA[3] =  {0xAA, 0x0A, 0x01};

// Image buffer
static uint8_t jpeg_buf[32000];


// Sends a command to the UART, and confirm acknowledgment.
static esp_err_t send_cmd(const uint8_t cmd[6], uint32_t ms_timeout) {
    int res;
    esp_err_t ret;

    ret = uart_flush_input(UART_NUM_1);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't flush UART");
    
    res = uart_write_bytes(UART_NUM_1, cmd, 6);
    ESP_RETURN_ON_FALSE(res == 6, ESP_FAIL, LOG_TAG, "UART write failed");

    ret = uart_wait_tx_done(UART_NUM_1, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't write UART");

    uint8_t reply[6];
    res = uart_read_bytes(UART_NUM_1, reply, 6, pdMS_TO_TICKS(ms_timeout));
    if (res != 6) {
        return ESP_ERR_TIMEOUT;
    } else if (memcmp(reply, RESP_ACK, sizeof(RESP_ACK)) == 0) {
        return ESP_OK;
    } else if (memcmp(reply, RESP_NAK, sizeof(RESP_NAK)) == 0) {
        ESP_LOGE(LOG_TAG, "Received NAK with error code: %d", reply[4]);
        return ESP_FAIL;
    } else {
        ESP_LOGE(LOG_TAG, "Received unknown msg instead of ack");
        return ESP_FAIL;
    }
}


esp_err_t ucam_save_photo(char* save_dir_path) {
    esp_err_t ret;
    int res;
    uint8_t reply[6];

    // Start the driver if it hasn't been already
    if (!uart_is_driver_installed(UART_NUM_1)) {
        uart_config_t uart_config = {
            .baud_rate = 921600,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
            .stop_bits = UART_STOP_BITS_1,
        };
        ret = uart_driver_install(UART_NUM_1, UART_BUF_SIZE, UART_BUF_SIZE, 10, NULL, 0);
        ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't install UART driver");
        ret = uart_param_config(UART_NUM_1, &uart_config);
        ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't set UART config");
        ret = uart_set_pin(UART_NUM_1, GPIO_TX, GPIO_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't configure UART GPIO pins");
    }

    // Wake up the camera (according to documentation)
    for (int retries = 0; retries < 60; retries++) {
        ret = send_cmd(CMD_SYNC, retries + 5);
        if (ret == ESP_OK) {
            break;
        }
    }
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't sync with UCAM-III");
    
    // Acknowledge sync message from camera
    res = uart_read_bytes(UART_NUM_1, reply, 6, pdMS_TO_TICKS(1000));
    ESP_RETURN_ON_FALSE(res == 6, ESP_FAIL, LOG_TAG, "UART read failed");

    if (memcmp(reply, CMD_SYNC, sizeof(CMD_SYNC)) != 0) {
        return ESP_FAIL;
    }
    res = uart_write_bytes(UART_NUM_1, ACK_SYNC, sizeof(ACK_SYNC));
    ESP_RETURN_ON_FALSE(res == 6, ESP_FAIL, LOG_TAG, "UART write failed");
    
    // initialize the camera
    ret = send_cmd(CMD_INIT, 1000);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "No ack");
    ret = send_cmd(CMD_PACKSIZE, 1000);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "No ack");

    // Wait for the camera to adjust exposure (according to documentation)
    vTaskDelay(pdMS_TO_TICKS(2000));

    // take the picture
    ret = send_cmd(CMD_SNAPSHOT, 1000);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "No ack");
    vTaskDelay(pdMS_TO_TICKS(200));
    ret = send_cmd(CMD_GET_PIC, 1000);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "No ack");

    // Get the photo size
    res = uart_read_bytes(UART_NUM_1, reply, 6, pdMS_TO_TICKS(1000));
    ESP_RETURN_ON_FALSE(res == 6, ESP_FAIL, LOG_TAG, "Reply had incorrect size");
    if (memcmp(reply, RESP_DATA, sizeof(RESP_DATA)) != 0) {
        return ESP_FAIL;
    }
    uint32_t jpeg_size = (uint32_t) reply[3] | ((uint32_t) reply[4] << 8) | ((uint32_t) reply[5] << 16);

    ret = uart_flush_input(UART_NUM_1);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't flush UART");

    size_t bytes_read = 0;

    for (uint16_t i = 0; i < (jpeg_size / (512 - 6)) + 1; i++) {
        int res;
        esp_err_t ret;

        uint8_t ack_chunk[6] = {0xAA, 0x0E, 0x00, 0x00, i & 0xFF, (i >> 8) & 0xFF};
        res = uart_write_bytes(UART_NUM_1, ack_chunk, sizeof(ack_chunk));
        ESP_RETURN_ON_FALSE(res == 6, ESP_FAIL, LOG_TAG, "UART write failed");

        ret = uart_wait_tx_done(UART_NUM_1, portMAX_DELAY);
        ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't write UART");

        res = uart_read_bytes(UART_NUM_1, reply, 4, pdMS_TO_TICKS(100));
        ESP_RETURN_ON_FALSE(res == 4, ESP_FAIL, LOG_TAG, "UART read failed");

        uint16_t reported_id = (uint16_t) reply[0] | ((uint16_t) reply[1] << 8);
        ESP_RETURN_ON_FALSE(i + 1 == reported_id, ESP_FAIL, LOG_TAG, "Unexpected chunk id");

        uint16_t data_size = (uint16_t) reply[2] | ((uint16_t) reply[3] << 8);
        
        // Read image payload
        res = uart_read_bytes(UART_NUM_1, jpeg_buf + bytes_read, data_size, pdMS_TO_TICKS(100));
        ESP_RETURN_ON_FALSE(res == data_size, ESP_FAIL, LOG_TAG, "IMG chunk read failed");
        bytes_read += res;

        // Verify the footer
        res = uart_read_bytes(UART_NUM_1, reply, 2, pdMS_TO_TICKS(100));
        ESP_RETURN_ON_FALSE(res == 2, ESP_FAIL, LOG_TAG, "Uart read failed");
        ESP_RETURN_ON_FALSE(reply[1] == 0, ESP_FAIL, LOG_TAG, "Expected last chunk byte to be zero");
    }


    ESP_RETURN_ON_FALSE(bytes_read == jpeg_size, ESP_FAIL, LOG_TAG, "Read unexpected image size");

    // Generate a timestamped image filename
    int64_t time = esp_timer_get_time() / 1000000;
    char img_path[64];
    res = snprintf(img_path, sizeof(img_path), "%s/img%llu.jpg", save_dir_path, time);
    if (res == -1 || res >= sizeof(img_path)) {
        return ESP_ERR_NO_MEM;
    }

    // Save the image
    FILE* imgfile = fopen(img_path, "w");
    if (imgfile == NULL) {
        ESP_LOGE(LOG_TAG, "Couldn't create jpg file. %s", strerror(errno));
        return ESP_FAIL;
    }
    res = fwrite(jpeg_buf, jpeg_size, 1, imgfile);
    if (res != 1) {
        ESP_LOGE(LOG_TAG, "Couldn't write jpg to file. %s", strerror(errno));
        return ESP_FAIL;
    }
    // Close (and flush) the file
    res = fclose(imgfile);
    if (res != 0) {
        ESP_LOGE(LOG_TAG, "Couldn't close jpg file. %s", strerror(errno));
    }

    return ESP_OK;
}
