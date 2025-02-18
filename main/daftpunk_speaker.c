#include <stdio.h>
#include <stdlib.h>
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
#include "esp_sleep.h"

#include "global_defines.h"
#include "Events.h"
#include "Buttons.h"
#include "FrameBuffer.h"
#include "sr_driver.h"
#include "Display_task.h"
#include "FFT_task.h"
#include "Font.h"
#include "bt_audio.h"
#include "i2s_task.h"
#include "MAX17048.h"
#include "rgb_manager.h"
#include "flash_manager.h"
#include "audio_manager.h"
#include "tcp_shell.h"
#include "esp_sntp.h"

#include "state_manager.h"
#include "system_states.h"
#include "Stack_Info.h"
#include "Power_Manager.h"

#include "driver/adc.h"
#include "soc/adc_channel.h"
#include "PI4IOE5V6408.h"
#include "BoardRevision.h"

#define MAIN_TAG "DAFTPUNK_SPEAKER"
#define RMT_TX_CHANNEL RMT_CHANNEL_0

#define I2C_MASTER_SCL_IO I2C_SCL_PIN /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO I2C_SDA_PIN /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM 0              /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ 400000     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0   /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0   /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000

#define WAKE_GPIO GPIO_NUM_34

state_manager_t state_manager;

static esp_err_t bt_sleep(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "%s", __FUNCTION__);
    if (bt_audio_enabled())
    {
        bt_audio_deinit();
    }
    return ESP_OK;
}
static esp_err_t bt_wake(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "%s", __FUNCTION__);
    bt_i2s_task_start_up();
    return ESP_OK;
}
static esp_err_t audio_amp_sleep(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "%s", __FUNCTION__);
    amp_shutdown_assert(true);
    return ESP_OK;
}
static esp_err_t audio_amp_wake(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "%s", __FUNCTION__);
    amp_shutdown_assert(false);
    return ESP_OK;
}
static esp_err_t status_led_sleep(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "%s", __FUNCTION__);
    rgb_led_enable(false);
    return ESP_OK;
}
static esp_err_t status_led_wake(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "%s", __FUNCTION__);
    rgb_led_enable(true);
    return ESP_OK;
}
static esp_err_t display_sleep(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "%s", __FUNCTION__);
    buffer_clear(&display_buffer);
    buffer_update(&display_buffer);
    return ESP_OK;
}
static esp_err_t display_wake(void *ctx)
{
    return ESP_OK;
}
static esp_err_t power_led_sleep(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "%s", __FUNCTION__);
    power_led_enable(false);
    return ESP_OK;
}
static esp_err_t power_led_wake(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "%s", __FUNCTION__);
    power_led_enable(true);
    return ESP_OK;
}
static esp_err_t gpio_trigger_wake(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "%s", __FUNCTION__);
    setup_button_gpio_config();
    return ESP_OK;
}

// TODO: Move volume callbacks to Volume module (may want to rethink how it interacts with Bluetooth audio library)
void volume_increase_cb(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "Volume Increase Pressed:");
    int vol = (int)bt_audio_get_volume();
    vol += 7;
    if (vol > 127)
    {
        vol = 127;
    }
    bt_audio_set_volume((uint8_t)vol);
    play_audio_sfx(AUDIO_SFX_VOLUME_UP, false);
}
void volume_decrease_cb(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "Volume Decrease Pressed");
    int vol = (int)bt_audio_get_volume();
    vol -= 7;
    if (vol < 0)
    {
        vol = 0;
    }
    bt_audio_set_volume((uint8_t)vol);
    play_audio_sfx(AUDIO_SFX_VOLUME_DOWN, false);
}
static void select_action(void *ctx)
{
    static bool triple_buffering = true;
    ESP_LOGI(MAIN_TAG, "Select button pressed");

    ESP_LOGI(MAIN_TAG, "%s triple buffering", (triple_buffering) ? "Enabling" : "Disabling");
    buffer_enable_triple_buffering(&display_buffer, triple_buffering);
    triple_buffering = !triple_buffering;
}

