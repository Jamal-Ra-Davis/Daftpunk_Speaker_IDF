#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef enum
{
    RGB_MANUAL,
    RGB_1HZ_CYCLE,
    RGB_LOW_BATTERY,
    RGB_PAIRING,
    BLINK_N,
    NUM_RGB_STATES
} rgb_states_t;

int init_rgb_manager();
int oneshot_blink(int cnt, int period, uint8_t r, uint8_t g, uint8_t b);
int set_rgb_state(rgb_states_t state);
rgb_states_t get_rgb_state();
void set_rgb_led(uint8_t r, uint8_t g, uint8_t b);

TaskHandle_t rgb_manager_task_handle();
