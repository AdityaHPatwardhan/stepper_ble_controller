#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "motor_control.h"

static const char *TAG = "motor_ctrl";

// GPIO pins from Kconfig
#define PULSE_GPIO      CONFIG_STEPPER_PULSE_GPIO
#define PEDAL_GPIO      CONFIG_STEPPER_PEDAL_GPIO
#define COUNTER_GPIO    CONFIG_STEPPER_COUNTER_GPIO

// LEDC configuration
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_DUTY_RES   LEDC_TIMER_10_BIT
#define LEDC_DUTY       (512)  // 50% duty cycle (1024 / 2)

// State variables
static volatile bool s_pedal_pressed = false;
static volatile bool s_counter_complete = false;
static volatile bool s_motor_running = false;
static motor_mode_t s_mode = MOTOR_MODE_AUTO;
static uint16_t s_frequency = CONFIG_STEPPER_DEFAULT_FREQ;

// ISR handler - defined before use, no forward declaration needed
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;

    if (gpio_num == PEDAL_GPIO) {
        s_pedal_pressed = gpio_get_level(PEDAL_GPIO);
    } else if (gpio_num == COUNTER_GPIO) {
        s_counter_complete = gpio_get_level(COUNTER_GPIO);
    }
}

esp_err_t motor_control_init(void)
{
    esp_err_t ret;

    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = s_frequency,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PULSE_GPIO,
        .duty           = 0,  // Start with 0 duty (stopped)
        .hpoint         = 0
    };
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure pedal input GPIO
    gpio_config_t pedal_conf = {
        .pin_bit_mask = (1ULL << PEDAL_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // Pull down when not pressed
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ret = gpio_config(&pedal_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Pedal GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure counter input GPIO
    gpio_config_t counter_conf = {
        .pin_bit_mask = (1ULL << COUNTER_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // Pull down when not set
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ret = gpio_config(&counter_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Counter GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Install GPIO ISR service
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means ISR service already installed, that's OK
        ESP_LOGE(TAG, "GPIO ISR service install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Add ISR handlers
    gpio_isr_handler_add(PEDAL_GPIO, gpio_isr_handler, (void *)(uintptr_t)PEDAL_GPIO);
    gpio_isr_handler_add(COUNTER_GPIO, gpio_isr_handler, (void *)(uintptr_t)COUNTER_GPIO);

    // Read initial states
    s_pedal_pressed = gpio_get_level(PEDAL_GPIO);
    s_counter_complete = gpio_get_level(COUNTER_GPIO);

    ESP_LOGI(TAG, "Motor control initialized");
    ESP_LOGI(TAG, "  Pulse GPIO: %d", PULSE_GPIO);
    ESP_LOGI(TAG, "  Pedal GPIO: %d", PEDAL_GPIO);
    ESP_LOGI(TAG, "  Counter GPIO: %d", COUNTER_GPIO);
    ESP_LOGI(TAG, "  Default frequency: %d Hz", s_frequency);

    return ESP_OK;
}

void motor_start(void)
{
    if (!s_motor_running) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        s_motor_running = true;
        ESP_LOGI(TAG, "Motor started at %d Hz", s_frequency);
    }
}

void motor_stop(void)
{
    if (s_motor_running) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        s_motor_running = false;
        ESP_LOGI(TAG, "Motor stopped");
    }
}

esp_err_t motor_set_frequency(uint16_t freq_hz)
{
    if (freq_hz < CONFIG_STEPPER_MIN_FREQ || freq_hz > CONFIG_STEPPER_MAX_FREQ) {
        ESP_LOGW(TAG, "Frequency %d Hz out of range [%d, %d]",
                 freq_hz, CONFIG_STEPPER_MIN_FREQ, CONFIG_STEPPER_MAX_FREQ);
        return ESP_ERR_INVALID_ARG;
    }

    s_frequency = freq_hz;

    esp_err_t ret = ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq_hz);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Frequency set to %d Hz", freq_hz);
    }
    return ret;
}

uint16_t motor_get_frequency(void)
{
    return s_frequency;
}

void motor_set_mode(motor_mode_t mode)
{
    s_mode = mode;
    ESP_LOGI(TAG, "Mode set to %d", mode);
    motor_control_update();  // Update immediately
}

motor_mode_t motor_get_mode(void)
{
    return s_mode;
}

void motor_get_status(motor_status_t *status)
{
    if (status == NULL) return;

    status->pedal_pressed = s_pedal_pressed;
    status->counter_complete = s_counter_complete;
    status->motor_running = s_motor_running;
    status->mode = s_mode;
    status->frequency = s_frequency;
}

void motor_control_update(void)
{
    // Read current input states (in case ISR was missed)
    s_pedal_pressed = gpio_get_level(PEDAL_GPIO);
    s_counter_complete = gpio_get_level(COUNTER_GPIO);

    bool should_run = false;

    switch (s_mode) {
        case MOTOR_MODE_MANUAL_ON:
            // Manual ON: run unless counter is complete
            should_run = !s_counter_complete;
            break;

        case MOTOR_MODE_MANUAL_OFF:
            // Manual OFF: never run
            should_run = false;
            break;

        case MOTOR_MODE_AUTO:
        default:
            // Auto mode: control logic
            // If counter complete (HIGH) -> don't run
            // If counter not complete (LOW) AND pedal pressed (HIGH) -> run
            if (s_counter_complete) {
                should_run = false;
            } else if (s_pedal_pressed) {
                should_run = true;
            } else {
                should_run = false;
            }
            break;
    }

    // Apply motor state
    if (should_run && !s_motor_running) {
        motor_start();
    } else if (!should_run && s_motor_running) {
        motor_stop();
    }
}
