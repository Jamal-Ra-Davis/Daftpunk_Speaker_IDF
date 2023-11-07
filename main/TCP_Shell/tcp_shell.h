#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#ifdef CONFIG_WIFI_ENABLED
int init_tcp_shell_task();
int stop_tcp_shell_task();
TaskHandle_t tcp_shell_task_handle();
#else
static inline int init_tcp_shell_task() {return 0;}
static inline int stop_tcp_shell_task() {return 0;}
static inline TaskHandle_t tcp_shell_task_handle() {return NULL;}
#endif