#include "pairing_state.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "rgb_manager.h"
#include "Events.h"
#include "Font.h"
#include "Framebuffer.h"
#include "global_defines.h"
#include "bt_audio.h"
#include "Animation.h"

#if defined(CONFIG_WIFI_ENABLED)
#include "tcp_shell.h"
#endif

#define TAG "PAIRING_STATE"
#define PAIRING_TIMEOUT_MS 30000

/*******************************
 * Data Type Definitions
 ******************************/
struct pairing_state_ctx
{
    animation_sequence_t eye_animation;
    int idx;
};

typedef enum
{
    PAIR_STATE_SEARCHING,
    PAIR_STATE_EYES,
    PAIR_STATE_CODE,
    NUM_PAIRING_STATES
} pair_state_t;

/*******************************
 * Function Prototypes
 ******************************/
static void pairing_timeout_func(TimerHandle_t xTimer);

static int pairing_state_searching_init(state_manager_t *state_manager);
static int pairing_state_searching_on_enter(state_manager_t *state_manager);
static int pairing_state_searching_on_exit(state_manager_t *state_manager);
static int pairing_state_searching_update(state_manager_t *state_manager);

static int pairing_state_eyes_init(state_manager_t *state_manager);
static int pairing_state_eyes_on_enter(state_manager_t *state_manager);
static int pairing_state_eyes_on_exit(state_manager_t *state_manager);
static int pairing_state_eyes_update(state_manager_t *state_manager);

static int pairing_state_code_init(state_manager_t *state_manager);
static int pairing_state_code_on_enter(state_manager_t *state_manager);
static int pairing_state_code_on_exit(state_manager_t *state_manager);
static int pairing_state_code_update(state_manager_t *state_manager);

/*******************************
 * Global Data
 ******************************/
static TimerHandle_t xTimer;
static bool pairing_timeout = false;
static char *bt_name = NULL;
static int bt_name_len = 0;
static int idx = 0;

#if defined(CONFIG_WIFI_ENABLED)
static bool valid_ip = false;
static int ip_address_len = 0;
static char ip_address[64] = {'\0'};
#endif

static state_manager_t pairing_state_manager;
static struct pairing_state_ctx state_ctx;
static animation_frame_t animation_frames[] = {
    {
        .id = BITMAP_EYE_OPEN,
        .hold_cnt = 15,
    },
    {
        .id = BITMAP_EYE_OPENING,
        .hold_cnt = 5,
    },
    {
        .id = BITMAP_EYE_CLOSED,
        .hold_cnt = 5,
    },
    {
        .id = BITMAP_EYE_OPENING,
        .hold_cnt = 5,
    },
    {
        .id = BITMAP_EYE_OPEN,
        .hold_cnt = 15,
    },
    {
        .id = BITMAP_EYE_LEFT,
        .hold_cnt = 15,
    },
    {
        .id = BITMAP_EYE_OPEN,
        .hold_cnt = 5,
    },
    {
        .id = BITMAP_EYE_RIGHT,
        .hold_cnt = 15,
    },
};
static state_element_t pairing_state_searching = {
    .init = pairing_state_searching_init,
    .on_enter = pairing_state_searching_on_enter,
    .on_exit = pairing_state_searching_on_exit,
    .update = pairing_state_searching_update,
};
static state_element_t pairing_state_eyes = {
    .init = pairing_state_eyes_init,
    .on_enter = pairing_state_eyes_on_enter,
    .on_exit = pairing_state_eyes_on_exit,
    .update = pairing_state_eyes_update,
};
static state_element_t pairing_state_code = {
    .init = pairing_state_code_init,
    .on_enter = pairing_state_code_on_enter,
    .on_exit = pairing_state_code_on_exit,
    .update = pairing_state_code_update,
};

/*******************************
 * Private Function Definitions
 ******************************/
static void pairing_timeout_func(TimerHandle_t xTimer)
{
    pairing_timeout = true;
}

/*******************************
 * Public Function Definitions
 ******************************/
