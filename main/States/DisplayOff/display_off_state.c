#include "display_off_state.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "Framebuffer.h"
#include "Font.h"
#include "bt_audio.h"

#define TAG "DISPLAY_OFF_STATE"


int display_off_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "display_off_state_init");
    return 0;
}
int display_off_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "display_off_state_on_enter");
    buffer_clear(&display_buffer);
    buffer_update(&display_buffer);
    return 0;
}
int display_off_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "display_off_state_on_exit");
    return 0;
}
int display_off_state_update(state_manager_t *state_manager)
{
    ESP_LOGD(TAG, "display_off_state_update");

    if (!state_manager) {
        return 0;
    }

    state_manager_context_t *ctx = (state_manager_context_t*)(state_manager->ctx);
    if (!ctx) {
        return 0;
    }

    bool exit_display_off = false;
    bool enter_streaming = false;
    bool bluetooth_connected = bt_audio_connected();
    em_system_event_t event;
    QueueHandle_t event_queue = ctx->event_queue;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Event Received: %d", (int)event);
        if (event == VOL_P_SHORT_PRESS) {
            exit_display_off = true;
        }
        if (event == VOL_M_SHORT_PRESS) {
            exit_display_off = true;
        }
        if (event == PAIR_SHORT_PRESS) {
            exit_display_off = true;
        }
        if (event == PAIR_LONG_PRESS) {
            exit_display_off = true;
        }
        if (event == FIRST_AUDIO_PACKET) {
            enter_streaming = true;
        }
    }
    
    if (exit_display_off) {
        sm_change_state(state_manager, IDLE_STATE_);
        return 0;
    }

    if (!bluetooth_connected) {
        sm_change_state(state_manager, SLEEP_STATE_);
        return 0;
    }

    if (enter_streaming) {
        sm_change_state(state_manager, STREAMING_STATE_);
        return 0;
    }
    
    buffer_clear(&display_buffer);
    buffer_update(&display_buffer);
    return 0;
}