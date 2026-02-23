// Our headers
#include <sensors.h>
#include <ucam.h>
#include <gps.h>
#include <sd_card.h>
#include <radio.h>
#include <rs.h>

// ESP-IDF headers
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <driver/gpio.h>
#include <soc/gpio_sig_map.h>
#include <esp_check.h>
#include <fcntl.h>
#include <unistd.h>
#include <esp_task.h>
#include <mbedtls/base64.h>

// Tag used for logging
#define LOG_TAG "main"

#define SERIAL_DATA_FMT "DATA_PKT;%llu;%.2f;%.2f;%.2f;%lu;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%u;%.2f;%.2f;%.8f;%.8f;%s\n"
#define SERIAL_JPEG_FMT "JPEG_PKT;%lu;%s\n"
// Size of each shard
#define JPEG_SHARD_SIZE 230
// Reed-Solomon shard configuration
#define DATA_SHARDS 16
#define PARITY_SHARDS 4
// Maximum number of blocks we can handle (including RS parity shards)
#define MAX_JPEG_SIZE 72000
#define MAX_TOTAL_SHARDS ((((MAX_JPEG_SIZE + JPEG_SHARD_SIZE - 1) / JPEG_SHARD_SIZE + DATA_SHARDS - 1) / DATA_SHARDS) * (DATA_SHARDS + PARITY_SHARDS))
// Maximum size of b64 buf
#define MAX_B64_BUF_SIZE (((MAX_JPEG_SIZE + 3 - 1) / 3) * 4 + 1)

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

typedef struct {
    int8_t rssi;
    int8_t snr;
    packet_t pkt;
} packet_meta_t;


static uint8_t b64_buf[MAX_B64_BUF_SIZE];
static uint8_t jpeg_shards_buf[MAX_TOTAL_SHARDS * JPEG_SHARD_SIZE];
static bool shards_received[MAX_TOTAL_SHARDS];
static uint8_t curr_photo_id = 0;
static uint16_t expected_shard_count = 0;
static uint16_t received_shard_count = 0;
static uint32_t original_data_size = 0;
static bool sent_serial_jpeg = false;

static QueueHandle_t packet_queue;


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

// Calculate the total number of shards (data + parity) for a given data size
static int calculate_total_shards(reed_solomon *rs, int data_size, int block_size) {
    int nr_blocks = (data_size + block_size - 1) / block_size;
    nr_blocks = ((nr_blocks + rs->data_shards - 1) / rs->data_shards) * rs->data_shards;
    int nr_fec_blocks = (nr_blocks / rs->data_shards) * rs->parity_shards;
    return nr_blocks + nr_fec_blocks;
}

static bool groups_decoded[MAX_TOTAL_SHARDS / (DATA_SHARDS + PARITY_SHARDS)];

static void reset_jpeg_reception(void) {
    memset(shards_received, 0, sizeof(shards_received));
    memset(jpeg_shards_buf, 0, sizeof(jpeg_shards_buf));
    memset(groups_decoded, 0, sizeof(groups_decoded));
    expected_shard_count = 0;
    received_shard_count = 0;
    original_data_size = 0;
    sent_serial_jpeg = false;
}

static int decode_group(reed_solomon *rs, int group_idx, int block_size) {
    int nr_groups = expected_shard_count / (DATA_SHARDS + PARITY_SHARDS);
    int data_base = group_idx * DATA_SHARDS;
    int parity_base = nr_groups * DATA_SHARDS + group_idx * PARITY_SHARDS;

    unsigned char *data_blocks[DATA_SHARDS + PARITY_SHARDS];
    unsigned char zilch[DATA_SHARDS + PARITY_SHARDS];
    memset(zilch, 0, DATA_SHARDS + PARITY_SHARDS);

    for (int i = 0; i < DATA_SHARDS; i++) {
        int shard_idx = data_base + i;
        data_blocks[i] = jpeg_shards_buf + shard_idx * block_size;
        if (!shards_received[shard_idx]) {
            memset(data_blocks[i], 0, block_size);
            zilch[i] = 1;
        }
    }
    for (int i = 0; i < PARITY_SHARDS; i++) {
        int shard_idx = parity_base + i;
        data_blocks[DATA_SHARDS + i] = jpeg_shards_buf + shard_idx * block_size;
        if (!shards_received[shard_idx]) {
            memset(data_blocks[DATA_SHARDS + i], 0, block_size);
            zilch[DATA_SHARDS + i] = 1;
        }
    }

    return reed_solomon_reconstruct(rs, data_blocks, zilch, DATA_SHARDS + PARITY_SHARDS, block_size);
}

