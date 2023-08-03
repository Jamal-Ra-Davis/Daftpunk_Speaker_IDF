#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
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
// SCL: 19, SDA: 21


/*
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "bt_app_core.h"
#include "bt_app_av.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "driver/i2s.h"
*/
#include "bt_audio.h"

#define MAIN_TAG "DAFTPUNK_SPEAKER"

system_state_t current_state = IDLE_STATE;
system_state_t prev_state = BOOT_STATE;

// TODO: Move volume callbacks to Volume module (may want to rethink how it interacts with Bluetooth audio library)
void volume_increase_cb(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "Volume Increase Pressed:");
    //volume_inc();
    int vol = (int)bt_audio_get_volume();
    vol += 7;
    if (vol > 127) {
        vol = 127;
    }
    bt_audio_set_volume((uint8_t)vol);
}
void volume_decrease_cb(void *ctx)
{
    ESP_LOGI(MAIN_TAG, "Volume Decrease Pressed");
    //volume_dec();
    int vol = (int)bt_audio_get_volume();
    vol -= 7;
    if (vol < 0) {
        vol = 0;
    }
    bt_audio_set_volume((uint8_t)vol);
}
static void select_action(void *ctx)
{
  ESP_LOGI(MAIN_TAG, "Select button pressed - Create a2dp sink");
  ESP_LOGI(MAIN_TAG, "Creating a2dp sink...");
  //a2dp_sink.start("DevBoard_v0");
  //volume_init();
  //a2dp_sink.set_stream_reader(read_data_stream);
  if (current_state == SLEEP_STATE) {
    current_state = IDLE_STATE;
  }
}

static volatile bool pair_press = false;
static void pair_action(void *ctx)
{
  pair_press = true;
  ESP_LOGI(MAIN_TAG, "Pair Button Pressed - Destroy a2dp sink");
  ESP_LOGI(MAIN_TAG, "Destroying a2dp sink...");
  //a2dp_sink.end(true);
}

static void charge_start_action(void *ctx)
{
  ESP_LOGI(MAIN_TAG, "Charging started");
  //oneshot_blink(10, 100, 0, 128, 32);
}
static void charge_stop_action(void *ctx)
{
  ESP_LOGI(MAIN_TAG, "Charging stopped");
  //oneshot_blink(10, 100, 128, 16, 16);
}
static void sleep_timer_func(TimerHandle_t xTimer)
{
    current_state = SLEEP_STATE;
}


void app_main(void)
{
    bool init_success = true;
    int ret = 0;
    // Setup I2C (Can probably handle in driver)
    /*
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    */

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
    if (init_display_task() < 0) {
        ESP_LOGE(MAIN_TAG, "Failed to init display task");
        init_success = false;
    }

    // Init Logger (Probably not needed now)

    // Init event manager
    if (init_event_manager() < 0) {
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

    // Init button manager
    if (init_buttons() < 0) {
        ESP_LOGE(MAIN_TAG, "Failed to init button manager");
        init_success = false;
    }

    // Init RGBW LED manager

    // Display countdown timer
    /*
    buffer_reset(&double_buffer);
    for (int i=0; i<8; i++) {
        buffer_set_pixel(&double_buffer, i, i);
    }
    buffer_update(&double_buffer);
    */

    for (int i=10; i >= 0; i--) {
        buffer_clear(&double_buffer);
        draw_int(i, 30, 2, &double_buffer);
        buffer_update(&double_buffer);
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    // Display boot text
    int test_str_len = get_str_width("DEVBOARD_V0");
    for (int i=FRAME_BUF_COLS; i >= -test_str_len; i--) {
        buffer_clear(&double_buffer);
        draw_str("DEVBOARD_V0", i, 2, &double_buffer);
        buffer_update(&double_buffer);
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    // Perfor I2C bus scan

    // Init FFT Task
    if (init_fft_task() < 0) {
    ESP_LOGE(MAIN_TAG, "Failed to init FFT Task");
        init_success = false;
    }

    // Init fuel gague driver

    // Init CLI task

    // Init timer thread manager and register timer threads

    // Init Bluetooth Audio and register reader callback
    bt_audio_register_data_cb(read_data_stream);
    bt_audio_init();

    TimerHandle_t sleep_timer = xTimerCreate("Sleep_Timer", MS_TO_TICKS(15000), pdFALSE, NULL, sleep_timer_func);
    

    int cnt = 0;
    char idle_str[32] = {'\0'};
    while (1) {
        //printf("Hello world %d!\n", ++cnt);


        bool on_enter = false;
        if (current_state != prev_state) {
            on_enter = true;
            prev_state = current_state;
        }

        switch (current_state) {
            case IDLE_STATE:
                if (on_enter) {
                    ESP_LOGI(MAIN_TAG, "Entering IDLE_STATE");
                    if (xTimerStart(sleep_timer, 0) != pdPASS)
                    {
                        ESP_LOGE(MAIN_TAG, "Failed to start sleep timer");
                    }
                }
                buffer_clear(&double_buffer);
                snprintf(idle_str, sizeof(idle_str), "CNT:%d", (cnt++) / 10);
                draw_str(idle_str, 0, 2, &double_buffer);
                buffer_update(&double_buffer);
                break;
            case STREAMING_STATE:
                if (on_enter) {
                    ESP_LOGI(MAIN_TAG, "Entering STREAMING_STATE");
                    if (xTimerStop(sleep_timer, 0) != pdPASS)
                    {
                        ESP_LOGE(MAIN_TAG, "Failed to stop sleep timer");
                    }
                }
                break;
            case SLEEP_STATE:
                if (on_enter) {
                    ESP_LOGI(MAIN_TAG, "Entering SLEEP_STATE");
                    buffer_clear(&double_buffer);
                    buffer_update(&double_buffer);
                }
                break;
            default:
                break;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
