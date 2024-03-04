#include "bt_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "bt_app_core.h"
#include "bt_app_av.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "driver/i2s.h"
#include "i2s_task.h"

// Bluetooth stuff
/* device name */
#define LOCAL_DEVICE_NAME "ESP_SPEAKER"

/* event for stack up */
enum
{
    BT_APP_EVT_STACK_UP = 0,
    BT_APP_EVT_STACK_DOWN,
};

static bool audio_enabled = false;
static SemaphoreHandle_t xWorkSem;
bool active_connection = false;

/********************************
 * STATIC FUNCTION DECLARATIONS
 *******************************/

/* GAP callback function */
static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
/* handler for bluetooth stack enabled events */
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param);

/*******************************
 * STATIC FUNCTION DEFINITIONS
 ******************************/

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    uint8_t *bda = NULL;

    switch (event)
    {
    /* when authentication completed, this event comes */
    case ESP_BT_GAP_AUTH_CMPL_EVT:
    {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(BT_AV_TAG, "authentication success: %s", param->auth_cmpl.device_name);
            esp_log_buffer_hex(BT_AV_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        }
        else
        {
            ESP_LOGE(BT_AV_TAG, "authentication failed, status: %d", param->auth_cmpl.stat);
        }
        break;
    }

#if (CONFIG_BT_SSP_ENABLED == true)
    /* when Security Simple Pairing user confirmation requested, this event comes */
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d", param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    /* when Security Simple Pairing passkey notified, this event comes */
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey: %d", param->key_notif.passkey);
        break;
    /* when Security Simple Pairing passkey requested, this event comes */
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
#endif

    /* when GAP mode changed, this event comes */
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode: %d", param->mode_chg.mode);
        break;
    /* when ACL connection completed, this event comes */
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
        bda = (uint8_t *)param->acl_conn_cmpl_stat.bda;
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT Connected to [%02x:%02x:%02x:%02x:%02x:%02x], status: 0x%x",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_conn_cmpl_stat.stat);
        break;
    /* when ACL disconnection completed, this event comes */
    case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
        bda = (uint8_t *)param->acl_disconn_cmpl_stat.bda;
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_ACL_DISC_CMPL_STAT_EVT Disconnected from [%02x:%02x:%02x:%02x:%02x:%02x], reason: 0x%x",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_disconn_cmpl_stat.reason);
        break;
    /* others */
    default:
    {
        ESP_LOGI(BT_AV_TAG, "event: %d", event);
        break;
    }
    }
}

static void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    ESP_LOGD(BT_AV_TAG, "%s event: %d", __func__, event);

    switch (event)
    {
    /* when do the stack up, this event comes */
    case BT_APP_EVT_STACK_UP:
    {
        esp_err_t err;
        esp_bt_dev_set_device_name(LOCAL_DEVICE_NAME);
        esp_bt_gap_register_callback(bt_app_gap_cb);

        assert(esp_avrc_ct_init() == ESP_OK);
        esp_avrc_ct_register_callback(bt_app_rc_ct_cb);
        assert(esp_avrc_tg_init() == ESP_OK);
        esp_avrc_tg_register_callback(bt_app_rc_tg_cb);

        esp_avrc_rn_evt_cap_mask_t evt_set = {0};
        esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
        assert(esp_avrc_tg_set_rn_evt_cap(&evt_set) == ESP_OK);

        err = esp_a2d_sink_init();
        if (err != ESP_OK)
        {
            ESP_LOGE(BT_AV_TAG, "%s esp_a2d_sink_init failed: %s\n", __func__, esp_err_to_name(err));
        }
        assert(err == ESP_OK);
        esp_a2d_register_callback(&bt_app_a2d_cb);
        esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);

        /* set discoverable and connectable mode, wait to be connected */
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        break;
    }
    case BT_APP_EVT_STACK_DOWN:
    {
        ESP_LOGE(BT_AV_TAG, "%s - BT_APP_EVT_STACK_DOWN called\n", __func__);
        /* set not discoverable and not connectable mode */
        esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

        // Deinit avrc before deiniting a2d
        assert(esp_avrc_ct_deinit() == ESP_OK);
        esp_avrc_ct_register_callback(NULL);
        assert(esp_avrc_tg_deinit() == ESP_OK);
        esp_avrc_tg_register_callback(NULL);

        // Unregister callbacks and deinit a2d sink
        esp_a2d_sink_register_data_callback(NULL);
        esp_a2d_register_callback(NULL);
        assert(esp_a2d_sink_deinit() == ESP_OK);

        // esp_avrc_rn_evt_cap_mask_t evt_set = {0};
        // esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
        // assert(esp_avrc_tg_set_rn_evt_cap(&evt_set) == ESP_OK);

        // esp_bt_dev_set_device_name(LOCAL_DEVICE_NAME);
        // esp_bt_gap_register_callback(bt_app_gap_cb);

        if (xSemaphoreGive(xWorkSem) != pdTRUE)
        {
            // Failed to give semaphore
            ESP_LOGE(BT_AV_TAG, "Failed to give semaphore");
        }
        break;
    }
    /* others */
    default:
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
}

