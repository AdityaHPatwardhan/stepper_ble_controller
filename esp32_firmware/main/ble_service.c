#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"
#include "ble_service.h"
#include "motor_control.h"

static const char *TAG = "ble_svc";

// Custom UUIDs (128-bit)
// Service UUID: 12345678-1234-5678-1234-56789abcdef0
static const ble_uuid128_t stepper_svc_uuid =
    BLE_UUID128_INIT(0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
                     0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

// Status characteristic UUID: 12345678-1234-5678-1234-56789abcdef1
static const ble_uuid128_t status_chr_uuid =
    BLE_UUID128_INIT(0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
                     0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

// Command characteristic UUID: 12345678-1234-5678-1234-56789abcdef2
static const ble_uuid128_t command_chr_uuid =
    BLE_UUID128_INIT(0xf2, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
                     0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

// Config characteristic UUID: 12345678-1234-5678-1234-56789abcdef3
static const ble_uuid128_t config_chr_uuid =
    BLE_UUID128_INIT(0xf3, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
                     0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

// Connection handle
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_status_chr_val_handle;
static bool s_notify_enabled = false;

// Characteristic value storage
static uint8_t s_status_value = 0;
static uint8_t s_command_value = BLE_CMD_AUTO_MODE;
static uint16_t s_config_value = CONFIG_STEPPER_DEFAULT_FREQ;
static uint8_t ble_addr_type;

// Forward declarations
static int stepper_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
static void ble_advertise(void);

// GATT service definition
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &stepper_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Status characteristic (read, notify)
                .uuid = &status_chr_uuid.u,
                .access_cb = stepper_chr_access,
                .val_handle = &s_status_chr_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                // Command characteristic (write)
                .uuid = &command_chr_uuid.u,
                .access_cb = stepper_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                // Config characteristic (read, write) - frequency
                .uuid = &config_chr_uuid.u,
                .access_cb = stepper_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {
                0,  // No more characteristics
            },
        },
    },
    {
        0,  // No more services
    },
};

static int stepper_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    int rc = 0;

    // Status characteristic
    if (ble_uuid_cmp(uuid, &status_chr_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            motor_status_t status;
            motor_get_status(&status);
            s_status_value = ble_pack_status(status.pedal_pressed,
                                             status.counter_complete,
                                             status.motor_running);
            rc = os_mbuf_append(ctxt->om, &s_status_value, sizeof(s_status_value));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    }
    // Command characteristic
    else if (ble_uuid_cmp(uuid, &command_chr_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len != 1) {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            rc = ble_hs_mbuf_to_flat(ctxt->om, &s_command_value, len, NULL);
            if (rc != 0) {
                return BLE_ATT_ERR_UNLIKELY;
            }

            ESP_LOGI(TAG, "Command received: 0x%02x", s_command_value);

            switch (s_command_value) {
                case BLE_CMD_STOP:
                    motor_set_mode(MOTOR_MODE_MANUAL_OFF);
                    break;
                case BLE_CMD_START:
                    motor_set_mode(MOTOR_MODE_MANUAL_ON);
                    break;
                case BLE_CMD_AUTO_MODE:
                default:
                    motor_set_mode(MOTOR_MODE_AUTO);
                    break;
            }
            return 0;
        }
    }
    // Config characteristic (frequency)
    else if (ble_uuid_cmp(uuid, &config_chr_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            s_config_value = motor_get_frequency();
            rc = os_mbuf_append(ctxt->om, &s_config_value, sizeof(s_config_value));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len != 2) {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            rc = ble_hs_mbuf_to_flat(ctxt->om, &s_config_value, len, NULL);
            if (rc != 0) {
                return BLE_ATT_ERR_UNLIKELY;
            }

            ESP_LOGI(TAG, "Config (frequency) received: %d Hz", s_config_value);

            if (motor_set_frequency(s_config_value) != ESP_OK) {
                return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
            }
            return 0;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Connection %s; status=%d",
                     event->connect.status == 0 ? "established" : "failed",
                     event->connect.status);
            if (event->connect.status == 0) {
                s_conn_handle = event->connect.conn_handle;
            } else {
                ble_advertise();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_notify_enabled = false;
            // Reset to auto mode on disconnect
            motor_set_mode(MOTOR_MODE_AUTO);
            ble_advertise();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising complete");
            ble_advertise();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "Subscribe event; attr_handle=%d, cur_notify=%d",
                     event->subscribe.attr_handle, event->subscribe.cur_notify);
            if (event->subscribe.attr_handle == s_status_chr_val_handle) {
                s_notify_enabled = event->subscribe.cur_notify;
            }
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU update: %d", event->mtu.value);
            break;
    }

    return 0;
}

static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));

    // Advertise flags
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Include device name
    fields.name = (uint8_t *)CONFIG_BLE_DEVICE_NAME;
    fields.name_len = strlen(CONFIG_BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    // Include TX power
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting adv fields: %d", rc);
        return;
    }

    // Set advertising parameters
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertising: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Advertising started");
}

static void ble_on_sync(void)
{
    int rc;

    // Use NRPA (non-resolvable private address) - simplest for dev
    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error inferring address type: %d", rc);
        return;
    }

    // Print device address
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(ble_addr_type, addr_val, NULL);
    if (rc == 0) {
        ESP_LOGI(TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x",
                 addr_val[5], addr_val[4], addr_val[3],
                 addr_val[2], addr_val[1], addr_val[0]);
    }

    ble_advertise();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset; reason=%d", reason);
}

static void nimble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_service_init(void)
{
    int rc;

    // Initialize NVS (required for BLE)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize NimBLE
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NimBLE port init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure NimBLE host
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    // Initialize GAP and GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Configure GATT server
    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT service count failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT service add failed: %d", rc);
        return ESP_FAIL;
    }

    // Set device name
    rc = ble_svc_gap_device_name_set(CONFIG_BLE_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to set device name: %d", rc);
    }

    // Start NimBLE host task
    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE service initialized");
    return ESP_OK;
}

bool ble_is_connected(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

void ble_notify_status(uint8_t status_byte)
{
    if (!s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&status_byte, sizeof(status_byte));
    if (om == NULL) {
        ESP_LOGW(TAG, "Failed to allocate mbuf for notification");
        return;
    }

    int rc = ble_gatts_notify_custom(s_conn_handle, s_status_chr_val_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "Notification failed: %d", rc);
    }
}
