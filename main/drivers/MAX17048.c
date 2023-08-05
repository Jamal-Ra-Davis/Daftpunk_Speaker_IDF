#include "MAX17048.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include <stdlib.h>
#include <string.h>

#define I2C_MASTER_NUM 0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define MAX17048_DEV_ADDR 0x36
#define I2C_MASTER_TIMEOUT_MS 1000
#define MAX17048_TAG "MAX17048_DRIVER"

#define ALERT_HANDLER_TASK_STACK_SIZE 2048
#define ALERT_HANDLER_TASK_PRIORITY 2

#define MAX17048_STATUS_RESET_INDICATOR BIT(0)
#define MAX17048_STATUS_VOLTAGE_HIGH BIT(1)
#define MAX17048_STATUS_VOLTAGE_LOW BIT(2)
#define MAX17048_STATUS_VOLTAGE_RESET BIT(3)
#define MAX17048_STATUS_SOC_LOW BIT(4)
#define MAX17048_STATUS_SOC_CHANGE BIT(5)
#define MAX17048_STATUS_VRESET_EN BIT(6)

typedef struct
{
    max17048_alert_cb_t cb;
    void *ctx;
    uint8_t bit_pos;
} max17048_alert_work_t;

static max17048_alert_work_t alert_callbacks[MAX17048_NUM_ALERTS];
static SemaphoreHandle_t max17048_mutex = NULL;
static SemaphoreHandle_t max17048_sem = NULL;
static TaskHandle_t xalert_task = NULL;

typedef enum
{
    MAX17048_VCELL_REG = 0x02,
    MAX17048_SOC_REG = 0x04,
    MAX17048_MODE_REG = 0x06,
    MAX17048_VERSION_REG = 0x08,
    MAX17048_HIBRT_REG = 0x0A,
    MAX17048_CONFIG_REG = 0x0C,
    MAX17048_VALRT_REG = 0x14,
    MAX17048_CRATE_REG = 0x16,
    MAX17048_VRESET_ID_REG = 0x18,
    MAX17048_STATUS_REG = 0x1A,
    MAX17048_TABLE_START_REG = 0x40,
    MAX17048_CMD_REG = 0xFE,
} MAX17048_register_t;

static void alert_handler_task(void *pvParameters);

esp_err_t max17048_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, MAX17048_DEV_ADDR, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
}

esp_err_t max17048_register_read_word(uint8_t reg_addr, uint16_t *data)
{
    uint8_t buf[2];
    esp_err_t ret = max17048_register_read(reg_addr, buf, sizeof(buf));
    if (ret != ESP_OK) {
        return ret;
    }
    *data = (uint16_t)((buf[0] << 8) | buf[1]);
    return ret;
}

esp_err_t max17048_register_write_word(uint8_t reg_addr, uint16_t data)
{
    int ret;
    uint8_t write_buf[3] = {reg_addr, (uint8_t)(data >> 8), (uint8_t)(0x00FF & data)};

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, MAX17048_DEV_ADDR, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

    return ret;
}

esp_err_t max17048_register_write(uint8_t reg_addr, uint8_t *data, size_t len)
{
    int ret;
    uint8_t write_buf[33];

    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (len > sizeof(write_buf) - 1)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    write_buf[0] = reg_addr;
    memcpy(&write_buf[1], data, len);

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, MAX17048_DEV_ADDR, write_buf, len + 1, I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

    return ret;
}
esp_err_t max17048_register_update(uint8_t reg_addr, uint16_t data, uint16_t mask)
{
    esp_err_t ret;
    uint16_t read_val;
    uint16_t val;
    ret = max17048_register_read_word(reg_addr, &read_val);
    if (ret != ESP_OK)
    {
        return ret;
    }

    val = (data & mask) | (read_val & ~mask);
    ret = max17048_register_write_word(reg_addr, val);
    return ret;
}

static uint16_t endian_swap_16bit(uint16_t data)
{
    uint8_t *raw = (uint8_t *)&data;
    uint16_t ret = (raw[0] << 8) | raw[1];
    return ret;
}

static void IRAM_ATTR alert_gpio_isr_handler(void *arg)
{
    if (max17048_sem == NULL) {
        return;
    }
    BaseType_t pxHigherPriorityTaskWoken;
    xSemaphoreGiveFromISR(max17048_sem, &pxHigherPriorityTaskWoken);
}

