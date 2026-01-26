#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "motor_control.h"
#include "ble_service.h"

static const char *TAG = "main";

// Timer handle for periodic status updates
static TimerHandle_t s_status_timer = NULL;

// Previous status for change detection
static uint8_t s_prev_status = 0xFF;

static void status_timer_callback(TimerHandle_t xTimer)
{
    // Update motor control logic
    motor_control_update();

    // Get current status
    motor_status_t status;
    motor_get_status(&status);

    // Pack status byte
    uint8_t status_byte = ble_pack_status(status.pedal_pressed,
                                          status.counter_complete,
                                          status.motor_running);

    // Send notification if connected (only if status changed or periodically)
    if (ble_is_connected()) {
        // Always send to ensure real-time updates
        ble_notify_status(status_byte);

        // Log if status changed
        if (status_byte != s_prev_status) {
            ESP_LOGI(TAG, "Status: pedal=%d, counter=%d, motor=%d",
                     status.pedal_pressed, status.counter_complete, status.motor_running);
            s_prev_status = status_byte;
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Stepper BLE Controller ===");
    ESP_LOGI(TAG, "Firmware version: 1.0.0");

    // Initialize motor control (PWM + GPIO)
    esp_err_t ret = motor_control_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Motor control init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Initialize BLE service
    ret = ble_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BLE service init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Create timer for periodic status updates
    s_status_timer = xTimerCreate(
        "status_timer",
        pdMS_TO_TICKS(CONFIG_STATUS_UPDATE_INTERVAL_MS),
        pdTRUE,  // Auto-reload
        NULL,
        status_timer_callback
    );

    if (s_status_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create status timer");
        return;
    }

    // Start the timer
    if (xTimerStart(s_status_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start status timer");
        return;
    }

    ESP_LOGI(TAG, "System initialized successfully");
    ESP_LOGI(TAG, "Status update interval: %d ms", CONFIG_STATUS_UPDATE_INTERVAL_MS);
    ESP_LOGI(TAG, "Default frequency: %d Hz", CONFIG_STEPPER_DEFAULT_FREQ);

    // Main loop - mostly handled by FreeRTOS tasks
    // We just keep the task alive and log status periodically
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        motor_status_t status;
        motor_get_status(&status);

        ESP_LOGI(TAG, "Heartbeat - Mode: %s, Connected: %s",
                 status.mode == MOTOR_MODE_AUTO ? "AUTO" :
                 status.mode == MOTOR_MODE_MANUAL_ON ? "MANUAL_ON" : "MANUAL_OFF",
                 ble_is_connected() ? "Yes" : "No");
    }
}
