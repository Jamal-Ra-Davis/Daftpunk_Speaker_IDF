#include "idle_state.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "Framebuffer.h"
#include "Font.h"

#define TAG "IDLE_STATE"

static int cnt = 0;

int idle_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "idle_state_init");
    return 0;
}
int idle_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "idle_state_on_enter");
    // Flash LED to indicate pairing attempt in progress
    // Start Idle timer
    set_rgb_state(RGB_MANUAL);
    set_rgb_led(0, 0, 0);
    cnt = 0;
    return 0;
}
int idle_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "idle_state_on_exit");
    // stop timer
    buffer_clear(&display_buffer);
    buffer_update(&display_buffer);
    return 0;
}
int idle_state_update(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "idle_state_update");

    if (!state_manager) {
        return 0;
    }

    state_manager_context_t *ctx = (state_manager_context_t*)(state_manager->ctx);
    if (!ctx) {
        return 0;
    }

    bool enter_pairing = false;
    bool enter_streaming = false;
    bool enter_menu = false;
    bool bluetooth_connected = false;

    system_event_t event;
    QueueHandle_t event_queue = ctx->event_queue;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Event Received: %d", (int)event);
        if (event == PAIR_LONG_PRESS) {
            enter_pairing = true;
        }
        if (event == PAIR_SHORT_PRESS) {
            enter_menu = true;
        }
        if (event == FIRST_AUDIO_PACKET) {
            enter_streaming = true;
        }
    }

    // State changes
    if (enter_pairing) {
        sm_change_state(state_manager, PAIRING_STATE_);
        return 0;
    }

    if (enter_streaming) {
        sm_change_state(state_manager, STREAMING_STATE_);
        return 0;
    }

    if (enter_menu) {
        sm_change_state(state_manager, MENU_STATE_);
        return 0;
    }


    ESP_LOGI(TAG, "CNT: %d", cnt);
    if (cnt >= 45) {
        if (bluetooth_connected) {
            sm_change_state(state_manager, DISPLAY_OFF_STATE_);
        }
        else {
            sm_change_state(state_manager, SLEEP_STATE_);
        }
    }
    cnt++;

    // State behavior
    static const char* idle_str = "IDLE";
    buffer_clear(&display_buffer);
    draw_str(idle_str, 8, 2, &display_buffer);
    buffer_update(&display_buffer);

    // Wait for pairing event or timeout
    // If pairing successful, enter PAIR_SUCCESS state
    // If pairing failed, enter PAIR_FAIL state
    return 0;
}