static void alert_handler_task(void *pvParameters)
{
    ESP_LOGI(MAX17048_TAG, "Starting Alert handler task...");
    uint8_t status = 0;
    esp_err_t ret;
    while (1)
    {
        if (xSemaphoreTake(max17048_sem, portMAX_DELAY) == pdFALSE)
        {
            ESP_LOGE(MAX17048_TAG, "Failed to take semaphore");
            continue;
        }
        
        ESP_LOGI(MAX17048_TAG, "Alert detected");
        if (max17048_get_status(&status) != ESP_OK) 
        {
            ESP_LOGE(MAX17048_TAG, "Failed to read status register");
            continue;
        }
        ret = max17048_clear_alert();

        if (xSemaphoreTake(max17048_mutex, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }
        for (int i=0; i<MAX17048_NUM_ALERTS; i++) {
            if ((alert_callbacks[i].cb != NULL) && (status & alert_callbacks[i].bit_pos)) {
                alert_callbacks[i].cb(alert_callbacks[i].ctx);
            }   
        }
        xSemaphoreGive(max17048_mutex);

        if (ret != ESP_OK) {    
            ESP_LOGE(MAX17048_TAG, "Failed to clear alert bit");
        }
    }
}

esp_err_t max17048_init()
{
    //static const uint8_t SLEEP_EN_BIT = BIT(5);
    esp_err_t ret;

    for (int i = 0; i < MAX17048_NUM_ALERTS; i++)
    {
        alert_callbacks[i].cb = NULL;
        alert_callbacks[i].ctx = NULL;
    }
    alert_callbacks[MAX17048_ALERT_VOLT_HIGH].bit_pos = MAX17048_STATUS_VOLTAGE_HIGH;
    alert_callbacks[MAX17048_ALERT_VOLT_LOW].bit_pos = MAX17048_STATUS_VOLTAGE_LOW;
    alert_callbacks[MAX17048_ALERT_SOC_LOW].bit_pos = MAX17048_STATUS_SOC_LOW;
    alert_callbacks[MAX17048_ALERT_SOC_CHANGE].bit_pos = MAX17048_STATUS_SOC_CHANGE;

    max17048_mutex = xSemaphoreCreateMutex();
    if (max17048_mutex == NULL)
    {
        ESP_LOGE(MAX17048_TAG, "Failed to init mutex");
        return ESP_FAIL;
    }

    max17048_sem = xSemaphoreCreateBinary();
    if (max17048_sem == NULL)
    {
        ESP_LOGE(MAX17048_TAG, "Failed to init semaphore");
        return ESP_FAIL;
    }

    /*
    ret = max17048_register_write_byte((uint8_t)MAX17048_MODE_REG, SLEEP_EN_BIT);
    if (ret != ESP_OK)
    {
        return ret;
    }
    */

    ret = max17048_clear_alert();
    if (ret != ESP_OK)
    {
        return ret;
    }

    gpio_config_t gp_cfg = {
        .pin_bit_mask = (uint64_t)((uint64_t)1 << CONFIG_MAX17048_ALERT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ret = gpio_config(&gp_cfg);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = gpio_isr_handler_add(CONFIG_MAX17048_ALERT_GPIO, alert_gpio_isr_handler, NULL);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = max17048_enable_soc_change_alert(true);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = max17048_set_empty_alert_threshold(CONFIG_MAX17048_EMPTY_ALERT_THESH);
    if (ret != ESP_OK)
    {
        return ret;
    }

    xTaskCreate(
        alert_handler_task,
        "MAX17048_Alert_Handler_Task",
        ALERT_HANDLER_TASK_STACK_SIZE,
        NULL,
        ALERT_HANDLER_TASK_PRIORITY,
        &xalert_task);
    return ESP_OK;
}
esp_err_t max17048_get_version(uint16_t *version)
{
    esp_err_t ret = ESP_OK;
    uint16_t version_raw;
    ret = max17048_register_read((uint8_t)MAX17048_VERSION_REG, (uint8_t *)&version_raw, sizeof(uint16_t));
    if (ret != ESP_OK)
    {
        return ret;
    }
    *version = endian_swap_16bit(version_raw);
    return ret;
}
esp_err_t max17048_get_voltage(float *voltage)
{
    static const float VOLTAGE_SCALE = 0.000078125;
    esp_err_t ret = ESP_OK;
    uint16_t voltage_raw;
    ret = max17048_register_read((uint8_t)MAX17048_VCELL_REG, (uint8_t *)&voltage_raw, sizeof(uint16_t));
    if (ret != ESP_OK)
    {
        return ret;
    }
    *voltage = (float)(VOLTAGE_SCALE * endian_swap_16bit(voltage_raw));
    return ret;
}
esp_err_t max17048_get_soc(uint8_t *soc)
{
    return max17048_register_read((uint8_t)MAX17048_SOC_REG, soc, sizeof(uint8_t));
}

esp_err_t max17048_enable_sleep(bool enable)
{
    static const uint16_t SLEEP_BIT = BIT(7);
    uint16_t val = (enable) ? SLEEP_BIT : 0;
    return max17048_register_update((uint8_t)MAX17048_CONFIG_REG, val, SLEEP_BIT);
}
esp_err_t max17048_enable_soc_change_alert(bool enable)
{
    static const uint16_t SOC_CHANGE_BIT = BIT(6);
    uint16_t val = (enable) ? SOC_CHANGE_BIT : 0;
    return max17048_register_update((uint8_t)MAX17048_CONFIG_REG, val, SOC_CHANGE_BIT);
}
esp_err_t max17048_clear_alert()
{   
    static const uint16_t ALERT_BIT = BIT(5);
    return max17048_register_update((uint8_t)MAX17048_CONFIG_REG, 0, ALERT_BIT);
}
esp_err_t max17048_set_empty_alert_threshold(uint8_t alert_soc)
{
    if (alert_soc < 1 || alert_soc > 32)
    {
        return ESP_ERR_INVALID_ARG;
    }
    static const uint16_t ALERT_THRESH_MASK = 0x001F;
    uint16_t val = 32 - alert_soc;
    return max17048_register_update((uint8_t)MAX17048_CONFIG_REG, val, ALERT_THRESH_MASK);
}

esp_err_t max17048_set_valert_max(float voltage)
{
    if (voltage > 5.1 || voltage < 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    static const uint16_t VALERT_MAX_MASK = 0x00FF;
    uint8_t val_ = (uint8_t)(voltage / 0.02);
    uint16_t val = (uint16_t)val_;
    return max17048_register_update((uint8_t)MAX17048_VALRT_REG, val, VALERT_MAX_MASK);
}
esp_err_t max17048_set_valert_min(float voltage)
{
    if (voltage > 5.1 || voltage < 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    static const uint16_t VALERT_MIN_MASK = 0xFF00;
    uint8_t val_ = (uint8_t)(voltage / 0.02);
    uint16_t val = (uint16_t)(val_ << 8);
    return max17048_register_update((uint8_t)MAX17048_VALRT_REG, val, VALERT_MIN_MASK);
}

esp_err_t max17048_get_c_rate(float *c_rate)
{
    static const float C_RATE_SCALE = 0.208;
    esp_err_t ret = ESP_OK;
    uint16_t c_rate_raw;
    ret = max17048_register_read_word((uint8_t)MAX17048_CRATE_REG, &c_rate_raw);
    if (ret != ESP_OK)
    {
        return ret;
    }

    *c_rate = (float)(C_RATE_SCALE * (int16_t)endian_swap_16bit(c_rate_raw));
    return ret;
}

esp_err_t max17048_get_id(uint8_t *id)
{
    uint16_t val;
    esp_err_t ret = max17048_register_read_word((uint8_t)MAX17048_VRESET_ID_REG, &val);
    if (ret != ESP_OK)
    {
        return ret;
    }
    *id = (uint8_t)(val & 0x00FF);
    return ret;
}

esp_err_t max17048_get_status(uint8_t *status)
{
    uint16_t val;
    esp_err_t ret = max17048_register_read_word((uint8_t)MAX17048_STATUS_REG, &val);
    if (ret != ESP_OK)
    {
        return ret;
    }
    *status = (uint8_t)((val >> 8) & 0x00FF);
    return ret;
}

esp_err_t max17048_soft_reset()
{
    static const uint16_t SOFT_RESET_CMD = 0x5400;
    return max17048_register_write_word((uint8_t)MAX17048_CMD_REG, SOFT_RESET_CMD);
}

esp_err_t max17048_register_alert_cb(max17048_alert_source_t src, max17048_alert_cb_t cb, void *ctx)
{
    if (max17048_mutex == NULL)
    {
        ESP_LOGE(MAX17048_TAG, "Mutex not initialized");
        return ESP_FAIL;
    }
    if (xSemaphoreTake(max17048_mutex, (TickType_t)10) != pdTRUE)
    {
        ESP_LOGE(MAX17048_TAG, "Failed to lock mutex");
        return ESP_FAIL;
    }
    if (alert_callbacks[src].cb != NULL) {
        ESP_LOGE(MAX17048_TAG, "Callback function already registered");
        return ESP_FAIL;
    }

    alert_callbacks[src].cb = cb;
    alert_callbacks[src].ctx = ctx;
    xSemaphoreGive(max17048_mutex);
    return ESP_OK;
}
esp_err_t max17048_unregister_alert_cb(max17048_alert_source_t src)
{
    if (max17048_mutex == NULL)
    {
        ESP_LOGE(MAX17048_TAG, "Mutex not initialized");
        return ESP_FAIL;
    }
    if (xSemaphoreTake(max17048_mutex, (TickType_t)10) != pdTRUE)
    {
        ESP_LOGE(MAX17048_TAG, "Failed to lock mutex");
        return ESP_FAIL;
    }

    alert_callbacks[src].cb = NULL;
    alert_callbacks[src].ctx = NULL;
    xSemaphoreGive(max17048_mutex);
    return ESP_OK;
}