static volatile bool pair_press = false;
static void pair_action(void *ctx)
{
    pair_press = true;
    ESP_LOGI(MAIN_TAG, "Pair Button Pressed");
}

static void charge_start_action(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "Charging started");
}
static void charge_stop_action(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "Charging stopped");
}

static void bt_connected_action(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "BT Audio Connected");
    play_audio_sfx(AUDIO_SFX_CONNECT, false);
}
static void bt_disconnected_action(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "BT Audio Disconnected");
    if (bt_audio_enabled()) {
        set_rgb_state(RGB_PAIRING);
    }
    play_audio_sfx(AUDIO_SFX_DISCONNECT, false);
}
static void bt_connecting_action(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "BT Audio connecting");
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
void wifi_connected_cb(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "WIFI Connected");
    play_audio_sfx(AUDIO_SFX_CONNECT, false);
}
void wifi_disconnected_cb(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "WIFI Disconnected");
    play_audio_sfx(AUDIO_SFX_DISCONNECT, false);
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

    // Init button manager
    if (init_buttons() < 0)
    {
        ESP_LOGE(MAIN_TAG, "Failed to init button manager");
        init_success = false;
    }

    // Setup I2C (Can probably handle in driver)
    esp_ret = i2c_master_init();
    ESP_ERROR_CHECK(esp_ret);

    // Init IO Expander and get board revision
    esp_ret = config_io_expander();
    ESP_ERROR_CHECK(esp_ret);

    board_rev_t board_revision = get_board_revision();
    ESP_LOGI(MAIN_TAG, "Board Revision = %s", get_board_revision_string(board_revision));
    
    // Assert power LED
    power_led_enable(true);

    // Init Power Manager
    esp_ret = pm_init(WAKE_GPIO, GPIO_INTR_LOW_LEVEL, gpio_trigger_wake, NULL);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(MAIN_TAG, "Failed to init power manager");
        init_success = false;
    }
    ESP_ERROR_CHECK(esp_ret);

    // Register power manager functions
    pm_api_t bt_pm = {
        .sleep = bt_sleep,
        .wake = bt_wake,
        .ctx = NULL,
    };
    pm_register_handler(&bt_pm);

    pm_api_t audio_amp_pm = {
        .sleep = audio_amp_sleep,
        .wake = audio_amp_wake,
        .ctx = NULL,
    };
    pm_register_handler(&audio_amp_pm);

    rgb_states_t rgb_state;
    pm_api_t status_led_pm = {
        .sleep = status_led_sleep,
        .wake = status_led_wake,
        .ctx = (void*)&rgb_state,
    };
    pm_register_handler(&status_led_pm);

    pm_api_t display_pm = {
        .sleep = display_sleep,
        .wake = display_wake,
        .ctx = NULL,
    };
    pm_register_handler(&display_pm);

    pm_api_t power_led_pm = {
        .sleep = power_led_sleep,
        .wake = power_led_wake,
        .ctx = NULL,
    };
    pm_register_handler(&power_led_pm);
    

    // Init Display task
    if (init_display_task() < 0)
    {
        ESP_LOGE(MAIN_TAG, "Failed to init display task");
        init_success = false;
    }

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

    ret |= register_event_callback(WIFI_CONNECTED, wifi_connected_cb, NULL);
    ret |= register_event_callback(WIFI_DISCONNECTED, wifi_disconnected_cb, NULL);

    // Init RGBW LED manager
    esp_ret = init_rgb_manager();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(MAIN_TAG, "Failed to init rgb manger");
        init_success = false;
    }

    // Display countdown timer
    for (int i = 10; i >= 0; i--)
    {
        buffer_clear(&display_buffer);
        draw_int(i, 30, 2, &display_buffer);
        buffer_update(&display_buffer);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Display boot text
    char *test_str = "DAFT PUNK";
    int test_str_len = get_str_width(test_str);
    buffer_enable_triple_buffering(&display_buffer, false);
    for (int i = FRAME_BUF_COLS; i >= -test_str_len; i--)
    {
        buffer_clear(&display_buffer);
        draw_str(test_str, i, 2, &display_buffer);
        buffer_update(&display_buffer);
        vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    vTaskDelay(200 / portTICK_PERIOD_MS);
    buffer_enable_triple_buffering(&display_buffer, true);
    for (int i = FRAME_BUF_COLS; i >= -test_str_len; i--)
    {
        buffer_clear(&display_buffer);
        draw_str(test_str, i, 2, &display_buffer);
        buffer_update(&display_buffer);
        vTaskDelay(30 / portTICK_PERIOD_MS);
    }

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

    // De-assert Audio amp shutdown signal
    gpio_config_t gp_cfg = {
        .pin_bit_mask = GPIO_SEL_4,
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_ret = gpio_config(&gp_cfg);
    ESP_ERROR_CHECK(esp_ret);

    esp_ret = gpio_set_level(GPIO_NUM_4, 0);
    ESP_ERROR_CHECK(esp_ret);

    // Enable bluetooth and audio playback
    bt_i2s_task_start_up();
#ifdef CONFIG_AUDIO_ENABLED
    // Init Bluetooth Audio and register reader callback
    bt_audio_register_data_cb(read_data_stream);
    bt_audio_init();
    set_rgb_state(RGB_PAIRING);
#endif

    // Init TCPShell
    init_tcp_shell_task();

    if (flash_init() < 0) {
        ESP_LOGW(MAIN_TAG, "Failed to setup flash manager, flash chip may not be connected");
    }
    else if (audio_manager_init() < 0) {
        ESP_LOGE(MAIN_TAG, "Failed to setup audio manager");
        init_success = false;
    }
    else {
        // Test playing sound if audio manager initialized successfully
        play_audio_sfx(AUDIO_SFX_POWERON, false);
    }

    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;

    time(&now);
    // Set timezone to Pacific Standard Time
    setenv("TZ", "PST", 1);
    tzset();

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(MAIN_TAG, "The current date/time in PST is: %s", strftime_buf);

    // Get time
    ESP_LOGI(MAIN_TAG, "%02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Set time
    timeinfo.tm_hour = 8;
    timeinfo.tm_min = 45;
    timeinfo.tm_sec = 0;
    time_t new_time = mktime(&timeinfo);
    struct timeval ts = {
        .tv_sec = new_time,
    };
    settimeofday(&ts, NULL);
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(MAIN_TAG, "%02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    gpio_config_t adc_gp_cfg = {
        .pin_bit_mask = GPIO_SEL_2,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    esp_ret = gpio_config(&adc_gp_cfg);
    adc_power_acquire();
    adc2_config_channel_atten(ADC2_GPIO2_CHANNEL, ADC_ATTEN_DB_11);

    
    for (int i=0; i<10; i++) {
        int reading; 
        adc2_get_raw(ADC2_GPIO2_CHANNEL, ADC_WIDTH_BIT_12, &reading);
        uint32_t voltage = (reading * 2450) / 4095;//esp_adc_cal_raw_to_voltage(reading, adc_chars);
        ESP_LOGI(MAIN_TAG, "V_ID voltage = %u mV, raw = %u", voltage, reading);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    print_stack_info();

    int time_str_len = get_str_width(strftime_buf);
    for (int i = FRAME_BUF_COLS; i >= -time_str_len; i--)
    {
        buffer_clear(&display_buffer);
        draw_str(strftime_buf, i, 2, &display_buffer);
        buffer_update(&display_buffer);
        vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    if (!init_success) {
        ESP_LOGE(MAIN_TAG, "Initialization Failed");
    }

    // Test state manager
    init_system_states(&state_manager);
    while (1) {
        sm_update(&state_manager);
        vTaskDelay(get_system_state_delay(&state_manager) / portTICK_PERIOD_MS);
    }
}
