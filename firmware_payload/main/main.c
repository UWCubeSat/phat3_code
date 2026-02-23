// Our headers
#include <sensors.h>
#include <ucam.h>
#include <gps.h>
#include <sd_card.h>
#include <radio.h>
#include <rs.h>

// ESP-IDF headers
#include <stdio.h>
#include <string.h>
#include <driver/gpio.h>
#include <soc/gpio_sig_map.h>
#include <esp_check.h>
#include <fcntl.h>
#include <unistd.h>
#include <esp_task.h>

// Tag used for logging
#define LOG_TAG "main"
// Size of each shard
#define JPEG_SHARD_SIZE 230
// Reed-Solomon shard configuration
#define DATA_SHARDS 16
#define PARITY_SHARDS 4
// Maximum number of blocks we can handle (including RS parity shards)
#define MAX_JPEG_SIZE 72000
#define MAX_TOTAL_SHARDS ((((MAX_JPEG_SIZE + JPEG_SHARD_SIZE - 1) / JPEG_SHARD_SIZE + DATA_SHARDS - 1) / DATA_SHARDS) * (DATA_SHARDS + PARITY_SHARDS))


// Packet type defs
typedef enum : uint8_t {
    JPEG_PKT = 0,
    SENSOR_PKT = 1,
} headers_t;

typedef struct {
    headers_t header;
    uint8_t photo_id;
    uint16_t shard_idx;
    uint32_t data_size;
    union {
        uint8_t jpeg_shard[JPEG_SHARD_SIZE];
        sensors_data_t sensor_data;
    };
} packet_t;

static uint8_t jpeg_shards_buf[MAX_TOTAL_SHARDS * JPEG_SHARD_SIZE];
static uint32_t jpeg_size = 0;
static uint8_t photo_id = 0;  // fine if this overflows
static uint16_t curr_shard = 0;
static uint16_t shard_count = 0;

static char photo_dir[64];
static char sensor_csv_path[64];
static sensors_data_t sensors_data;
static bool sensors_ready;

static int update_encoding(reed_solomon *rs, unsigned char *data, int data_size, int block_size) {
    int rs_data_shards = rs->data_shards;
    int rs_parity_shards = rs->parity_shards;
    
    int nr_blocks = (data_size + block_size - 1) / block_size;
    nr_blocks = ((nr_blocks + rs_data_shards - 1) / rs_data_shards) * rs_data_shards;
    
    int nr_groups = nr_blocks / rs_data_shards;
    int nr_fec_blocks = nr_groups * rs_parity_shards;
    int nr_shards = nr_blocks + nr_fec_blocks;
    
    if (nr_shards > MAX_TOTAL_SHARDS) {
        ESP_LOGE(LOG_TAG, "Data too large for encoding: %d shards needed", nr_shards);
        return -1;
    }
    
    // Layout: all data blocks first, then all parity blocks
    // [group0_data][group1_data]...[group0_parity][group1_parity]...
    memcpy(jpeg_shards_buf, data, data_size);
    memset(jpeg_shards_buf + data_size, 0, (nr_shards * block_size) - data_size);
    
    unsigned char *shard_ptrs[MAX_TOTAL_SHARDS];
    for (int i = 0; i < nr_shards; i++) {
        shard_ptrs[i] = jpeg_shards_buf + i * block_size;
    }
    
    int result = reed_solomon_encode2(rs, shard_ptrs, nr_shards, block_size);
    if (result == 0) {
        shard_count = nr_shards;
        curr_shard = 0;
    }
    
    return result;
}

static void sensor_task(void* pvParameters) {
    esp_err_t err;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (true) {
        ESP_LOGI(LOG_TAG, "Polling sensors");
        err = sensors_read(&sensors_data);
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        
        err = sensors_save_to_csv(&sensors_data, sensor_csv_path);
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);

        sensors_log_data(&sensors_data);
        sensors_ready = true;
        
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10000));
    }
}

