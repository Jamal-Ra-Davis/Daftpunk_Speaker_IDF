#include "menu_state.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "Framebuffer.h"
#include "Font.h"
#include "Time_Helpers.h"

#define TAG "MENU_STATE"

// TODO: Decide on functionality for menu state, implement, and clean up this module

static int idx = 10;

char buf[32];

int set_hour;
int set_min;
int flash_cnt = 0;
int flash_max = 20;
char time_buf[64];

typedef enum
{
    SET_TIME_IDLE,
    SET_TIME_HOURS,
    SET_TIME_MINUTES,
} set_time_state_t;
set_time_state_t time_idx = SET_TIME_IDLE;

int menu_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "menu_state_init");
    return 0;
}
int menu_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "menu_state_on_enter");
    snprintf(buf, sizeof(buf), "IDX:%d", idx);

    // Get timestamp
    time_t now;
    time(&now);
    int hour, min, sec;
    bool am;
    get_time_components(&now, &hour, &min, &sec, &am);
    set_hour = hour;
    set_min = min;
    time_idx = SET_TIME_HOURS;

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

    if (!state_manager)
    {
        return 0;
    }

    state_manager_context_t *ctx = (state_manager_context_t *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }

    bool next = false;
    bool back = false;
    bool select = false;
    bool cancel = false;
    em_system_event_t event;
    QueueHandle_t event_queue = ctx->event_queue;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE)
    {
        ESP_LOGI(TAG, "Event Received: %d", (int)event);
        if (event == VOL_P_SHORT_PRESS)
        {
            next = true;
        }
        if (event == VOL_M_SHORT_PRESS)
        {
            back = true;
        }
        if (event == PAIR_SHORT_PRESS)
        {
            select = true;
        }
        if (event == PAIR_LONG_PRESS)
        {
            cancel = true;
        }
    }

    if (next && back)
    {
        next = false;
        back = false;
    }

    if (select)
    {
        // ESP_LOGI(TAG, "Menu Selected: %d", idx);
        if (time_idx == SET_TIME_IDLE)
        {
            time_idx = SET_TIME_HOURS;
        }
        else if (time_idx == SET_TIME_HOURS)
        {
            time_idx = SET_TIME_MINUTES;
        }
        else
        {
            time_idx = SET_TIME_IDLE;
            // Set time
            ESP_LOGI(TAG, "Setting time to %02d:%02d", set_hour, set_min);
            
            /*
            struct tm timeinfo;
            timeinfo.tm_hour = 5;//set_hour;
            timeinfo.tm_min = 55;//set_min;
            timeinfo.tm_sec = 35;
            time_t new_time = mktime(&timeinfo);
            */
            //struct timeval tv = {
            //    .tv_sec = new_time,
            //};
            struct timeval tv;
            tv.tv_sec = 1727802395;
            int ret = settimeofday(&tv, NULL);        
            if (ret != 0) {
                ESP_LOGI(TAG, "Failed to set time: %d", (int)ret);
            }
            else {
                ESP_LOGI(TAG, "Successfully set time");
            }
            setenv("TZ", "PST", 1);
            //setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/ 3", 1); // https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
            tzset();

            //set_time_components(set_hour, set_min, 0);
            sm_change_state(state_manager, IDLE_STATE_);
        }
    }
    else if (cancel)
    {
        sm_change_state(state_manager, IDLE_STATE_);
        return 0;
    }
    else if (next)
    {
        /*
        if (idx < 20)
        {
            idx++;
        }
        snprintf(buf, sizeof(buf), "IDX:%d", idx);
        */

        if (time_idx == SET_TIME_HOURS)
        {
            set_hour = (set_hour + 1) % 24;
        }
        else if (time_idx == SET_TIME_MINUTES)
        {
            set_min = (set_min + 1) % 60;
        }
    }
    else if (back)
    {
        /*
        if (idx > 0)
        {
            idx--;
        }
        snprintf(buf, sizeof(buf), "IDX:%d", idx);
        */

        if (time_idx == SET_TIME_HOURS)
        {
            if (set_hour > 0)
            {
                set_hour--;
            }
            else
            {
                set_hour = 23;
            }
            ESP_LOGI(TAG, "Set Hour: %d", set_hour);
        }
        else if (time_idx == SET_TIME_MINUTES)
        {
            if (set_min > 0)
            {
                set_min--;
            }
            else
            {
                set_min = 59;
            }
            ESP_LOGI(TAG, "Set Minute: %d", set_min);
        }
    }

    /*
    buffer_clear(&display_buffer);
    draw_str(buf, 2, 2, &display_buffer);
    buffer_update(&display_buffer);
    */

    bool am = (set_hour < 12);
    bool blank = (flash_cnt < flash_max/2);
    int hour_12 = convert_24hour_to_12hour(set_hour);
    char hour_str[8];
    char min_str[8];

    snprintf(hour_str, sizeof(hour_str), "%02d", hour_12);
    snprintf(min_str, sizeof(min_str), "%02d", set_min);

    if (time_idx == SET_TIME_HOURS && blank) {
        snprintf(hour_str, sizeof(hour_str), "  ");
    }
    else if (time_idx == SET_TIME_MINUTES && blank) {
        snprintf(min_str, sizeof(min_str), "  ");
    }

    snprintf(time_buf, sizeof(time_buf), "%s:%s %s", hour_str, min_str, (am) ? "AM" : "PM");
    buffer_clear(&display_buffer);
    draw_str(time_buf, 0, 2, &display_buffer);
    buffer_update(&display_buffer);
    flash_cnt = (flash_cnt + 1) % flash_max;
    return 0;
}