#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_check.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <fcntl.h>
#include <errno.h>

#define LOG_TAG "sd_card"

#define GPIO_MOSI 47
#define GPIO_MISO 34
#define GPIO_CLK 33
#define GPIO_CS 48

esp_err_t init_sd_card(int* ret_fd) {
    esp_err_t ret;

    const char mount_point[] = "/sdcard";

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI1_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_CLK,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't initialize SPI bus.");

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_CS;
    slot_config.host_id = host.slot;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
    };

    // This function call uses all the configuration created above.
    sdmmc_card_t* card;
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    ESP_RETURN_ON_ERROR(ret, LOG_TAG, "Couldn't mount SD card file system.");

    // Display SD Card info
    ESP_LOGI(LOG_TAG, "SD Card info: ");
    sdmmc_card_print_info(stdout, card);

    // Create a file at an unused path
    char path[64];
    *ret_fd = -1;
    for (int filenum = 0; filenum < 100; filenum++) {
        snprintf(path, sizeof(path), "/sdcard/flight_log_%d.txt", filenum);

        *ret_fd = open("/sdcard/myfile.txt", O_CREAT | O_EXCL | O_WRONLY, 0666);
        if (*ret_fd == -1) {
            if (errno == EEXIST) {
                continue;
            } else {
                ESP_LOGE(LOG_TAG, "Couldn't open ");
                return ESP_FAIL;
            }
        }
    }

    if (*ret_fd == -1) {
        ESP_LOGE(LOG_TAG, "All 100 possible SD file names taken!");
        return ESP_FAIL;
    }

    return ESP_OK;
}