#include "Power_Manager.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

#include "sdkconfig.h"
#include "esp_log.h"

#define MAX_PM_APIS 16
#define USE_DEEP_SLEEP false

// Set wake GPIO to Volume Minus button
#define WAKE_GPIO GPIO_NUM_34

static SemaphoreHandle_t pm_mutex = NULL;
static int pm_api_cnt = 0;
static pm_api_t api_list[MAX_PM_APIS];

static esp_err_t pm_perform_sleep();
static esp_err_t pm_perform_wake();

esp_err_t pm_register_handler(pm_api_t *api)
{
    if (api == NULL) {
        return ESP_FAIL;
    }
    if (pm_api_cnt >= MAX_PM_APIS) {
        ESP_LOGE(PM_TAG, "Cannot register more power manager handlers, max is %d", MAX_PM_APIS);
        return ESP_FAIL;
    }
    if (xSemaphoreTake(pm_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_FAIL;
    }

    api_list[pm_api_cnt] = *api;
    pm_api_cnt++;
    xSemaphoreGive(pm_mutex);

    ESP_LOGD(PM_TAG, "%d power management functions have been registered", pm_api_cnt);
    return ESP_OK;
}
esp_err_t pm_enter_sleep()
{
    esp_err_t ret;
    ret = pm_perform_sleep();
    if (ret != ESP_OK) {
        ESP_LOGE(PM_TAG, "Failed to perform sleep operation");
    }

    ret = pm_perform_wake();
    if (ret != ESP_OK) {
        ESP_LOGE(PM_TAG, "Failed to perform wake operation");
    }
    return ESP_OK;
}


esp_err_t pm_init()
{
    pm_mutex = xSemaphoreCreateMutex();
    if (pm_mutex == NULL)
    {
        ESP_LOGE(PM_TAG, "Failed to create power manager mutex");
        return ESP_FAIL;
    }
    return ESP_OK;
}


static esp_err_t pm_perform_sleep()
{
    esp_err_t ret;
    bool success = true;
    if (xSemaphoreTake(pm_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_FAIL;
    }
    for (int i=0; i<pm_api_cnt; i++) {
        if (api_list[i].sleep) {
            ret = api_list[i].sleep(api_list[i].ctx);
            if (ret != ESP_OK) {
                success = false;
            }
        }
    }
    xSemaphoreGive(pm_mutex);

    //TODO: Not sure if this is needed, test with and without
    vTaskDelay(200 / portTICK_PERIOD_MS); // Wait for Vsync and bluetooth stack to settle out

    // Setup GPIO wake enable signal
    gpio_wakeup_enable(WAKE_GPIO, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    // Trigger sleep
    if (USE_DEEP_SLEEP) {
        esp_deep_sleep_start();
    }
    else {
        esp_light_sleep_start();
    }

    return (success) ? ESP_OK : ESP_FAIL;
}
static esp_err_t pm_perform_wake()
{
    esp_err_t ret;
    bool success = true;

    // Revert wake GPIO settings
    gpio_wakeup_disable(WAKE_GPIO);

    if (xSemaphoreTake(pm_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_FAIL;
    }
    for (int i=0; i<pm_api_cnt; i++) {
        if (api_list[i].wake) {
            ret = api_list[i].wake(api_list[i].ctx);
            if (ret != ESP_OK) {
                success = false;
            }
        }
    }
    xSemaphoreGive(pm_mutex);
    return (success) ? ESP_OK : ESP_FAIL;
}