static void camera_task(void* pvParameters) {
    esp_err_t err;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    // Create Reed-Solomon encoder once (reusable)
    fec_init();
    reed_solomon* rs = reed_solomon_new(DATA_SHARDS, PARITY_SHARDS);
    if (rs == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create Reed-Solomon encoder");
        vTaskDelete(NULL);
        return;
    }
    
    while (true) {
        ESP_LOGI(LOG_TAG, "Taking photo");
        err = ucam_save_photo(photo_dir);
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);

        // If the last image has finished transmitting
        if (curr_shard >= shard_count || shard_count == 0) {
            uint8_t* raw_jpeg_buf = NULL;
            err = ucam_get_last_saved_photo(&raw_jpeg_buf, &jpeg_size);
            if (err != ESP_OK || raw_jpeg_buf == NULL) {
                ESP_LOGE(LOG_TAG, "Failed to get photo data");
                xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10000));
                continue;
            }
            ESP_LOGI(LOG_TAG, "Encoding %lu bytes with RS(%d,%d)", 
                     (unsigned long)jpeg_size, DATA_SHARDS, PARITY_SHARDS);
            // TODO: add mutex/semaphore protection for jpeg_shards_buf and shard_count
            int result = update_encoding(rs, raw_jpeg_buf, jpeg_size, JPEG_SHARD_SIZE);
            if (result == 0) {
                photo_id += 1;
                ESP_LOGI(LOG_TAG, "Encoded into %lu shards", (unsigned long)shard_count);
            } else {
                ESP_LOGE(LOG_TAG, "Reed-Solomon encoding failed");
            }
        }
        
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10000)); 
    }
}

static void radio_task(void* pvParameters) {
    esp_err_t err;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (true) {
        // prioritize sensor packet
        if (sensors_ready) {
            ESP_LOGI(LOG_TAG, "Sending sensor packet");

            packet_t snr_pkt = { .header = SENSOR_PKT, .photo_id = 0, .shard_idx = 0, .data_size = 0, .jpeg_shard = {0} };
            memcpy(&snr_pkt.sensor_data, &sensors_data, sizeof(sensors_data_t));
            // TODO: create semephore when changing sensors_ready
            sensors_ready = false;

            err = radio_send((uint8_t*) &snr_pkt, sizeof(snr_pkt));
            ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        
        // send shard of jpeg when available
        } else if (curr_shard < shard_count) {
            ESP_LOGI(LOG_TAG, "Sending jpeg packet");

            packet_t jpeg_pkt = { .header = JPEG_PKT, .photo_id = photo_id, .shard_idx = curr_shard, .data_size = jpeg_size, .jpeg_shard = {0} };
            memcpy(&jpeg_pkt.jpeg_shard, jpeg_shards_buf + curr_shard * JPEG_SHARD_SIZE, JPEG_SHARD_SIZE);
            // TODO: only increment the curr_shard if the radio_send() succeeds
            // TODO: create semephore when changing curr_shard size
            curr_shard += 1;

            err = radio_send((uint8_t*) &jpeg_pkt, sizeof(jpeg_pkt));
            ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        }
        
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(150)); 
    }
}

static void init_save_paths(void) {
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

    res = mkdir(photo_dir, 0777);
    if (res != 0) {
        ESP_LOGE(LOG_TAG, "Couldn't create photo directory");
        abort();
    }
}

void app_main(void) {
    // Create files on SD card

    init_save_paths();
    ESP_ERROR_CHECK(sensors_init());
    ESP_ERROR_CHECK(radio_init());
    
    // Spawn threads
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 2, NULL);
    xTaskCreate(camera_task, "camera_task", 4096, NULL, 2, NULL);
    xTaskCreate(radio_task, "radio_task", 4096, NULL, 2, NULL);
    
    ESP_LOGI(LOG_TAG, "System start successful.");
}
