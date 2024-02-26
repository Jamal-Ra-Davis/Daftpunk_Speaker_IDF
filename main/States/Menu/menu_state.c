#include "menu_state.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "Framebuffer.h"
#include "Font.h"

#define TAG "MENU_STATE"

static int idx = 10;

char buf[32];

int menu_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "menu_state_init");
    return 0;
}
int menu_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "menu_state_on_enter");
    snprintf(buf, sizeof(buf), "IDX:%d", idx);
    return 0;
}
int menu_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "menu_state_on_exit");
    return 0;
}
int menu_state_update(state_manager_t *state_manager)
{
    ESP_LOGD(TAG, "menu_state_update");

    if (!state_manager) {
        return 0;
    }

    state_manager_context_t *ctx = (state_manager_context_t*)(state_manager->ctx);
    if (!ctx) {
        return 0;
    }

    bool next = false;
    bool back = false;
    bool select = false;
    bool cancel = false;
    system_event_t event;
    QueueHandle_t event_queue = ctx->event_queue;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Event Received: %d", (int)event);
        if (event == VOL_P_SHORT_PRESS) {
            next = true;
        }
        if (event == VOL_M_SHORT_PRESS) {
            back = true;
        }
        if (event == PAIR_SHORT_PRESS) {
            select = true;
        }
        if (event == PAIR_LONG_PRESS) {
            cancel = true;
        }
    }

    if (next && back) {
        next = false;
        back = false;
    }
    
    if (select) {
        ESP_LOGI(TAG, "Menu Selected: %d", idx);
    }
    else if (cancel) {
        sm_change_state(state_manager, IDLE_STATE_);
        return 0;
    }
    else if (next) {
        if (idx < 20) {
            idx++;
        }
        snprintf(buf, sizeof(buf), "IDX:%d", idx);
    }
    else if (back) {
        if (idx > 0) {
            idx--;
        }
        snprintf(buf, sizeof(buf), "IDX:%d", idx);
    }

    
    buffer_clear(&display_buffer);
    draw_str(buf, 2, 2, &display_buffer);
    buffer_update(&display_buffer);
    return 0;
}