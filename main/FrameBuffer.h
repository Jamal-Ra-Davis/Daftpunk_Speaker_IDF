#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FRAME_BUF_ROWS      8
#define FRAME_BUF_COL_BYTES 5
#define FRAME_BUF_COLS (FRAME_BUF_COL_BYTES*8)

typedef struct {
    uint8_t frame_buffer[FRAME_BUF_ROWS][FRAME_BUF_COL_BYTES];    
} frame_buffer_t;


typedef struct {
    frame_buffer_t buf0;
    frame_buffer_t buf1;
    frame_buffer_t buf2;
    frame_buffer_t *rbuf;
    frame_buffer_t *wbuf;
    frame_buffer_t *ibuf;
    bool update;
    bool triple_buffering;
} display_buffer_t;

void buffer_reset(display_buffer_t *buffer);
void buffer_clear(display_buffer_t *buffer);
void buffer_copy(display_buffer_t *buffer);
int buffer_set_pixel(display_buffer_t *buffer, uint8_t x, uint8_t y);
int buffer_clear_pixel(display_buffer_t *buffer, uint8_t x, uint8_t y);
int buffer_set_byte(display_buffer_t *buffer, uint8_t x, uint8_t y, uint8_t b);
bool buffer_check_pixel(display_buffer_t *buffer, uint8_t x, uint8_t y);
bool buffer_compare_match(display_buffer_t *buffer);
void buffer_update(display_buffer_t *buffer);
void buffer_start_of_frame(display_buffer_t *buffer);
void buffer_enable_triple_buffering(display_buffer_t *buffer, bool triple_buffering);
frame_buffer_t *buffer_get_read_buffer(display_buffer_t *buffer);

extern display_buffer_t display_buffer;