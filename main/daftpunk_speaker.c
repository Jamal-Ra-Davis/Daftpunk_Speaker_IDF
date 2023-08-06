#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "driver/i2c.h"
#include "led_strip.h"
#include "esp_log.h"

#include "global_defines.h"
#include "Events.h"
#include "Buttons.h"
#include "FrameBuffer.h"
#include "sr_driver.h"
#include "Display_task.h"
#include "FFT_task.h"
#include "Font.h"
#include "system_states.h"
#include "bt_audio.h"
#include "MAX17048.h"
#include "rgb_manager.h"

#define MAIN_TAG "DAFTPUNK_SPEAKER"
#define RMT_TX_CHANNEL RMT_CHANNEL_0

#define I2C_MASTER_SCL_IO I2C_SCL_PIN /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO I2C_SDA_PIN /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM 0              /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ 400000     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0   /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0   /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000

system_state_t current_state = IDLE_STATE;
system_state_t prev_state = BOOT_STATE;

// TODO: Move volume callbacks to Volume module (may want to rethink how it interacts with Bluetooth audio library)
void volume_increase_cb(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "Volume Increase Pressed:");
    // volume_inc();
    int vol = (int)bt_audio_get_volume();
    vol += 7;
    if (vol > 127)
    {
        vol = 127;
    }
    bt_audio_set_volume((uint8_t)vol);
}
void volume_decrease_cb(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "Volume Decrease Pressed");
    // volume_dec();
    int vol = (int)bt_audio_get_volume();
    vol -= 7;
    if (vol < 0)
    {
        vol = 0;
    }
    bt_audio_set_volume((uint8_t)vol);
}
static void select_action(void *ctx)
{
    static bool triple_buffering = true;
    ESP_LOGI(MAIN_TAG, "Select button pressed - Create a2dp sink");
    ESP_LOGI(MAIN_TAG, "Creating a2dp sink...");
    // a2dp_sink.start("DevBoard_v0");
    // volume_init();
    // a2dp_sink.set_stream_reader(read_data_stream);
    if (current_state == SLEEP_STATE)
    {
        current_state = IDLE_STATE;
    }

    ESP_LOGI(MAIN_TAG, "%s triple buffering", (triple_buffering) ? "Enabling" : "Disabling");
    buffer_enable_triple_buffering(&double_buffer, triple_buffering);
    triple_buffering = !triple_buffering;
}

static volatile bool pair_press = false;
static void pair_action(void *ctx)
{
    pair_press = true;
    ESP_LOGI(MAIN_TAG, "Pair Button Pressed - Destroy a2dp sink");
    ESP_LOGI(MAIN_TAG, "Destroying a2dp sink...");
    // a2dp_sink.end(true);
}

static void charge_start_action(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "Charging started");
    // oneshot_blink(10, 100, 0, 128, 32);
}
static void charge_stop_action(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "Charging stopped");
    // oneshot_blink(10, 100, 128, 16, 16);
}
static void sleep_timer_func(TimerHandle_t xTimer)
{
    current_state = SLEEP_STATE;
}

static void bt_connected_action(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "BT Audio Connected");
    set_rgb_state(RGB_MANUAL);
    //oneshot_blink(5, 200, 100, 100, 100);
    set_rgb_led(60, 80, 60);
}
static void bt_disconnected_action(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "BT Audio Disconnected");
    set_rgb_state(RGB_PAIRING);
}
static void bt_connecting_action(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "BT Audio connecting");
    set_rgb_state(RGB_MANUAL);
    set_rgb_led(0, 100, 30);
}
void soc_change_cb(void *ctx)
{
    uint8_t soc;
    if (max17048_get_soc(&soc) == ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "SOC Changed! Battery SOC: %u%%", soc);
    }
    else
    {
        ESP_LOGE(MAIN_TAG, "Failed to read SOC");
    }
}
void soc_low_cb(void *ctx)
{
    ESP_LOGE(MAIN_TAG, "Battery level low, please recharge soon");
    set_rgb_state(RGB_LOW_BATTERY);
}

static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}



esp_err_t fuel_gauge_setup()
{
    esp_err_t esp_ret;
    esp_ret = max17048_init();
    if (esp_ret != ESP_OK)
    {
        return esp_ret;
    }

    esp_ret = max17048_register_alert_cb(MAX17048_ALERT_SOC_CHANGE, soc_change_cb, NULL);
    if (esp_ret != ESP_OK)
    {
        return esp_ret;
    }

    esp_ret = max17048_register_alert_cb(MAX17048_ALERT_SOC_LOW, soc_low_cb, NULL);
    if (esp_ret != ESP_OK)
    {
        return esp_ret;
    }

    esp_ret = max17048_enable_soc_change_alert(true);
    if (esp_ret != ESP_OK)
    {
        return esp_ret;
    }

    return esp_ret;
}

