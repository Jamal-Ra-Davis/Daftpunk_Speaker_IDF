#include "pairing_success_state.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "global_defines.h"

#define TAG "PAIRING_SUCCESS_STATE"
#define PAIRING_SUCCESS_TIMEOUT_MS 5000

/*******************************
 * Data Type Definitions
 ******************************/

/*******************************
 * Global Data
 ******************************/
static TimerHandle_t xTimer;
static bool pairing_success_timeout = false;

/*******************************
 * Function Prototypes
 ******************************/
static void pairing_success_timeout_func(TimerHandle_t xTimer);

/*******************************
 * Private Function Definitions
 ******************************/
static void pairing_success_timeout_func(TimerHandle_t xTimer)
{
    pairing_success_timeout = true;
}

/*******************************
 * Public Function Definitions
 ******************************/
int pairing_success_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_success_state_init");
    xTimer = xTimerCreate("Pair_Fail_Timer", MS_TO_TICKS(PAIRING_SUCCESS_TIMEOUT_MS), pdFALSE, NULL, pairing_success_timeout_func);
    return 0;
}
int pairing_success_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_success_state_on_enter");

    // Show success LED
    set_rgb_state(RGB_MANUAL);
    set_rgb_led(0, 100, 0);

    // Start timer
    if (xTimerStart(xTimer, 0) != pdPASS) 
    {
        ESP_LOGE(TAG, "Failed to start timeout timer");
    }
    pairing_success_timeout = false;
    return 0;
}
int pairing_success_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_success_state_on_exit");

    // Stop timer
    if (xTimerStop(xTimer, 0) != pdPASS) 
    {
        ESP_LOGE(TAG, "Failed to start timeout timer");
    }
    return 0;
}
int pairing_success_state_update(state_manager_t *state_manager)
{
    ESP_LOGD(TAG, "pairing_success_state_update");

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

    if (pairing_success_timeout) {
        sm_change_state(state_manager, IDLE_STATE_);
        return 0;
    }
    return 0;
}