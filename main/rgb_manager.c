#include "rgb_manager.h"
#include "global_defines.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "led_strip.h"
#include "esp_log.h"
#include <math.h>

#define RGB_MANAGER_TASK_STACK_SIZE 2048
#define RGB_TAG "RGB_MANAGER"
#define RMT_TX_CHANNEL RMT_CHANNEL_0

static rgb_states_t current_state;
static rgb_states_t cached_state;
static TaskHandle_t xrgb_manager_task = NULL;

static void rgb_manager_task(void *pvParameters);

static void timer_thread_task(void *pvParameters);
static void led_low_battery();
static void _oneshot_blink();
static void rgb_led_cycle();

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_data_t;

typedef struct
{
    int cnt;
    int period;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} oneshot_blink_data_t;

static oneshot_blink_data_t oneshot_blink_data;
static led_strip_t *strip = NULL;
static rgb_data_t rgb_data;

static void rgb_pairing();

esp_err_t init_rgb_manager()
{
    esp_err_t esp_ret;
    gpio_config_t gp_cfg = {
        .pin_bit_mask = GPIO_SEL_17,
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_ret = gpio_config(&gp_cfg);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(RGB_TAG, "Failed to configure RGB led enable pin");
        return esp_ret;
    }
    
    esp_ret = gpio_set_level(GPIO_NUM_17, 1);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(RGB_TAG, "Failed to assert RGB led enable pin");
        return esp_ret;
    }

    // set counter clock to 40MHz
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(RGB_LED_DATA, RMT_TX_CHANNEL);
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(1, (led_strip_dev_t)config.channel);
    strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!strip)
    {
        ESP_LOGE(RGB_TAG, "install WS2812 driver failed");
        return ESP_FAIL;
    }
    set_rgb_led(0, 0, 0);

    current_state = RGB_MANUAL;
    cached_state = current_state;
    xTaskCreate(
        rgb_manager_task,
        "RGB_Manager_Task",          // A name just for humans
        RGB_MANAGER_TASK_STACK_SIZE, // Stack size
        NULL,
        RGB_MANAGER_TASK_PRIORITY, // priority
        &xrgb_manager_task);
    return 0;
}
TaskHandle_t rgb_manager_task_handle()
{
    return xrgb_manager_task;
}
int set_rgb_state(rgb_states_t state)
{
    current_state = state;
    return 0;
}
rgb_states_t get_rgb_state()
{
    return current_state;
}
int oneshot_blink(int cnt, int period, uint8_t r, uint8_t g, uint8_t b)
{
    if (current_state == BLINK_N)
    {
        return 0;
    }
    cached_state = current_state;
    current_state = BLINK_N;

    oneshot_blink_data.cnt = cnt;
    oneshot_blink_data.period = period;
    oneshot_blink_data.r = r;
    oneshot_blink_data.g = g;
    oneshot_blink_data.b = b;
    return 0;
}
void set_rgb_led(uint8_t r, uint8_t g, uint8_t b)
{
    rgb_data.r = r;
    rgb_data.g = g;
    rgb_data.b = b;
    strip->set_pixel(strip, 0, rgb_data.r, rgb_data.g, rgb_data.b);
    strip->refresh(strip, 100);
}

static void rgb_manager_task(void *pvParameters)
{
    static int cnt = 0;
    ESP_LOGI(RGB_TAG, "RGB Manager task started");
    while (1)
    {
        switch (current_state)
        {
        case RGB_1HZ_CYCLE:
            rgb_led_cycle();
            break;
        case RGB_LOW_BATTERY:
            led_low_battery();
            break;
        case RGB_PAIRING:
            rgb_pairing();
            break;
        case BLINK_N:
            _oneshot_blink();
            break;
        default:
            vTaskDelay(pdMS_TO_TICKS(250));
            break;
        }
    }
}
static void led_low_battery()
{
    static uint8_t brightness = 0x1;
    static bool increasing = true;

    if (brightness == 0x01)
    {
        increasing = true;
    }
    else if (brightness == 0xFF)
    {
        increasing = false;
    }

    if (increasing)
    {
        brightness = (brightness << 1) | 0x01;
    }
    else
    {
        brightness = brightness >> 1;
    }

    set_rgb_led(brightness, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(25));
}
static void _oneshot_blink()
{
    for (int i = 0; i < oneshot_blink_data.cnt; i++)
    {
        set_rgb_led(oneshot_blink_data.r, oneshot_blink_data.g, oneshot_blink_data.b);
        vTaskDelay(pdMS_TO_TICKS(oneshot_blink_data.period / 2));

        set_rgb_led(0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(oneshot_blink_data.period / 2));
    }
    current_state = cached_state;
}
static void rgb_led_cycle()
{
    static const rgb_data_t colors[3] = {
        {.r = 255, .g = 0, .b = 0},
        {.r = 0, .g = 255, .b = 0},
        {.r = 0, .g = 0, .b = 255},
    };
    static uint8_t i = 0;
    set_rgb_led(colors[i].r, colors[i].g, colors[i].b);
    i = (i + 1) % 3;
    vTaskDelay(pdMS_TO_TICKS(1000));
}
static void rgb_pairing()
{
    //static uint8_t brightness = 0x1;
    //static bool increasing = true;

    
    static int cnt = 0;
    float brightness = sin((2*3.14*cnt) / 64.0);
    brightness = (brightness / 2.0) + 0.5;
    brightness = (100*brightness) + 10;
    cnt = (cnt + 1) % 64;
    

    //float x = sin(3.14159 / 4.0);
    //ESP_LOGI(RGB_TAG, "sin(pi/4) = %f", x);
    //ESP_LOGI(RGB_TAG, "Test");

    /*
    if (brightness == 0x01)
    {
        increasing = true;
    }
    else if (brightness == 0x7F)
    {
        increasing = false;
    }

    if (increasing)
    {
        brightness = (brightness << 1) | 0x01;
    }
    else
    {
        brightness = brightness >> 1;
    }
    */

    set_rgb_led(0, (uint8_t)(brightness/2), (uint8_t)brightness);
    vTaskDelay(pdMS_TO_TICKS(30));
    //vTaskDelay(pdMS_TO_TICKS(1000));
}