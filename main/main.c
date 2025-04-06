#include <stdio.h>
#include <driver/gpio.h>
#include <soc/gpio_sig_map.h>
#include <esp_check.h>
#include <i2c.h>
#include <sd_card.h>
#include <ucam.h>
#include <fcntl.h>
#include <unistd.h>
#include <esp_task.h>


#define LOG_TAG "main"


void app_main(void)
{
    char sd_path[64];
    ESP_ERROR_CHECK(sd_card_mount(sd_path, sizeof(sd_path)));
    
    char log_path[64];
    int res = snprintf(log_path, sizeof(log_path), "%s/log.txt", sd_path);
    if (res == -1 || res >= sizeof(log_path)) {
        abort();
    }

    ESP_LOGI(LOG_TAG, "PATH: %s", log_path);

    FILE* logfile = fopen(log_path, "w");

    char text[] = "Hello world!\n";
    fwrite(text, sizeof(text) - 1, 1, logfile);
    fclose(logfile);

    vTaskDelay(pdMS_TO_TICKS(1000));

    uint8_t* jpg;
    uint32_t jpg_len;

    ESP_ERROR_CHECK(ucam_get_photo(&jpg, &jpg_len));

    char img_path[64];
    res = snprintf(img_path, sizeof(img_path), "%s/img.jpg", sd_path);
    if (res == -1 || res >= sizeof(img_path)) {
        abort();
    }
    FILE* imgfile = fopen(img_path, "w");
    fwrite(jpg, jpg_len, 1, imgfile);
    fclose(imgfile);

    ESP_ERROR_CHECK(sd_card_unmount());
}


// // Remaps the ESP32 GPIO pins to match our PHAT-3 PCB design.
// // 
// static esp_err_t configure_heltec_gpio_matrix(void) {
//     ESP_RETURN_ON_ERROR(gpio_set_direction(GPIO_NUM_6, GPIO_MODE_INPUT_OUTPUT), TAG, "Couldn't configure GPIO");
//     gpio_set_pull_mode(GPIO_NUM_6, GPIO_PULLUP_ONLY);
//     gpio_iomux_in(GPIO_NUM_6, I2CEXT0_SDA_IN_IDX);
//     gpio_iomux_out(GPIO_NUM_6, I2CEXT0_SDA_OUT_IDX, false);

//     gpio_set_direction(GPIO_NUM_7, GPIO_MODE_OUTPUT);
//     gpio_set_pull_mode(GPIO_NUM_7, GPIO_PULLUP_ONLY);
//     // gpio_iomux_in(GPIO_NUM_45, I2CEXT0_SCL_IN_IDX);
//     gpio_iomux_out(GPIO_NUM_7, I2CEXT0_SDA_OUT_IDX, false);

//     return ESP_OK;
//     gpio_dump_io_configuration(stdout, (1ULL << 6) | (1ULL << 7) | (1ULL << 26));
// }