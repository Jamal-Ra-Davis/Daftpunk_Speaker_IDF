#include "Bitmap.h"
#include "FrameBuffer.h"
#include <stddef.h>

static struct bitmap bitmap_data[NUM_BITMAPS] = {
    {
        // BITMAP_EYE_CLOSED
        .width = 10,
        .height = 5,
        .data = {
            0x0000,
            0x0000,
            0xFFC0,
            0x0000,
            0x0000,
        },
    },
    {
        // BITMAP_EYE_OPENING
        .width = 10,
        .height = 5,
        .data = {
            0x0000,
            0x6D80,
            0xEDC0,
            0x7380,
            0x0000,
        },
    },
    {
        // BITMAP_EYE_OPEN
        .width = 10,
        .height = 5,
        .data = {
            0x2100,
            0x6D80,
            0xEDC0,
            0x7380,
            0x1E00,
        },
    },
    {
        // BITMAP_EYE_LEFT
        .width = 10,
        .height = 5,
        .data = {
            0x0300,
            0x5B80,
            0xDBC0,
            0x6380,
            0x1E00,
        },
    },
    {
        // BITMAP_EYE_RIGHT
        .width = 10,
        .height = 5,
        .data = {
            0x3000,
            0x7680,
            0xF6C0,
            0x7980,
            0x1E00,
        },
    },
    {
        // BITMAP_ARROW_0
        .width = 8,
        .height = 8,
        .data = {
            0x9900,
            0xCC00,
            0x6600,
            0x3300,
            0x3300,
            0x6600,
            0xCC00,
            0x9900,
        },
    },
    {
        // BITMAP_ARROW_1
        .width = 8,
        .height = 8,
        .data = {
            0xCC00,
            0x6600,
            0x3300,
            0x9900,
            0x9900,
            0x3300,
            0x6600,
            0xCC00,
        },
    },
    {
        // BITMAP_ARROW_2
        .width = 8,
        .height = 8,
        .data = {
            0x6600,
            0x3300,
            0x9900,
            0xCC00,
            0xCC00,
            0x9900,
            0x3300,
            0x6600,
        },
    },
    {
        // BITMAP_ARROW_3
        .width = 8,
        .height = 8,
        .data = {
            0x3300,
            0x9900,
            0xCC00,
            0x6600,
            0x6600,
            0xCC00,
            0x9900,
            0x3300,
        },
    },
};

struct bitmap* get_bitmap(bitmap_id_t id)
{
    if (id >= NUM_BITMAPS) {
        return NULL;
    }
    return &bitmap_data[id];
}
int draw_bitmap(bitmap_id_t id, int x, int y, display_buffer_t *frame_buffer)
{
    if (id >= NUM_BITMAPS) {
        return -1;
    }
    if (bitmap_data[id].width < 0 || bitmap_data[id].height < 0) {
        return -1;
    }
    if ((x + bitmap_data[id].width) < 0 || x >= FRAME_BUF_COLS) {
        return -1;
    }
    if ((y + bitmap_data[id].height) < 0 || y >= FRAME_BUF_ROWS) {
        return -1;
    }

    /*
    for (int i=0; i<bitmap_data[id].height; i++) {
         for (int j=0; j<bitmap_data[id].width; j++) {
             if (font_data[font_idx + i] & (1 << j)) {
                //frame_buffer->setPixel(x + j, i + y);
                buffer_set_pixel(frame_buffer, x + j, i + y);
             }
         }
    }
    */

    for (int i=0; i<bitmap_data[id].height; i++) {
        for (int j=0; j<bitmap_data[id].width; j++) {
            uint16_t data = bitmap_data[id].data[i];
            if (data & (0x8000 >> j)) {
                buffer_set_pixel(frame_buffer, x + j, i + y);
            }
        }
    }

    return 0;
}