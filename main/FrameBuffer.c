#include "FrameBuffer.h"
#include <stdbool.h>
#include <string.h>

#define BITS_PER_BYTE 8

double_buffer_t double_buffer;

void buffer_reset(double_buffer_t *buffer)
{
    buffer->update = false;
    buffer->triple_buffering = true;
    buffer->rbuf = &buffer->buf0;
    buffer->wbuf = &buffer->buf1;
    buffer->ibuf = &buffer->buf2;
    for (int i = 0; i < FRAME_BUF_ROWS; i++)
    {
        for (int j = 0; j < FRAME_BUF_COL_BYTES; j++)
        {
            buffer->buf0.frame_buffer[i][j] = 0xFF;
            buffer->buf1.frame_buffer[i][j] = 0xFF;
            buffer->buf2.frame_buffer[i][j] = 0xFF;
        }
    }
}
void buffer_clear(double_buffer_t *buffer)
{
    for (int i = 0; i < FRAME_BUF_ROWS; i++)
    {
        for (int j = 0; j < FRAME_BUF_COL_BYTES; j++)
        {
            buffer->wbuf->frame_buffer[i][j] = 0xFF;
        }
    }
}
void buffer_copy(double_buffer_t *buffer)
{
    memcpy(buffer->wbuf, buffer->rbuf, sizeof(frame_buffer_t));
}
int buffer_set_pixel(double_buffer_t *buffer, uint8_t x, uint8_t y)
{
    if (x >= FRAME_BUF_COL_BYTES * BITS_PER_BYTE)
    {
        return -1;
    }
    if (y >= FRAME_BUF_ROWS)
    {
        return -1;
    }

    int idx = 4 - x / 8;
    int bit_idx = x % 8;
    buffer->wbuf->frame_buffer[y][idx] &= ~(1 << bit_idx);
    return 0;
}
int buffer_clear_pixel(double_buffer_t *buffer, uint8_t x, uint8_t y)
{
    if (x >= FRAME_BUF_COL_BYTES * BITS_PER_BYTE)
    {
        return -1;
    }
    if (y >= FRAME_BUF_ROWS)
    {
        return -1;
    }

    int idx = 4 - x / 8;
    int bit_idx = x % 8;
    buffer->wbuf->frame_buffer[y][idx] |= (1 << bit_idx);
    return 0;
}
int buffer_set_byte(double_buffer_t *buffer, uint8_t x, uint8_t y, uint8_t b)
{
    if (x >= FRAME_BUF_COL_BYTES * BITS_PER_BYTE)
    {
        return -1;
    }
    if (y >= FRAME_BUF_ROWS)
    {
        return -1;
    }

    int idx = 4 - x / 8;
    buffer->wbuf->frame_buffer[y][idx] = b;
    return 0;
}
bool buffer_check_pixel(double_buffer_t *buffer, uint8_t x, uint8_t y)
{
    if (x >= FRAME_BUF_COL_BYTES * BITS_PER_BYTE)
    {
        return false;
    }
    if (y >= FRAME_BUF_ROWS)
    {
        return false;
    }

    int idx = 4 - x / 8;
    int bit_idx = x % 8;
    return !(buffer->rbuf->frame_buffer[y][idx] & (1 << bit_idx));
}
bool buffer_compare_match(double_buffer_t *buffer)
{
    for (int i = 0; i < FRAME_BUF_ROWS; i++)
    {
        for (int j = 0; j < FRAME_BUF_COL_BYTES; j++)
        {
            if (buffer->wbuf->frame_buffer[i][j] != buffer->rbuf->frame_buffer[i][j])
            {
                return false;
            }
        }
    }
    return true;
}
void buffer_update(double_buffer_t *buffer)
{
    if (!buffer->triple_buffering) 
    {
        frame_buffer_t *temp = buffer->rbuf;
        buffer->rbuf = buffer->wbuf;
        buffer->wbuf = temp;    
    }
    else {
        frame_buffer_t *temp = buffer->ibuf;
        buffer->ibuf = buffer->wbuf;
        buffer->wbuf = temp;
        buffer->update = true;
    }
}
inline void buffer_start_of_frame(double_buffer_t *buffer)
{
    if (buffer->triple_buffering && buffer->update)
    {
        buffer->update = false;
        frame_buffer_t *temp = buffer->ibuf;
        buffer->ibuf = buffer->rbuf;
        buffer->rbuf = temp;
    }
}
void buffer_enable_triple_buffering(double_buffer_t *buffer, bool triple_buffering)
{
    buffer->triple_buffering = triple_buffering;
}
frame_buffer_t *buffer_get_read_buffer(double_buffer_t *buffer)
{
    return buffer->rbuf;
}