void bt_audio_init()
{
    /* initialize NVS — it is used to store PHY calibration data */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    static bool first = true;
    /*
     * This example only uses the functions of Classical Bluetooth.
     * So release the controller memory for Bluetooth Low Energy.
     */
    if (first)
    {
        first = false;
        ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

        xWorkSem = xSemaphoreCreateBinary();
        if (xWorkSem == NULL)
        {
            ESP_LOGE(BT_AV_TAG, "Could not allocate work semaphore");
            assert(false);
        }
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_LOGI(BT_AV_TAG, "%s initializing controller\n", __func__);
    if ((err = esp_bt_controller_init(&bt_cfg)) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(BT_AV_TAG, "%s enabling controller\n", __func__);
    if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(BT_AV_TAG, "%s initializing bluedroid\n", __func__);
    if ((err = esp_bluedroid_init()) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(BT_AV_TAG, "%s enabling bluedroid\n", __func__);
    if ((err = esp_bluedroid_enable()) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

#if (CONFIG_BT_SSP_ENABLED == true)
    /* set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

    /* set default parameters for Legacy Pairing (use fixed pin code 1234) */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    pin_code[0] = '1';
    pin_code[1] = '2';
    pin_code[2] = '3';
    pin_code[3] = '4';
    esp_bt_gap_set_pin(pin_type, 4, pin_code);

    ESP_LOGI(BT_AV_TAG, "%s Starting bt app task\n", __func__);
    bt_app_task_start_up();

    /* bluetooth device name, connection mode and profile set up */
    ESP_LOGI(BT_AV_TAG, "%s Dispatching bt_av_hdl_stack_evt: BT_APP_EVT_STACK_UP\n", __func__);
    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL);

    audio_enabled = true;
}

void bt_audio_deinit()
{
    esp_err_t err;

    ESP_LOGI(BT_AV_TAG, "%s Dispatching bt_av_hdl_stack_evt: BT_APP_EVT_STACK_DOWN\n", __func__);
    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_STACK_DOWN, NULL, 0, NULL);

    // Wait for dispatched work to complete
    if (xSemaphoreTake(xWorkSem, portMAX_DELAY) == pdFALSE)
    {
        ESP_LOGE(BT_AV_TAG, "Failed to take semaphore");
    }

    ESP_LOGI(BT_AV_TAG, "%s Shutting down bluetooth app task\n", __func__);
    bt_app_task_shut_down();

    ESP_LOGI(BT_AV_TAG, "%s Disabling bluedroid\n", __func__);
    if ((err = esp_bluedroid_disable()) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s disable bluedroid failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(BT_AV_TAG, "%s Deinitializing bluedroid\n", __func__);
    if ((err = esp_bluedroid_deinit()) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s deinitialize bluedroid failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(BT_AV_TAG, "%s Disaabling controller\n", __func__);
    if ((err = esp_bt_controller_disable()) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s disable controller failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(BT_AV_TAG, "%s Deinitializing controller\n", __func__);
    if ((err = esp_bt_controller_deinit()) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s deinitialize controller failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    active_connection = false;
    audio_enabled = false;
}

bool bt_audio_enabled()
{
    return audio_enabled;
}

bool bt_audio_connected()
{
    return active_connection;
}