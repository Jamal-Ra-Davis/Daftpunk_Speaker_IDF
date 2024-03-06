#pragma once
#include "FrameBuffer.h"
#include "Bitmap.h"
#include <stdint.h>

#define ANIMATION_MAX_FRAMES 32

typedef struct {
    bitmap_id_t id;
    int hold_cnt;
} animation_frame_t;

typedef struct {
    animation_frame_t animation_frames[ANIMATION_MAX_FRAMES];
    uint8_t frame_count;
    uint8_t current_frame;
    int idx;
} animation_sequence_t;

int animation_sequence_init(animation_sequence_t *seq, animation_frame_t *animation_frames, uint8_t frame_count);
int animation_sequence_reset(animation_sequence_t *seq);
int animation_sequence_update(animation_sequence_t *seq);
int animation_sequence_draw(animation_sequence_t *seq, int x, int y, display_buffer_t *frame_buffer);
int animation_sequence_set_frame(animation_sequence_t *seq, uint8_t frame_idx);
int animation_sequence_get_frame(animation_sequence_t *seq, uint8_t *frame_idx);