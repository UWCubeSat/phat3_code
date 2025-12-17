idf_component_register(
    SRCS "radio.c"
    INCLUDE_DIRS "."
    REQUIRES nopnop2002__sx126x esp_driver_spi
)