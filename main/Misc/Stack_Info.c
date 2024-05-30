#include "Stack_Info.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <TCP_Shell/tcp_shell.h>
#include <Display_task.h>
#include <Events.h>
#include <rgb_manager.h>
#include <FFT/FFT_task.h>
#include <flash/audio_manager.h>      // Need to expose flash manager task
#include <bluetooth_audio/bt_audio.h> // Need to expose I2S and Bluetooth Tasks

#define STACK_INFO_TAG "Stack_Info"

int print_stack_info()
{
    char buf[256];
    int size = get_stack_info(buf, sizeof(buf));
    ESP_LOGI(STACK_INFO_TAG, "%s", buf);
    return 0;
}

int get_stack_info(char *buf, size_t buf_size)
{
    if (buf == NULL)
    {
        return -1;
    }
    int buf_idx = 0;
    int num_tasks = 0;
    TaskStatus_t xTaskDetails;

    TaskHandle_t task_list[16];
    task_list[num_tasks++] = fft_task_handle();
    task_list[num_tasks++] = display_task_handle();
    task_list[num_tasks++] = event_task_handle();
#if defined(CONFIG_WIFI_ENABLED)
    task_list[num_tasks++] = tcp_handler_task_handle();
    task_list[num_tasks++] = tcp_server_task_handle();
#endif
    task_list[num_tasks++] = rgb_manager_task_handle();

    for (int task_idx = 0; task_idx < num_tasks; task_idx++)
    {
        if (task_list[task_idx] == NULL)
        {
            continue;
        }
        UBaseType_t watermark = uxTaskGetStackHighWaterMark(task_list[task_idx]);
        vTaskGetInfo(task_list[task_idx],
                     &xTaskDetails,
                     pdTRUE,
                     eInvalid);

        const char *task_name = xTaskDetails.pcTaskName;
        int remaining_bytes = buf_size - buf_idx;
        int N = snprintf(&buf[buf_idx], remaining_bytes, "%s watermark: %d\n", task_name, (int)watermark);
        buf_idx += N;
        if (buf_idx >= buf_size)
        {
            return buf_size;
        }
    }
    volatile size_t xFreeStackSpace = xPortGetFreeHeapSize();
    int remaining_bytes = buf_size - buf_idx;
    int N = snprintf(&buf[buf_idx], remaining_bytes, "Free Heap Size = %d\n", xFreeStackSpace);
    buf_idx += N;
    return buf_idx;
}