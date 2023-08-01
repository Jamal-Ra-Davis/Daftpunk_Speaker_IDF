#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "Events.h"
#include "Buttons.h"
#include "FrameBuffer.h"
#include "sr_driver.h"
#include "Display_task.h"
#include "FFT_task.h"
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
    buffer_reset(&double_buffer);
    for (int i=0; i<8; i++) {
        buffer_set_pixel(&double_buffer, i, i);
    }
    buffer_update(&double_buffer);

    // Display boot text

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

    int cnt = 0;
    while (1) {
        printf("Hello world %d!\n", ++cnt);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
