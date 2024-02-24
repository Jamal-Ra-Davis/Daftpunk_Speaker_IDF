#pragma once
#include <stdint.h>

/**
 * @brief  start up the is task
 */
void bt_i2s_task_start_up(void);

/**
 * @brief  shut down the I2S task
 */
void bt_i2s_task_shut_down(void);

/**
 * @brief  write data to ringbuffer
 *
 * @param [in] data  pointer to data stream
 * @param [in] size  data length in byte
 *
 * @return size if writteen ringbuffer successfully, 0 others
 */
size_t write_ringbuf(const uint8_t *data, size_t size);

/**
 * @brief  Flush ringbuf to immediataly play buffered audio data
 */
void flush_ringbuf();