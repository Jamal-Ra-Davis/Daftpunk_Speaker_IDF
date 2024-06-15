#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef enum {FFT_LINEAR, FFT_LOG, FFT_LOG_MIRROR, NUM_FFT_DISPLAYS} fft_display_type_t;

int init_fft_task();
void read_data_stream(const uint8_t *data, uint32_t length);
TaskHandle_t fft_task_handle();

fft_display_type_t get_fft_display_type();
void set_fft_display_type(fft_display_type_t fft);
const char *get_fft_display_type_name(fft_display_type_t fft);
uint32_t get_fft_log_min();
void set_fft_log_min(uint32_t log_min);
