#include "pairing_state.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "global_defines.h"
#include "bt_audio.h"

#define TAG "PAIRING_STATE"
#define PAIRING_TIMEOUT_MS 30000

/*******************************
 * Data Type Definitions
 ******************************/

/*******************************
 * Global Data
 ******************************/
static TimerHandle_t xTimer;
static bool pairing_timeout = false;

/*******************************
 * Function Prototypes
 ******************************/
static void pairing_timeout_func(TimerHandle_t xTimer);

/*******************************
 * Private Function Definitions
 ******************************/
static void pairing_timeout_func(TimerHandle_t xTimer)
{
    pairing_timeout = true;
}

/*******************************
 * Public Function Definitions
 ******************************/
int pairing_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_init");
    xTimer = xTimerCreate("Pair_Timer", MS_TO_TICKS(PAIRING_TIMEOUT_MS), pdFALSE, NULL, pairing_timeout_func);
    return 0;
}
int pairing_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_on_enter");

    // Check if bluetooth is enabled, and enable if not
    if (!bt_audio_enabled()) {
        bt_audio_init();
    }

    // Flash LED to indicate pairing attempt in progress    
    set_rgb_state(RGB_PAIRING);

    // Start timer
    if (xTimerStart(xTimer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timeout timer");
    }
    pairing_timeout = false;
    return 0;
}
int pairing_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_on_exit");

    // Stop timer
    if (xTimerStop(xTimer, 0) != pdPASS) 
    {
        ESP_LOGE(TAG, "Failed to start timeout timer");
    }
    return 0;
}
int pairing_state_update(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_update");

    if (!state_manager) {
        return 0;
    }

    state_manager_context_t *ctx = (state_manager_context_t*)(state_manager->ctx);
    if (!ctx) {
        return 0;
    }

    bool pair_connected = false;
    bool pair_connecting = false;
    bool pair_exit = false;
    bool bluetooth_connected = bt_audio_connected();
    system_event_t event;
    QueueHandle_t event_queue = ctx->event_queue;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Event Received: %d", (int)event);
        if (event == BT_AUDIO_CONNECTING) {
            pair_connecting = true;
        }
        if (event == BT_AUDIO_CONNECTED) {
            pair_connected = true;
        }
        if (event == PAIR_LONG_PRESS) {
            pair_exit = true;
        }
    }

    // State changes:
        // - On successful pair or bluetooth already connected, enter PAIR_SUCCESS state
        // - On timeout, enter PAIR_FAIL state
        // - On manaul exit, enter PAIR_FAIL state
    if (pair_connected || bluetooth_connected) {
        sm_change_state(state_manager, PAIR_SUCCESS_STATE_);
        return 0;
    }
    if (pairing_timeout || pair_exit) {
        sm_change_state(state_manager, PAIR_FAIL_STATE_);
        return 0;
    }

    // State action
    if (pair_connecting) {
        set_rgb_state(RGB_MANUAL);
        set_rgb_led(50, 100, 0);
    }    
    return 0;
}