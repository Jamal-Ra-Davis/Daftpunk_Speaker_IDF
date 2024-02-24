/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/xtensa_api.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bt_app_core.h"
#include "driver/i2s.h"
#include "freertos/ringbuf.h"
#include "i2s_task.h"

#define RINGBUF_HIGHEST_WATER_LEVEL (32 * 1024)
#define RINGBUF_PREFETCH_WATER_LEVEL (20 * 1024)

#define BT_I2S_TASK_TAG "BT_I2S_TASK"

enum
{
    RINGBUFFER_MODE_PROCESSING,  /* ringbuffer is buffering incoming audio data, I2S is working */
    RINGBUFFER_MODE_PREFETCHING, /* ringbuffer is buffering incoming audio data, I2S is waiting */
    RINGBUFFER_MODE_DROPPING     /* ringbuffer is not buffering (dropping) incoming audio data, I2S is working */
};

/*******************************
 * STATIC FUNCTION DECLARATIONS
 ******************************/

/* handler for I2S task */
static void bt_i2s_task_handler(void *arg);
/* message sender */
//static bool bt_app_send_msg(bt_app_msg_t *msg);
/* handle dispatched messages */
//static void bt_app_work_dispatched(bt_app_msg_t *msg);

/*******************************
 * STATIC VARIABLE DEFINITIONS
 ******************************/

static xTaskHandle s_bt_i2s_task_handle = NULL; /* handle of I2S task */
static RingbufHandle_t s_ringbuf_i2s = NULL;    /* handle of ringbuffer for I2S */
static SemaphoreHandle_t s_i2s_write_semaphore = NULL;
static uint16_t ringbuffer_mode = RINGBUFFER_MODE_PROCESSING;

/*******************************
 * STATIC FUNCTION DEFINITIONS
 ******************************/

static void bt_i2s_task_handler(void *arg)
{
    uint8_t *data = NULL;
    size_t item_size = 0;
    /**
     * The total length of DMA buffer of I2S is:
     * `dma_frame_num * dma_desc_num * i2s_channel_num * i2s_data_bit_width / 8`.
     * Transmit `dma_frame_num * dma_desc_num` bytes to DMA is trade-off.
     */
    const size_t item_size_upto = 60 * 6;
    size_t bytes_written = 0;

    for (;;)
    {
        if (pdTRUE == xSemaphoreTake(s_i2s_write_semaphore, portMAX_DELAY))
        {
            for (;;)
            {
                item_size = 0;
                /* receive data from ringbuffer and write it to I2S DMA transmit buffer */
                data = (uint8_t *)xRingbufferReceiveUpTo(s_ringbuf_i2s, &item_size, (TickType_t)pdMS_TO_TICKS(5), item_size_upto);
                if (item_size == 0)
                {
                    ESP_LOGI(BT_I2S_TASK_TAG, "ringbuffer underflowed! mode changed: RINGBUFFER_MODE_PREFETCHING");
                    ringbuffer_mode = RINGBUFFER_MODE_PREFETCHING;
                    break;
                }
                i2s_write(0, data, item_size, &bytes_written, portMAX_DELAY);
                vRingbufferReturnItem(s_ringbuf_i2s, (void *)data);
            }
        }
    }
}

static void bt_i2s_driver_install(void)
{
    /* I2S configuration parameters */
    i2s_config_t i2s_config = {
#ifdef CONFIG_EXAMPLE_A2DP_SINK_OUTPUT_INTERNAL_DAC
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
#else
        .mode = I2S_MODE_MASTER | I2S_MODE_TX, /* only TX */
#endif
        .sample_rate = 44100,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, /* 2-channels */
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .dma_buf_count = 6,
        .dma_buf_len = 60,
        .intr_alloc_flags = 0,     /* default interrupt priority */
        .tx_desc_auto_clear = true /* auto clear tx descriptor on underflow */
    };

    /* enable I2S */
    i2s_driver_install(0, &i2s_config, 0, NULL);
#ifdef CONFIG_EXAMPLE_A2DP_SINK_OUTPUT_INTERNAL_DAC
    i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
    i2s_set_pin(0, NULL);
#else
    i2s_pin_config_t pin_config = {
        .bck_io_num = CONFIG_EXAMPLE_I2S_BCK_PIN,
        .ws_io_num = CONFIG_EXAMPLE_I2S_LRCK_PIN,
        .data_out_num = CONFIG_EXAMPLE_I2S_DATA_PIN,
        .data_in_num = -1 /* not used */
    };
    i2s_set_pin(0, &pin_config);
#endif
}

