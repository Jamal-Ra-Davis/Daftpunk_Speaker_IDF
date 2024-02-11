#include "Display_task.h"
#include "global_defines.h"
#include "FrameBuffer.h"
#include "sr_driver.h"
#include "driver/timer.h"

#include "freertos/semphr.h"
#include "sdkconfig.h"
#include <string.h>
#include "esp_log.h"

#define DISPLAY_TASK_STACK_SIZE 2048

#define TIMER_DIVIDER (16)                                          //  Hardware timer clock divider
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)                // convert counter value to seconds
#define TIMER_SCALE_US ((TIMER_BASE_CLK / TIMER_DIVIDER) / 1000000) // convert counter value to seconds

#define DISPLAY_REFRESH_RATE_HZ 90
#define TIMER_PERIOD_US ((1000000 / DISPLAY_REFRESH_RATE_HZ) / FRAME_BUF_ROWS)

#define DISPLAY_TASK_TAG "DISPLAY_TASK"

// File Globals
static uint8_t sr_buffer[SR_CNT] = {0x00};
static uint8_t row_idx = 0;
static TaskHandle_t xdisplay_task = NULL;
static volatile SemaphoreHandle_t xDisplayTimerSem;

// Function Prototypes
static void display_task(void *pvParameters);

static bool IRAM_ATTR timer_group_isr_callback(void *args)
{
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
    if (xSemaphoreGiveFromISR(xDisplayTimerSem, &pxHigherPriorityTaskWoken) != pdTRUE)
    {
        return false;
    }
    return pxHigherPriorityTaskWoken == pdTRUE;
}

// Public Functions
/**
 * @brief Initializes display task. Responsible for updating shift registers
 * with data read from framebuffer
 * @return 0 on success, -1 on failure
 */
int init_display_task()
{
    init_shift_registers();
    buffer_reset(&display_buffer);

    xDisplayTimerSem = xSemaphoreCreateBinary();
    if (xDisplayTimerSem == NULL)
    {
        ESP_LOGE(DISPLAY_TASK_TAG, "Failed create xDisplayTimerSem");
        return -1;
    }

    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true,
    }; // default clock source is APB

    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, TIMER_PERIOD_US * TIMER_SCALE_US);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);

    timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_group_isr_callback, NULL, 0);
    timer_start(TIMER_GROUP_0, TIMER_0);

    xTaskCreate(
        display_task,
        "Display_Task",
        DISPLAY_TASK_STACK_SIZE,
        NULL,
        DISPLAY_TASK_PRIORITY,
        &xdisplay_task);
    return 0;
}
TaskHandle_t display_task_handle()
{
    return xdisplay_task;
}

// Private Functions
static inline void update_display()
{
    // sr_buffer[0] corresponds to ROW select shift register,
    // while sr_buffer[1:N] corresponds to column data

    if (row_idx == 0)
    {
        buffer_start_of_frame(&display_buffer);
    }
    sr_buffer[0] = (1 << row_idx);
    frame_buffer_t *rbuf = buffer_get_read_buffer(&display_buffer);
    memcpy(&sr_buffer[1], rbuf->frame_buffer[row_idx], FRAME_BUF_COL_BYTES);

    sr_write(sr_buffer, SR_CNT);
    row_idx = (row_idx + 1) % FRAME_BUF_ROWS;
}
static void display_task(void *pvParameters)
{
    ESP_LOGI(DISPLAY_TASK_TAG, "Display task started");
    while (1)
    {
        if (xSemaphoreTake(xDisplayTimerSem, portMAX_DELAY) == pdFALSE)
        {
            ESP_LOGE(DISPLAY_TASK_TAG, "Failed to take semaphore");
            vTaskDelay(1);
            continue;
        }
        update_display();
    }
}