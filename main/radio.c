#include <radio.h>
// TODO:
// I think it'll be simplest for us to use the official Semtech SX1262 driver:
// https://github.com/Lora-net/sx126x_driver
// I already added it as a dependency to the `components` directory.
// Before we can use it, we must implement the
// hardware-abstraction-layer functions declared in `sx126x_hal.h`.


#include <sx126x.h>