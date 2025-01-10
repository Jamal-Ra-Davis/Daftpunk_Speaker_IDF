#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "driver/gpio.h"

#define PM_TAG "POWER_MANAGER"

typedef esp_err_t (*pm_function_t)(void *ctx);

typedef struct {
    pm_function_t sleep;
    pm_function_t wake;
    void *ctx;
} pm_api_t;
typedef struct {
    pm_function_t wake;
    void *ctx;
} pm_gpio_api_t;

esp_err_t pm_register_handler(pm_api_t *api);
esp_err_t pm_enter_sleep();
esp_err_t pm_init(gpio_num_t gpio_num, gpio_int_type_t gpio_intr_type, pm_function_t wake, void *ctx);
