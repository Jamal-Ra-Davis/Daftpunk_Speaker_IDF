#include "streaming_state.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "Framebuffer.h"
#include "Font.h"

#define TAG "STREAMING_STATE"


int streaming_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "streaming_state_init");
    return 0;
}
int streaming_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "streaming_state_on_enter");
    buffer_clear(&display_buffer);
    buffer_update(&display_buffer);
    return 0;
}
int streaming_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "streaming_state_on_exit");
    return 0;
}
int streaming_state_update(state_manager_t *state_manager)
{
    ESP_LOGD(TAG, "streaming_state_update");

    if (!state_manager) {
        return 0;
    }

    state_manager_context_t *ctx = (state_manager_context_t*)(state_manager->ctx);
    if (!ctx) {
        return 0;
    }

    bool exit_streaming = false;
    em_system_event_t event;
    QueueHandle_t event_queue = ctx->event_queue;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Event Received: %d", (int)event);
        if (event == STREAMING_TIMEOUT) {
            exit_streaming = true;
        }
    }
    
    if (exit_streaming) {
        sm_change_state(state_manager, IDLE_STATE_);
        return 0;
    }
    
    return 0;
}