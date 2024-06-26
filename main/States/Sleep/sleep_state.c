#include "sleep_state.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "Framebuffer.h"
#include "Font.h"
#include "bt_audio.h"
#include "i2s_task.h"
#include "audio_manager.h"

#define TAG "DISPLAY_OFF_STATE"

int sleep_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "sleep_state_init");
    return 0;
}
int sleep_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "sleep_state_on_enter");
    buffer_clear(&display_buffer);
    buffer_update(&display_buffer);

    // Block while playing audio as putting system to sleep will halt playback
    play_audio_sfx_blocking(AUDIO_SFX_SLEEP, portMAX_DELAY);

    if (bt_audio_enabled())
    {
        bt_audio_deinit();
    }
    return 0;
}
int sleep_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "sleep_state_on_exit");
    bt_i2s_task_start_up();
    play_audio_sfx(AUDIO_SFX_WAKE, false);
    return 0;
}
int sleep_state_update(state_manager_t *state_manager)
{
    ESP_LOGD(TAG, "sleep_state_update");

    if (!state_manager)
    {
        return 0;
    }

    state_manager_context_t *ctx = (state_manager_context_t *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }

    bool exit_sleep = false;
    em_system_event_t event;
    QueueHandle_t event_queue = ctx->event_queue;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE)
    {
        ESP_LOGI(TAG, "Event Received: %d", (int)event);
        if (event == PAIR_SHORT_PRESS)
        {
            exit_sleep = true;
        }
    }

    buffer_clear(&display_buffer);
    buffer_update(&display_buffer);

    // Setup wake source

    // Enter Sleep mode

    if (exit_sleep)
    {
        sm_change_state(state_manager, IDLE_STATE_);
        return 0;
    }
    return 0;
}