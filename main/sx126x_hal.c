#include <sx126x_hal.h>

// This file implements the functions from the
// above library file ^.


/**
 * Radio data transfer - write
 *
 * @remark Shall be implemented by the user
 *
 * @param [in] context          Radio implementation parameters
 * @param [in] command          Pointer to the buffer to be transmitted
 * @param [in] command_length   Buffer size to be transmitted
 * @param [in] data             Pointer to the buffer to be transmitted
 * @param [in] data_length      Buffer size to be transmitted
 *
 * @returns Operation status
 */
sx126x_hal_status_t sx126x_hal_write( const void* context, const uint8_t* command, const uint16_t command_length,
    const uint8_t* data, const uint16_t data_length
) {
    // TODO
    return SX126X_HAL_STATUS_OK;
}

/**
* Radio data transfer - read
*
* @remark Shall be implemented by the user
*
* @param [in] context          Radio implementation parameters
* @param [in] command          Pointer to the buffer to be transmitted
* @param [in] command_length   Buffer size to be transmitted
* @param [in] data             Pointer to the buffer to be received
* @param [in] data_length      Buffer size to be received
*
* @returns Operation status
*/
sx126x_hal_status_t sx126x_hal_read( const void* context, const uint8_t* command, const uint16_t command_length,
   uint8_t* data, const uint16_t data_length
) {
    // TODO
    return SX126X_HAL_STATUS_OK;
}

/**
* Reset the radio
*
* @remark Shall be implemented by the user
*
* @param [in] context Radio implementation parameters
*
* @returns Operation status
*/
sx126x_hal_status_t sx126x_hal_reset( const void* context ) {
    // TODO
    return SX126X_HAL_STATUS_OK;
}

/**
* Wake the radio up.
*
* @remark Shall be implemented by the user
*
* @param [in] context Radio implementation parameters
*
* @returns Operation status
*/
sx126x_hal_status_t sx126x_hal_wakeup( const void* context ) {
    // TODO
    return SX126X_HAL_STATUS_OK;
}