#include <radio.h>
#include <ra01s.h>
#include <esp_err.h>
#include <esp_log.h>

// Code based heavily on usage example of sx1262 library here:
// https://github.com/nopnop2002/esp-idf-sx126x/blob/main/basic/main/main.c

#define LOG_TAG "radio"

esp_err_t radio_init(void) {
    LoRaInit();

    uint32_t frequencyInHz = 915000000;
    int8_t txPowerInDbm = 22;
    float tcxoVoltage = 3.3; // use TCXO
	bool useRegulatorLDO = true; // use DCDC + LDO

    int16_t ret = LoRaBegin(frequencyInHz, txPowerInDbm, tcxoVoltage, useRegulatorLDO);

    if (ret != 0) {
        ESP_LOGE(LOG_TAG, "Error on LoRaBegin");
        return ESP_FAIL;
    }

    uint8_t spreadingFactor = 7;
	uint8_t bandwidth = 4;
	uint8_t codingRate = 1;
	uint16_t preambleLength = 8;
	uint8_t payloadLen = 0;
	bool crcOn = true;
	bool invertIrq = false;

    LoRaConfig(spreadingFactor, bandwidth, codingRate, preambleLength, payloadLen, crcOn, invertIrq);

    return ESP_OK;
}

esp_err_t radio_send(uint8_t *data, int16_t len) {
    if (LoRaSend(data, len, SX126x_TXMODE_SYNC)) {
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

uint8_t radio_receive(uint8_t *data, int16_t len) {
    return LoRaReceive(data, len);
}