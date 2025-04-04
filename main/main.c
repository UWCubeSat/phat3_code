#include <stdio.h>
#include <driver/gpio.h>
#include <soc/gpio_sig_map.h>
#include <esp_check.h>
#include <i2c.h>
#include <sd_card.h>


#define LOG_TAG "main"


void app_main(void)
{
    int sd_fd;
    ESP_ERROR_CHECK(init_sd_card(&sd_fd));


    
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