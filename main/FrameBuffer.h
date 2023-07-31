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
    frame_buffer_t *rbuf;
    frame_buffer_t *wbuf;
} double_buffer_t;

void buffer_reset(double_buffer_t *buffer);
void buffer_clear(double_buffer_t *buffer);
void buffer_copy(double_buffer_t *buffer);
int buffer_set_pixel(double_buffer_t *buffer, uint8_t x, uint8_t y);
int buffer_clear_pixel(double_buffer_t *buffer, uint8_t x, uint8_t y);
int buffer_set_byte(double_buffer_t *buffer, uint8_t x, uint8_t y, uint8_t b);
bool buffer_check_pixel(double_buffer_t *buffer, uint8_t x, uint8_t y);
bool buffer_compare_match(double_buffer_t *buffer);
void buffer_update(double_buffer_t *buffer);
frame_buffer_t *buffer_get_read_buffer(double_buffer_t *buffer);

extern double_buffer_t double_buffer;

/*
class DoubleBuffer {
    private:
        frame_buffer_t buf0;
        frame_buffer_t buf1;
        frame_buffer_t *rbuf;
        frame_buffer_t *wbuf;
    public:
        DoubleBuffer();
        void reset();
        void clear();
        void copy();
        int setPixel(uint8_t x, uint8_t y);
        int clearPixel(uint8_t x, uint8_t y);
        int setByte(uint8_t x, uint8_t y, uint8_t b);
        bool checkPixel(uint8_t x, uint8_t y);
        bool bufferCompareMatch();
        void update();
        frame_buffer_t *getReadBuffer() {return rbuf;}
};

extern DoubleBuffer double_buffer;
*/