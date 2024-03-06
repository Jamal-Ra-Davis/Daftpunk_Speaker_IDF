#pragma once
#include "FrameBuffer.h"
#include <stdint.h>

struct bitmap
{
    int width;
    int height;
    uint16_t data[8];
};
typedef enum
{
    BITMAP_EYE_CLOSED,
    BITMAP_EYE_OPENING,
    BITMAP_EYE_OPEN,
    BITMAP_EYE_LEFT,
    BITMAP_EYE_RIGHT,
    BITMAP_ARROW_0,
    BITMAP_ARROW_1,
    BITMAP_ARROW_2,
    BITMAP_ARROW_3,
    NUM_BITMAPS
} bitmap_id_t;

struct bitmap *get_bitmap(bitmap_id_t id);
int draw_bitmap(bitmap_id_t id, int x, int y, display_buffer_t *frame_buffer);