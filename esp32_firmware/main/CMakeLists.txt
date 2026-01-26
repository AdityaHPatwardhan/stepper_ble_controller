idf_component_register(
    SRCS "main.c" "motor_control.c" "ble_service.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver nvs_flash bt esp_driver_ledc esp_driver_gpio
)