static void bt_i2s_driver_uninstall(void)
{
    i2s_driver_uninstall(0);
}

/********************************
 * EXTERNAL FUNCTION DEFINITIONS
 *******************************/

void bt_i2s_task_start_up(void)
{
    if (s_bt_i2s_task_handle) {
        ESP_LOGI(BT_I2S_TASK_TAG, "I2S Task already running");
        return;
    }

    ESP_LOGI(BT_I2S_TASK_TAG, "ringbuffer data empty! mode changed: RINGBUFFER_MODE_PREFETCHING");
    ringbuffer_mode = RINGBUFFER_MODE_PREFETCHING;
    if ((s_i2s_write_semaphore = xSemaphoreCreateBinary()) == NULL)
    {
        ESP_LOGE(BT_I2S_TASK_TAG, "%s, Semaphore create failed", __func__);
        return;
    }
    if ((s_ringbuf_i2s = xRingbufferCreate(RINGBUF_HIGHEST_WATER_LEVEL, RINGBUF_TYPE_BYTEBUF)) == NULL)
    {
        ESP_LOGE(BT_I2S_TASK_TAG, "%s, ringbuffer create failed", __func__);
        return;
    }
    bt_i2s_driver_install();
    xTaskCreate(bt_i2s_task_handler, "BtI2STask", 2048, NULL, configMAX_PRIORITIES - 3, &s_bt_i2s_task_handle);
}

void bt_i2s_task_shut_down(void)
{
    if (s_bt_i2s_task_handle)
    {
        vTaskDelete(s_bt_i2s_task_handle);
        s_bt_i2s_task_handle = NULL;
        bt_i2s_driver_uninstall();
    }
    if (s_ringbuf_i2s)
    {
        vRingbufferDelete(s_ringbuf_i2s);
        s_ringbuf_i2s = NULL;
    }
    if (s_i2s_write_semaphore)
    {
        vSemaphoreDelete(s_i2s_write_semaphore);
        s_i2s_write_semaphore = NULL;
    }
}

size_t write_ringbuf(const uint8_t *data, size_t size)
{
    size_t item_size = 0;
    BaseType_t done = pdFALSE;

    if (s_ringbuf_i2s == NULL)
    {
        return 0;
    }

    if (ringbuffer_mode == RINGBUFFER_MODE_DROPPING)
    {
        ESP_LOGW(BT_I2S_TASK_TAG, "ringbuffer is full, drop this packet!");
        vRingbufferGetInfo(s_ringbuf_i2s, NULL, NULL, NULL, NULL, &item_size);
        if (item_size <= RINGBUF_PREFETCH_WATER_LEVEL)
        {
            ESP_LOGI(BT_I2S_TASK_TAG, "ringbuffer data decreased! mode changed: RINGBUFFER_MODE_PROCESSING");
            ringbuffer_mode = RINGBUFFER_MODE_PROCESSING;
        }
        return 0;
    }

    done = xRingbufferSend(s_ringbuf_i2s, (void *)data, size, (TickType_t)0);

    if (!done)
    {
        ESP_LOGW(BT_I2S_TASK_TAG, "ringbuffer overflowed, ready to decrease data! mode changed: RINGBUFFER_MODE_DROPPING");
        ringbuffer_mode = RINGBUFFER_MODE_DROPPING;
    }

    if (ringbuffer_mode == RINGBUFFER_MODE_PREFETCHING)
    {
        vRingbufferGetInfo(s_ringbuf_i2s, NULL, NULL, NULL, NULL, &item_size);
        if (item_size >= RINGBUF_PREFETCH_WATER_LEVEL)
        {
            ESP_LOGI(BT_I2S_TASK_TAG, "ringbuffer data increased! mode changed: RINGBUFFER_MODE_PROCESSING");
            ringbuffer_mode = RINGBUFFER_MODE_PROCESSING;
            if (pdFALSE == xSemaphoreGive(s_i2s_write_semaphore))
            {
                ESP_LOGE(BT_I2S_TASK_TAG, "semphore give failed");
            }
        }
    }

    return done ? size : 0;
}

void flush_ringbuf()
{
    ringbuffer_mode = RINGBUFFER_MODE_PROCESSING;
    if (pdFALSE == xSemaphoreGive(s_i2s_write_semaphore))
    {
        ESP_LOGE(BT_I2S_TASK_TAG, "semphore give failed");
    }
}