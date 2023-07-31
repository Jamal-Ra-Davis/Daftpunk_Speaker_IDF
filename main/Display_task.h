#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

int init_display_task();
TaskHandle_t display_task_handle();