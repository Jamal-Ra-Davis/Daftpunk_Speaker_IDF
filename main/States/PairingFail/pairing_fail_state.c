#include "pairing_fail_state.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "global_defines.h"
#include "bt_audio.h"
#include "Bitmap.h"
#include "Animation.h"
#include "Framebuffer.h"

#define TAG "PAIRING_FAIL_STATE"
#define PAIRING_FAIL_TIMEOUT_MS 5000

/*******************************
 * Data Type Definitions
 ******************************/

/*******************************
 * Global Data
 ******************************/
static TimerHandle_t xTimer;
static bool pairing_fail_timeout = false;
static animation_sequence_t arrow_animation;
static animation_frame_t arrow_frames[] = {
    {
        .id = BITMAP_ARROW_0,
        .hold_cnt = 5,
    },
    {
        .id = BITMAP_ARROW_1,
        .hold_cnt = 5,
    },
    {
        .id = BITMAP_ARROW_2,
        .hold_cnt = 5,
    },
    {
        .id = BITMAP_ARROW_3,
        .hold_cnt = 5,
    },
};


/*******************************
 * Function Prototypes
 ******************************/
static void pairing_fail_timeout_func(TimerHandle_t xTimer);

/*******************************
 * Private Function Definitions
 ******************************/
static void pairing_fail_timeout_func(TimerHandle_t xTimer)
{
    pairing_fail_timeout = true;
}

/*******************************
 * Public Function Definitions
 ******************************/
int pairing_fail_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_fail_state_init");
    xTimer = xTimerCreate("Pair_Fail_Timer", MS_TO_TICKS(PAIRING_FAIL_TIMEOUT_MS), pdFALSE, NULL, pairing_fail_timeout_func);
    animation_sequence_init(&arrow_animation, arrow_frames, sizeof(arrow_frames) / sizeof(animation_frame_t));
    return 0;
}
int pairing_fail_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_fail_state_on_enter");

    // Disable bluetooth
    if (bt_audio_enabled()) {
        bt_audio_deinit();
    }

    // Show error LED
    set_rgb_state(RGB_MANUAL);
    set_rgb_led(100, 0, 0);

    // Start timer
    if (xTimerStart(xTimer, 0) != pdPASS) 
    {
        ESP_LOGE(TAG, "Failed to start timeout timer");
    }
    pairing_fail_timeout = false;
    return 0;
}
int pairing_fail_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_fail_state_on_exit");
    set_rgb_state(RGB_MANUAL);
    set_rgb_led(0, 0, 0);

    // Stop timer
    if (xTimerStop(xTimer, 0) != pdPASS) 
    {
        ESP_LOGE(TAG, "Failed to start timeout timer");
    }
    return 0;
}
int pairing_fail_state_update(state_manager_t *state_manager)
{
    ESP_LOGD(TAG, "pairing_fail_state_update");

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

    if (pairing_fail_timeout) {
        sm_change_state(state_manager, IDLE_STATE_);
        return 0;
    }

    buffer_clear(&display_buffer);
    animation_sequence_update(&arrow_animation);
    for (int i=0; i<(FRAME_BUF_COLS); i+=8) {
        animation_sequence_draw(&arrow_animation, i, 0, &display_buffer);
    }
    buffer_update(&display_buffer);

    return 0;
}