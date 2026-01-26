#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Motor control modes
typedef enum {
    MOTOR_MODE_AUTO = 0,    // Controlled by pedal (default)
    MOTOR_MODE_MANUAL_ON,   // Forced ON by app
    MOTOR_MODE_MANUAL_OFF   // Forced OFF by app
} motor_mode_t;

// Status structure for BLE notifications
typedef struct {
    bool pedal_pressed;     // Pedal input state
    bool counter_complete;  // Counter input state
    bool motor_running;     // Motor output state
    motor_mode_t mode;      // Current control mode
    uint16_t frequency;     // Current PWM frequency
} motor_status_t;

/**
 * @brief Initialize motor control (PWM and GPIO)
 * @return ESP_OK on success
 */
esp_err_t motor_control_init(void);

/**
 * @brief Start PWM output at current frequency
 */
void motor_start(void);

/**
 * @brief Stop PWM output
 */
void motor_stop(void);

/**
 * @brief Set PWM frequency
 * @param freq_hz Frequency in Hz (100-1000)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if out of range
 */
esp_err_t motor_set_frequency(uint16_t freq_hz);

/**
 * @brief Get current PWM frequency
 * @return Current frequency in Hz
 */
uint16_t motor_get_frequency(void);

/**
 * @brief Set motor control mode
 * @param mode MOTOR_MODE_AUTO, MOTOR_MODE_MANUAL_ON, or MOTOR_MODE_MANUAL_OFF
 */
void motor_set_mode(motor_mode_t mode);

/**
 * @brief Get current motor control mode
 * @return Current mode
 */
motor_mode_t motor_get_mode(void);

/**
 * @brief Get current motor status (for BLE notifications)
 * @param status Pointer to status structure to fill
 */
void motor_get_status(motor_status_t *status);

/**
 * @brief Update motor control logic (call periodically or on input change)
 * Should be called from main loop or timer
 */
void motor_control_update(void);

#ifdef __cplusplus
}
#endif