static void radio_receive_task(void* pvParameters) {
    while (true) {
        uint8_t bytes;
        packet_meta_t curr;
        int8_t rssi, snr;
        bytes = radio_receive((uint8_t*) &curr.pkt, sizeof(curr.pkt), &rssi, &snr);
        if (bytes == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        if (bytes != sizeof(curr.pkt)) {
            ESP_LOGW(LOG_TAG, "Corrupted radio packet: expected %d bytes, got %d",
                sizeof(curr.pkt), bytes);
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        curr.rssi = rssi;
        curr.snr = snr;
        if (xQueueSend(packet_queue, &curr, 0) != pdTRUE) {
            ESP_LOGW(LOG_TAG, "Packet queue full, dropping packet");
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void packet_process_task(void* pvParameters) {
    fec_init();
    reed_solomon *rs = reed_solomon_new(DATA_SHARDS, PARITY_SHARDS);
    if (rs == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create Reed-Solomon decoder");
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        packet_meta_t curr;
        // Block until a packet is available
        if (xQueueReceive(packet_queue, &curr, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        ESP_LOGW(LOG_TAG, "Packet rssi: %d, snr: %d", curr.rssi, curr.snr);

        switch (curr.pkt.header) {
            case JPEG_PKT: {
                // First shard of a new image - reset state
                if (curr.pkt.photo_id != curr_photo_id) {
                    reset_jpeg_reception();
                    curr_photo_id = curr.pkt.photo_id;
                    original_data_size = curr.pkt.data_size;
                    expected_shard_count = calculate_total_shards(rs, original_data_size, JPEG_SHARD_SIZE);
                    ESP_LOGI(LOG_TAG, "Starting new JPEG reception: %lu bytes, %lu shards, %u id",
                             (unsigned long)original_data_size, (unsigned long)expected_shard_count, (unsigned)curr_photo_id);
                }
                
                // Validate shard index
                if (curr.pkt.shard_idx >= expected_shard_count) {
                    ESP_LOGW(LOG_TAG, "Invalid shard index: %lu >= %lu",
                             (unsigned long)curr.pkt.shard_idx, (unsigned long)expected_shard_count);
                    break;
                }

                // Store shard if not already received
                if (!shards_received[curr.pkt.shard_idx]) {
                    memcpy(jpeg_shards_buf + curr.pkt.shard_idx * JPEG_SHARD_SIZE, curr.pkt.jpeg_shard, JPEG_SHARD_SIZE);
                    shards_received[curr.pkt.shard_idx] = true;
                    received_shard_count++;
                }
                ESP_LOGI(LOG_TAG, "Received shard %lu/%lu",
                         (unsigned long)curr.pkt.shard_idx, (unsigned long)expected_shard_count);

                if (!sent_serial_jpeg) {
                    int nr_groups = expected_shard_count / (DATA_SHARDS + PARITY_SHARDS);
                    int nr_data_shards_total = nr_groups * DATA_SHARDS;

                    // Correctly determine group_idx for both data and parity shards
                    int group_idx;
                    if (curr.pkt.shard_idx < nr_data_shards_total) {
                        // Data shard
                        group_idx = curr.pkt.shard_idx / DATA_SHARDS;
                    } else {
                        // Parity shard
                        group_idx = (curr.pkt.shard_idx - nr_data_shards_total) / PARITY_SHARDS;
                    }

                    if (group_idx < nr_groups && !groups_decoded[group_idx]) {
                        if (decode_group(rs, group_idx, JPEG_SHARD_SIZE) == 0) {
                            groups_decoded[group_idx] = true;
                            ESP_LOGI(LOG_TAG, "Group %d decoded successfully", group_idx);
                        }
                    }

                    // Check if all groups are decoded
                    bool all_decoded = true;
                    for (int i = 0; i < nr_groups; i++) {
                        if (!groups_decoded[i]) { all_decoded = false; break; }
                    }

                    if (all_decoded) {
                        size_t written;
                        mbedtls_base64_encode(b64_buf, sizeof(b64_buf), &written,
                                              jpeg_shards_buf, original_data_size);
                        printf(SERIAL_JPEG_FMT, (unsigned long)original_data_size, b64_buf);
                        sent_serial_jpeg = true;
                    }
                }
                break;
            }
            case SENSOR_PKT:
                sensors_log_data_compact(&curr.pkt.sensor_data);
                break;
            default:
                ESP_LOGW(LOG_TAG, "Corrupted radio packet: got %d header", curr.pkt.header);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(radio_init());
    
    packet_queue = xQueueCreate(10, sizeof(packet_meta_t));

    // Spawn threads
    xTaskCreate(radio_receive_task, "radio_receive_task", 4096, NULL, 3, NULL);
    xTaskCreate(packet_process_task, "radio_receive_task", 16384, NULL, 2, NULL);
    
    ESP_LOGI(LOG_TAG, "System start successful.");
}
