#include "pairing_success_state.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"

#define TAG "PAIRING_SUCCESS_STATE"

static int cnt = 0;

int pairing_success_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_success_state_init");
    return 0;
}
int pairing_success_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_success_state_on_enter");
    // Flash LED to indicate pairing attempt in progress
    // Start timer
    //set_rgb_state(RGB_PAIRING);
    set_rgb_state(RGB_MANUAL);
    set_rgb_led(0, 100, 0);
    cnt = 0;
    return 0;
}
int pairing_success_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_success_state_on_exit");
    set_rgb_state(RGB_MANUAL);
    set_rgb_led(0, 0, 0);
    return 0;
}
int pairing_success_state_update(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_success_state_update");

    if (!state_manager) {
        return 0;
    }

    state_manager_context_t *ctx = (state_manager_context_t*)(state_manager->ctx);
    if (!ctx) {
        return 0;
    }

    system_event_t event;
    QueueHandle_t event_queue = ctx->event_queue;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Event Received: %d", (int)event);
    }

    ESP_LOGI(TAG, "CNT: %d", cnt);
    if (cnt >= 10) {
        sm_change_state(state_manager, IDLE_STATE_);
    }
    cnt++;
    
    // Wait for pairing event or timeout
    // If pairing successful, enter PAIR_SUCCESS state
    // If pairing failed, enter PAIR_FAIL state
    return 0;
}