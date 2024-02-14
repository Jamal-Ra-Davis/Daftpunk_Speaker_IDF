#include "Events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "sdkconfig.h"
#include "esp_log.h"

#include "global_defines.h"

#define MAX_EVENTS 10
#define MAX_EXTERNAL_EVENT_QUEUES 8
#define EVENT_MANAGER_TASK_STACK_SIZE 2304
#define MUTEX_DELAY 100

struct event_callback_data
{
    event_callback_t cb;
    void *ctx;
};

// File Globals
static struct event_callback_data event_callbacks[NUM_EVENTS];
static QueueHandle_t event_queue = NULL;
static QueueHandle_t external_event_queues[MAX_EXTERNAL_EVENT_QUEUES];
static SemaphoreHandle_t event_mutex = NULL;
static TaskHandle_t xevent_task = NULL;
static int external_event_queue_cnt = 0;

// Function Prototypes
static void event_manager_task(void *pvParameters);

// Public Functions
int init_event_manager()
{
    for (int i = 0; i < NUM_EVENTS; i++)
    {
        event_callbacks[i].cb = NULL;
        event_callbacks[i].ctx = NULL;
    }

    event_queue = xQueueCreate(MAX_EVENTS, sizeof(system_event_t));
    if (event_queue == NULL)
    {
        ESP_LOGI(EVENTS_TAG, "Failed to create event queue");
        return -1;
    }
    for (int i=0; i<MAX_EXTERNAL_EVENT_QUEUES; i++) {
        external_event_queues[i] = NULL;
    }

    event_mutex = xSemaphoreCreateMutex();
    if (event_mutex == NULL)
    {
        ESP_LOGI(EVENTS_TAG, "Failed to create event mutex");
        return -1;
    }

    xTaskCreate(
        event_manager_task,
        "Event_Manager_Task",
        EVENT_MANAGER_TASK_STACK_SIZE,
        NULL,
        EVENT_MANAGER_TASK_PRIORITY,
        &xevent_task);
    return 0;
}
int register_event_callback(system_event_t event, event_callback_t cb, void *ctx)
{
    if (event >= NUM_EVENTS)
    {
        return -1;
    }
    if (event_mutex == NULL)
    {
        return -1;
    }
    if (xSemaphoreTake(event_mutex, MS_TO_TICKS(MUTEX_DELAY)) != pdTRUE)
    {
        return -1;
    }

    event_callbacks[event].cb = cb;
    event_callbacks[event].ctx = ctx;
    xSemaphoreGive(event_mutex);
    return 0;
}
int unregister_event_callback(system_event_t event, event_callback_t cb)
{
    if (event >= NUM_EVENTS)
    {
        return -1;
    }
    if (event_mutex == NULL)
    {
        return -1;
    }
    if (xSemaphoreTake(event_mutex, MS_TO_TICKS(MUTEX_DELAY)) != pdTRUE)
    {
        return -1;
    }

    event_callbacks[event].cb = NULL;
    event_callbacks[event].ctx = NULL;
    xSemaphoreGive(event_mutex);
    return 0;
}
bool event_callback_registered(system_event_t event)
{
    if (event >= NUM_EVENTS)
    {
        return false;
    }
    if (event_mutex == NULL)
    {
        return false;
    }
    if (xSemaphoreTake(event_mutex, MS_TO_TICKS(MUTEX_DELAY)) != pdTRUE)
    {
        return false;
    }
    bool result = (event_callbacks[event].cb != NULL);
    xSemaphoreGive(event_mutex);
    return result;
}
int push_event(system_event_t event, bool isr)
{
    if (event_queue == NULL)
    {
        return -1;
    }
    BaseType_t ret;
    if (isr)
    {
        ret = xQueueSendFromISR(event_queue, (void *)&event, NULL);
        for (int i=0; i<external_event_queue_cnt; i++) {
            xQueueSendFromISR(external_event_queues[i], (void *)&event, NULL);
        }
    }
    else
    {
        ret = xQueueSend(event_queue, (void *)&event, (TickType_t)0);
        for (int i=0; i<external_event_queue_cnt; i++) {
            xQueueSend(external_event_queues[i], (void *)&event, (TickType_t)0);
        }
    }

    return (ret == pdTRUE) ? 0 : -1;
}
QueueHandle_t get_event_queue_handle()
{
    if (external_event_queue_cnt >= MAX_EXTERNAL_EVENT_QUEUES) {
        ESP_LOGI(EVENTS_TAG, "Maximum number of external event queues created");
        return NULL;
    }
    // Create event queue
    QueueHandle_t queue = xQueueCreate(MAX_EVENTS, sizeof(system_event_t));
    if (queue == NULL) {
        return NULL;
    }

    if (xSemaphoreTake(event_mutex, MS_TO_TICKS(MUTEX_DELAY)) != pdTRUE)
    {
        vQueueDelete(queue);
        return NULL;
    }

    external_event_queues[external_event_queue_cnt] = queue;
    external_event_queue_cnt++;
    xSemaphoreGive(event_mutex);
    return queue;
}
TaskHandle_t event_task_handle()
{
    return xevent_task;
}

// Private Functions
static void event_manager_task(void *pvParameters)
{
    if (event_queue == NULL)
    {
        ESP_LOGI(EVENTS_TAG, "Could not get handle to system event queue");
        vTaskDelete(NULL);
    }
    if (event_mutex == NULL)
    {
        ESP_LOGI(EVENTS_TAG, "Could not get handle to system event mutex");
        vTaskDelete(NULL);
    }

    system_event_t event;
    while (1)
    {
        xQueueReceive(event_queue, &event, portMAX_DELAY);
        if (event >= NUM_EVENTS)
        {
            ESP_LOGI(EVENTS_TAG, "Invalid event");
            continue;
        }

        if (xSemaphoreTake(event_mutex, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }
        if (event_callbacks[event].cb != NULL)
        {
            event_callbacks[event].cb(event_callbacks[event].ctx);
        }
        xSemaphoreGive(event_mutex);
    }
}
