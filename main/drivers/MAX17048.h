#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef void (*max17048_alert_cb_t)(void *ctx);

typedef enum {
    MAX17048_ALERT_VOLT_HIGH,
    MAX17048_ALERT_VOLT_LOW,
    MAX17048_ALERT_SOC_LOW,
    MAX17048_ALERT_SOC_CHANGE,
    MAX17048_NUM_ALERTS,
} max17048_alert_source_t;

esp_err_t max17048_init();

esp_err_t max17048_get_voltage(float *voltage);
esp_err_t max17048_get_soc(uint8_t *soc);
esp_err_t max17048_get_version(uint16_t *version);
esp_err_t max17048_enable_sleep(bool enable);
esp_err_t max17048_get_c_rate(float *c_rate);  // Seems to be returning weird values 
esp_err_t max17048_get_id(uint8_t *id);
esp_err_t max17048_get_status(uint8_t *status);
esp_err_t max17048_soft_reset();

// ALERT settings
esp_err_t max17048_clear_alert();
esp_err_t max17048_enable_soc_change_alert(bool enable);
esp_err_t max17048_set_empty_alert_threshold(uint8_t alert_soc);
esp_err_t max17048_set_valert_max(float voltage);
esp_err_t max17048_set_valert_min(float voltage);

esp_err_t max17048_register_alert_cb(max17048_alert_source_t src, max17048_alert_cb_t cb, void *ctx);
esp_err_t max17048_unregister_alert_cb(max17048_alert_source_t src);

