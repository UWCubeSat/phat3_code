#include <sd_card.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_check.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define LOG_TAG "sd_card"

const gpio_num_t GPIO_MOSI = GPIO_NUM_47;
const gpio_num_t GPIO_MISO = GPIO_NUM_34;
const gpio_num_t GPIO_CLK = GPIO_NUM_33;
const gpio_num_t GPIO_CS = GPIO_NUM_48;

static sdmmc_card_t* card;

esp_err_t sd_card_mount(char* ret_dir_path, size_t ret_dir_path_size) {
    esp_err_t ret;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_CLK,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't initialize SPI bus.");

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_CS;
    slot_config.host_id = host.slot;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
    };

    // This function call uses all the configuration created above.
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't mount SD card file system.");

    // Display SD Card info
    ESP_LOGI(LOG_TAG, "SD Card info: ");
    sdmmc_card_print_info(stdout, card);

    // Create a new unused folder
    for (int foldernum = 0; foldernum < 100; foldernum++) {
        int res = snprintf(ret_dir_path, ret_dir_path_size, "/sdcard/flight_%d/", foldernum);
        if (res == -1 || res >= ret_dir_path_size) {
            return ESP_ERR_NO_MEM;
        }
        res = mkdir(ret_dir_path, 0777);
        if (res == 0) {
            return ESP_OK;
        } else if (errno == EEXIST) {
            continue;
        } else {
            return ESP_FAIL;
        }
    }

    ESP_LOGE(LOG_TAG, "All 100 possible SD directory names taken!");
    return ESP_FAIL;
}

esp_err_t sd_card_unmount(void) {
    return esp_vfs_fat_sdcard_unmount("/sdcard", card);
}