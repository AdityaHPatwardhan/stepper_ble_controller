#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// BLE Commands (write to Command characteristic)
#define BLE_CMD_STOP            0x00
#define BLE_CMD_START           0x01
#define BLE_CMD_AUTO_MODE       0x02

// Status byte bit positions
#define STATUS_BIT_PEDAL        0
#define STATUS_BIT_COUNTER      1
#define STATUS_BIT_MOTOR        2

/**
 * @brief Initialize BLE service
 * @return ESP_OK on success
 */
esp_err_t ble_service_init(void);

/**
 * @brief Send status notification to connected client
 * @param status_byte Packed status byte
 */
void ble_notify_status(uint8_t status_byte);

/**
 * @brief Check if a client is connected
 * @return true if connected
 */
bool ble_is_connected(void);

/**
 * @brief Pack motor status into a status byte
 * @param pedal_pressed Pedal state
 * @param counter_complete Counter state
 * @param motor_running Motor state
 * @return Packed status byte
 */
static inline uint8_t ble_pack_status(bool pedal_pressed, bool counter_complete, bool motor_running)
{
    uint8_t status = 0;
    if (pedal_pressed)    status |= (1 << STATUS_BIT_PEDAL);
    if (counter_complete) status |= (1 << STATUS_BIT_COUNTER);
    if (motor_running)    status |= (1 << STATUS_BIT_MOTOR);
    return status;
}

#ifdef __cplusplus
}
#endif