int pairing_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_init");
    xTimer = xTimerCreate("Pair_Timer", MS_TO_TICKS(PAIRING_TIMEOUT_MS), pdFALSE, NULL, pairing_timeout_func);

    // Init pairing state animation
    animation_sequence_init(&state_ctx.eye_animation, animation_frames, sizeof(animation_frames) / sizeof(animation_frame_t));

    sm_setup_state_manager(&pairing_state_manager, NUM_PAIRING_STATES);
    sm_register_state(&pairing_state_manager, PAIR_STATE_SEARCHING, pairing_state_searching);
    sm_register_state(&pairing_state_manager, PAIR_STATE_EYES, pairing_state_eyes);
    sm_register_state(&pairing_state_manager, PAIR_STATE_CODE, pairing_state_code);
    sm_init(&pairing_state_manager, PAIR_STATE_SEARCHING, (void *)(&state_ctx));
    return 0;
}
int pairing_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_on_enter");

    // Check if bluetooth is enabled, and enable if not
    if (!bt_audio_enabled())
    {
        bt_audio_init();
    }

    // Flash LED to indicate pairing attempt in progress
    set_rgb_state(RGB_PAIRING);

    // Start timer
    if (xTimerStart(xTimer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start timeout timer");
    }
    pairing_timeout = false;

    idx = FRAME_BUF_COLS;
    bt_name = bt_audio_get_device_name();
    if (bt_name)
    {
        bt_name_len = get_str_width(bt_name);
    }

#if defined(CONFIG_WIFI_ENABLED)
    valid_ip = (get_tcp_ip(ip_address, sizeof(ip_address)) > 0);
    if (valid_ip)
    {
        ip_address_len = get_str_width(ip_address);
    }
#endif

    sm_change_state(&pairing_state_manager, PAIR_STATE_SEARCHING);
    return 0;
}
int pairing_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_on_exit");

    // Stop timer
    if (xTimerStop(xTimer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start timeout timer");
    }
    return 0;
}
int pairing_state_update(state_manager_t *state_manager)
{
    ESP_LOGD(TAG, "pairing_state_update");

    if (!state_manager)
    {
        return 0;
    }

    state_manager_context_t *ctx = (state_manager_context_t *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }

    bool pair_connected = false;
    bool pair_connecting = false;
    bool pair_exit = false;
    bool bluetooth_connected = bt_audio_connected();
    em_system_event_t event;
    QueueHandle_t event_queue = ctx->event_queue;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE)
    {
        ESP_LOGI(TAG, "Event Received: %d", (int)event);
        if (event == BT_AUDIO_CONNECTING)
        {
            pair_connecting = true;
        }
        if (event == BT_AUDIO_CONNECTED)
        {
            pair_connected = true;
        }
        if (event == PAIR_LONG_PRESS)
        {
            pair_exit = true;
        }
#if defined(CONFIG_WIFI_ENABLED)
        if (event == WIFI_READY)
        {
            ESP_LOGI(TAG, "WIFI READY");
            valid_ip = (get_tcp_ip(ip_address, sizeof(ip_address)) > 0);
            if (valid_ip)
            {
                ip_address_len = get_str_width(ip_address);
            }
        }
        if (event == WIFI_CONNECTED)
        {
            pair_connected = true;
        }
#endif
    }

    // State changes:
    // - On successful pair or bluetooth already connected, enter PAIR_SUCCESS state
    // - On timeout, enter PAIR_FAIL state
    // - On manaul exit, enter PAIR_FAIL state
    if (pair_connected || bluetooth_connected)
    {
        sm_change_state(state_manager, PAIR_SUCCESS_STATE_);
        return 0;
    }
    if (pairing_timeout || pair_exit)
    {
        sm_change_state(state_manager, PAIR_FAIL_STATE_);
        return 0;
    }

    // State action
    if (pair_connecting)
    {
        set_rgb_state(RGB_MANUAL);
        set_rgb_led(50, 100, 0);
    }

    sm_update(&pairing_state_manager);
    return 0;
}

