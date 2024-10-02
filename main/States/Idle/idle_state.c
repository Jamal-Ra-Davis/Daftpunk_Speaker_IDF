#include "idle_state.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "Framebuffer.h"
#include "Font.h"
#include "global_defines.h"
#include "bt_audio.h"
#include "Time_Helpers.h"

#define TAG "IDLE_STATE"
#define IDLE_TIMEOUT_MS 45000
#define IDLE_DELAY_MS 50

/*******************************
 * Data Type Definitions
 ******************************/

/*******************************
 * Global Data
 ******************************/
static TimerHandle_t xTimer;
static bool idle_timeout = false;
static int idx = 0;
static const char *idle_str = "IDLE";
static int idle_str_len;

static char time_buf[64];
static int prev_sec;
/*******************************
 * Function Prototypes
 ******************************/
static void idle_timeout_func(TimerHandle_t xTimer);
//static void get_time_components(time_t *ts, int *hour, int *min, int *sec, bool *am);

/*******************************
 * Private Function Definitions
 ******************************/
static void idle_timeout_func(TimerHandle_t xTimer)
{
    idle_timeout = true;
}

/*******************************
 * Public Function Definitions
 ******************************/
int idle_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "idle_state_init");
    xTimer = xTimerCreate("Idle_Timer", MS_TO_TICKS(IDLE_TIMEOUT_MS), pdFALSE, NULL, idle_timeout_func);
    idle_str_len = get_str_width(idle_str);
    return 0;
}
int idle_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "idle_state_on_enter");

    // Get timestamp
    time_t now;
    time(&now);
    int hour, min;
    bool am;
    get_time_components(&now, &hour, &min, &prev_sec, &am);
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d %s", hour, min, prev_sec, (am) ? "AM" : "PM");

    idx = FRAME_BUF_COLS;

    // Turn off LED
    set_rgb_state(RGB_MANUAL);
    set_rgb_led(0, 0, 0);

    // Start timer
    if (xTimerStart(xTimer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start timeout timer");
    }
    idle_timeout = false;

    // Set delay for idle state
    state_manager_context_t *ctx = (state_manager_context_t *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }
    ctx->delay_ms = IDLE_DELAY_MS;
    return 0;
}
int idle_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "idle_state_on_exit");

    // Clear buffer
    buffer_clear(&display_buffer);
    buffer_update(&display_buffer);

    // Stop timer
    if (xTimerStop(xTimer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start timeout timer");
    }

    // Revert delay back to default value
    state_manager_context_t *ctx = (state_manager_context_t *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }
    ctx->delay_ms = DEFAULT_STATE_DELAY_MS;
    return 0;
}
int idle_state_update(state_manager_t *state_manager)
{
    ESP_LOGD(TAG, "idle_state_update");

    if (!state_manager)
    {
        return 0;
    }

    state_manager_context_t *ctx = (state_manager_context_t *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }

    bool enter_pairing = false;
    bool enter_streaming = false;
    bool enter_menu = false;
    bool bluetooth_connected = bt_audio_connected();
    bool button_pressed = false;

    em_system_event_t event;
    QueueHandle_t event_queue = ctx->event_queue;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE)
    {
        ESP_LOGI(TAG, "Event Received: %d", (int)event);
        if (event == PAIR_LONG_PRESS)
        {
            enter_pairing = true;
            button_pressed = true;
        }
        if (event == PAIR_SHORT_PRESS)
        {
            enter_menu = true;
            button_pressed = true;
        }
        if (event == VOL_M_LONG_PRESS)
        {
            button_pressed = true;
        }
        if (event == VOL_M_SHORT_PRESS)
        {
            button_pressed = true;
        }
        if (event == VOL_P_LONG_PRESS)
        {
            button_pressed = true;
        }
        if (event == VOL_P_SHORT_PRESS)
        {
            button_pressed = true;
        }

        if (event == FIRST_AUDIO_PACKET)
        {
            enter_streaming = true;
        }
    }

    // State changes
    if (enter_pairing)
    {
        sm_change_state(state_manager, PAIRING_STATE_);
        return 0;
    }

    if (enter_streaming)
    {
        sm_change_state(state_manager, STREAMING_STATE_);
        return 0;
    }

    if (enter_menu)
    {
        sm_change_state(state_manager, MENU_STATE_);
        return 0;
    }

    if (idle_timeout && !button_pressed)
    {
        if (bluetooth_connected)
        {
            sm_change_state(state_manager, DISPLAY_OFF_STATE_);
        }
        else
        {
            sm_change_state(state_manager, SLEEP_STATE_);
        }
        return 0;
    }

    // State behavior
    if (button_pressed)
    {
        ESP_LOGI(TAG, "Button pressed, resetting idle timer");
        if (xTimerStart(xTimer, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to start timeout timer");
        }
    }

    /*
    if (bt_audio_enabled() && !bt_audio_connected()) {
        set_rgb_state(RGB_PAIRING);
    }
    */

    // Display system time
    time_t now;
    time(&now);
    int hour, min, sec;
    bool am;
    get_time_components(&now, &hour, &min, &sec, &am);

    if (sec != prev_sec) {
        ESP_LOGI(TAG, "Time = %02d:%02d:%02d %s", hour, min, sec, (am) ? "AM" : "PM");
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d %s", hour, min, prev_sec, (am) ? "AM" : "PM");
    }
    prev_sec = sec;

    buffer_clear(&display_buffer);
    draw_str(time_buf, 0, 2, &display_buffer);
    buffer_update(&display_buffer);
    return 0;

    /*
    // Display IDLE marquee text
    buffer_clear(&display_buffer);
    draw_str(idle_str, idx, 2, &display_buffer);
    buffer_update(&display_buffer);
    if (--idx <= -idle_str_len)
    {
        idx = FRAME_BUF_COLS;
    }
    return 0;
    */
}