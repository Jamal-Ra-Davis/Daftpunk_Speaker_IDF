#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define TCP_SHELL_BUF_SZ 2048

#ifdef CONFIG_WIFI_ENABLED
int init_tcp_shell_task();
int stop_tcp_shell_task();
TaskHandle_t tcp_handler_task_handle();
TaskHandle_t tcp_server_task_handle();
int get_tcp_ip(char *buf, size_t size);
bool wifi_connected();
#else
static inline int init_tcp_shell_task() {return 0;}
static inline int stop_tcp_shell_task() {return 0;}
static inline TaskHandle_t tcp_handler_task_handle() {return NULL;}
static inline TaskHandle_t tcp_server_task_handle() {return NULL;}
static inline int get_tcp_ip(char *buf, size_t size) {return -1;}
static inline bool wifi_connected() {return false;}
#endif