// TODO: Move pairing state machine function groups to their own sub-modules
static int search_cnt = 0;
static int pairing_state_searching_init(state_manager_t *state_manager)
{
    return 0;
}
static int pairing_state_searching_on_enter(state_manager_t *state_manager)
{
    struct pairing_state_ctx *ctx = (struct pairing_state_ctx *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }
    ctx->idx = 2 * FRAME_BUF_COLS;
    search_cnt = 0;
    return 0;
}
static int pairing_state_searching_on_exit(state_manager_t *state_manager)
{
    return 0;
}
static int pairing_state_searching_update(state_manager_t *state_manager)
{
    struct pairing_state_ctx *ctx = (struct pairing_state_ctx *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }

    buffer_clear(&display_buffer);
    draw_str("SEARCHING FOR DEVICE", ctx->idx / 2, 2, &display_buffer);
    buffer_update(&display_buffer);

    if ((--ctx->idx / 2) <= -get_str_width("SEARCHING FOR DEVICE"))
    {
        ctx->idx = 2 * FRAME_BUF_COLS;
        search_cnt++;
        if (search_cnt >= 2)
        {
            sm_change_state(state_manager, PAIR_STATE_EYES);
        }
    }
    return 0;
}

static bool move_up = true;
static int eye_pos = 10;
static int pairing_state_eyes_init(state_manager_t *state_manager)
{
    return 0;
}
static int pairing_state_eyes_on_enter(state_manager_t *state_manager)
{
    struct pairing_state_ctx *ctx = (struct pairing_state_ctx *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }
    move_up = true;
    ctx->idx = 0;
    eye_pos = 10;
    animation_sequence_set_frame(&ctx->eye_animation, 2);
    return 0;
}
static int pairing_state_eyes_on_exit(state_manager_t *state_manager)
{
    return 0;
}
static int pairing_state_eyes_update(state_manager_t *state_manager)
{
    struct pairing_state_ctx *ctx = (struct pairing_state_ctx *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }

    buffer_clear(&display_buffer);
    animation_sequence_update(&ctx->eye_animation);
    animation_sequence_draw(&ctx->eye_animation, 4, 2, &display_buffer);
    animation_sequence_draw(&ctx->eye_animation, 18, 2, &display_buffer);
    buffer_update(&display_buffer);

    uint8_t frame_idx = 0;
    animation_sequence_get_frame(&ctx->eye_animation, &frame_idx);
    ctx->idx++;
    if (ctx->idx >= 500 && frame_idx == 2)
    {
        sm_change_state(state_manager, PAIR_STATE_CODE);
    }
    return 0;
}

static int code_cnt = 0;
static int pairing_state_code_init(state_manager_t *state_manager)
{
    return 0;
}
static int pairing_state_code_on_enter(state_manager_t *state_manager)
{
    struct pairing_state_ctx *ctx = (struct pairing_state_ctx *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }
    ctx->idx = 2 * FRAME_BUF_COLS;
    code_cnt = 0;
    return 0;
}
static int pairing_state_code_on_exit(state_manager_t *state_manager)
{
    return 0;
}
static int pairing_state_code_update(state_manager_t *state_manager)
{
    struct pairing_state_ctx *ctx = (struct pairing_state_ctx *)(state_manager->ctx);
    if (!ctx)
    {
        return 0;
    }

    if (bt_name)
    {
        buffer_clear(&display_buffer);
        draw_str(bt_name, ctx->idx / 2, 2, &display_buffer);
        buffer_update(&display_buffer);

        if ((--ctx->idx / 2) <= -bt_name_len)
        {
            ctx->idx = 2 * FRAME_BUF_COLS;
            code_cnt++;
            if (code_cnt >= 2)
            {
                sm_change_state(state_manager, PAIR_STATE_SEARCHING);
            }
        }
    }
#if defined(CONFIG_WIFI_ENABLED)
    else if (valid_ip)
    {
        buffer_clear(&display_buffer);
        draw_str(ip_address, ctx->idx / 2, 2, &display_buffer);
        buffer_update(&display_buffer);

        if ((--ctx->idx / 2) <= -ip_address_len)
        {
            ctx->idx = 2 * FRAME_BUF_COLS;
            code_cnt++;
            if (code_cnt >= 5)
            {
                sm_change_state(state_manager, PAIR_STATE_SEARCHING);
            }
        }
    }
#endif
    else
    {
        sm_change_state(state_manager, PAIR_STATE_SEARCHING);
    }
    return 0;
}