void app_main(void)
{
    esp_err_t esp_ret;
    bool init_success = true;
    int ret = 0;
    // Setup I2C (Can probably handle in driver)
    /*
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    */
    esp_ret = i2c_master_init();
    ESP_ERROR_CHECK(esp_ret);

    // Setup GPIOs
    /*
    pinMode(RGB_LED_EN, OUTPUT);
    pinMode(RGB_LED_DATA, OUTPUT);
    pinMode(AMP_SD_PIN, OUTPUT);

    digitalWrite(AMP_SD_PIN, LOW);
    digitalWrite(RGB_LED_EN, HIGH);

    pinMode(CHG_STAT_PIN, INPUT_PULLUP);
    */

    // Init Display task
    if (init_display_task() < 0)
    {
        ESP_LOGE(MAIN_TAG, "Failed to init display task");
        init_success = false;
    }

    // Init Logger (Probably not needed now)

    // Init event manager
    if (init_event_manager() < 0)
    {
        ESP_LOGE(MAIN_TAG, "Failed to init event manager");
        init_success = false;
    }

    // Register event callback functions
    ret |= register_event_callback(VOL_P_SHORT_PRESS, volume_increase_cb, NULL);
    ret |= register_event_callback(VOL_M_SHORT_PRESS, volume_decrease_cb, NULL);
    ret |= register_event_callback(PAIR_SHORT_PRESS, select_action, NULL);
    ret |= register_event_callback(PAIR_LONG_PRESS, pair_action, NULL);
    ret |= register_event_callback(CHARGE_START, charge_start_action, NULL);
    ret |= register_event_callback(CHARGE_STOP, charge_stop_action, NULL);

    ret |= register_event_callback(BT_AUDIO_CONNECTED, bt_connected_action, NULL);
    ret |= register_event_callback(BT_AUDIO_DISCONNECTED, bt_disconnected_action, NULL);
    ret |= register_event_callback(BT_AUDIO_CONNECTING, bt_connecting_action, NULL);

    // Init button manager
    if (init_buttons() < 0)
    {
        ESP_LOGE(MAIN_TAG, "Failed to init button manager");
        init_success = false;
    }

    // Init RGBW LED manager
    /*
    gpio_config_t gp_cfg = {
        .pin_bit_mask = GPIO_SEL_17,
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_ret = gpio_config(&gp_cfg);
    ESP_ERROR_CHECK(esp_ret);

    esp_ret = gpio_set_level(GPIO_NUM_17, 1);
    ESP_ERROR_CHECK(ret);

    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(RGB_LED_DATA, RMT_TX_CHANNEL);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(1, (led_strip_dev_t)config.channel);
    led_strip_t *strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!strip)
    {
        ESP_LOGE(MAIN_TAG, "install WS2812 driver failed");
        init_success = false;
    }
    strip->clear(strip, 100);

    for (int i = 0; i < 5; i++)
    {
        strip->set_pixel(strip, 0, 0, 128, 255);
        strip->refresh(strip, 100);
        vTaskDelay(pdMS_TO_TICKS(100));

        strip->set_pixel(strip, 0, 0, 0, 0);
        strip->refresh(strip, 100);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    */
    esp_ret = init_rgb_manager();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(MAIN_TAG, "Failed to init rgb manger");
        init_success = false;
    }
    set_rgb_state(RGB_PAIRING);

    // Display countdown timer
    for (int i = 10; i >= 0; i--)
    {
        buffer_clear(&double_buffer);
        draw_int(i, 30, 2, &double_buffer);
        buffer_update(&double_buffer);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Display boot text
    int test_str_len = get_str_width("DEVBOARD_V0");
    buffer_enable_triple_buffering(&double_buffer, false);
    for (int i = FRAME_BUF_COLS; i >= -test_str_len; i--)
    {
        buffer_clear(&double_buffer);
        draw_str("DEVBOARD_V0", i, 2, &double_buffer);
        buffer_update(&double_buffer);
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    vTaskDelay(200 / portTICK_PERIOD_MS);
    buffer_enable_triple_buffering(&double_buffer, true);
    for (int i = FRAME_BUF_COLS; i >= -test_str_len; i--)
    {
        buffer_clear(&double_buffer);
        draw_str("DEVBOARD_V0", i, 2, &double_buffer);
        buffer_update(&double_buffer);
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    // Perfor I2C bus scan

    // Init FFT Task
    if (init_fft_task() < 0)
    {
        ESP_LOGE(MAIN_TAG, "Failed to init FFT Task");
        init_success = false;
    }

    // Init fuel gague driver
    esp_ret = fuel_gauge_setup();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to setup fuel gauge");
        ESP_ERROR_CHECK(esp_ret);
        init_success = false;
    }

    // Init CLI task

    // Init timer thread manager and register timer threads

    // Init Bluetooth Audio and register reader callback
    bt_audio_register_data_cb(read_data_stream);
    bt_audio_init();

    TimerHandle_t sleep_timer = xTimerCreate("Sleep_Timer", MS_TO_TICKS(15000), pdFALSE, NULL, sleep_timer_func);

    
    int cnt = 0;
    char idle_str[32] = {'\0'};
    while (1)
    {
        bool on_enter = false;
        if (current_state != prev_state)
        {
            on_enter = true;
            prev_state = current_state;
        }

        switch (current_state)
        {
        case IDLE_STATE:
        {
            if (on_enter)
            {
                ESP_LOGI(MAIN_TAG, "Entering IDLE_STATE");
                if (xTimerStart(sleep_timer, 0) != pdPASS)
                {
                    ESP_LOGE(MAIN_TAG, "Failed to start sleep timer");
                }
            }
            uint8_t soc;
            max17048_get_soc(&soc);
            buffer_clear(&double_buffer);
            snprintf(idle_str, sizeof(idle_str), "SOC:%d%%", soc);
            draw_str(idle_str, 0, 2, &double_buffer);
            buffer_update(&double_buffer);
            break;
        }
        case STREAMING_STATE:
        {
            if (on_enter)
            {
                ESP_LOGI(MAIN_TAG, "Entering STREAMING_STATE");
                if (xTimerStop(sleep_timer, 0) != pdPASS)
                {
                    ESP_LOGE(MAIN_TAG, "Failed to stop sleep timer");
                }
            }
            break;
        }
        case SLEEP_STATE:
        {
            if (on_enter)
            {
                ESP_LOGI(MAIN_TAG, "Entering SLEEP_STATE");
                buffer_clear(&double_buffer);
                buffer_update(&double_buffer);
            }
            break;
        }
        default:
            break;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
