#include "pairing_state.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"

#define TAG "PAIRING_STATE"

static int cnt = 0;

int pairing_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_init");
    return 0;
}
int pairing_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_on_enter");
    // Flash LED to indicate pairing attempt in progress
    // Start timer
    set_rgb_state(RGB_PAIRING);
    //set_rgb_state(RGB_1HZ_CYCLE);
    cnt = 0;
    return 0;
}
int pairing_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_on_exit");
    set_rgb_state(RGB_LOW_BATTERY);
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
    }

    if (pair_connected) {
        sm_change_state(state_manager, PAIR_SUCCESS_STATE_);
    }
    else if (pair_connecting) {
        set_rgb_state(RGB_MANUAL);
        set_rgb_led(50, 100, 0);
    }
    else {
        ESP_LOGI(TAG, "CNT: %d", cnt);
        if (cnt >= 30) {
            sm_change_state(state_manager, PAIR_FAIL_STATE_);
        }
        cnt++;
    }

    
    // Wait for pairing event or timeout
    // If pairing successful, enter PAIR_SUCCESS state
    // If pairing failed, enter PAIR_FAIL state
    return 0;
}