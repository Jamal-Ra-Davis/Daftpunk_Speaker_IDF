#include "Animation.h"
#include "FrameBuffer.h"
#include <stddef.h>

int animation_sequence_init(animation_sequence_t *seq, animation_frame_t *animation_frames, uint8_t frame_count)
{
    seq->frame_count = 0;
    if (frame_count > ANIMATION_MAX_FRAMES) {
        return -1;
    }
    if (animation_frames == NULL) {
        return -1;
    }

    seq->frame_count = frame_count;
    for (int i=0; i<seq->frame_count; i++) {
        seq->animation_frames[i] = animation_frames[i];
    }
    return animation_sequence_reset(seq);
}
int animation_sequence_reset(animation_sequence_t *seq)
{
    if (seq == NULL) {
        return -1;
    }
    seq->idx = 0;
    seq->current_frame = 0;
    return 0;
}
int animation_sequence_update(animation_sequence_t *seq)
{
    if (seq == NULL) {
        return -1;
    }
    if (seq->frame_count == 0) {
        return -1;
    }

    int hold_cnt = seq->animation_frames[seq->current_frame].hold_cnt;
    if (hold_cnt < 0) {
        // Negative hold counts mean animation should be held indefinitely
        return 0; 
    }

    if (seq->idx >= hold_cnt) {
        seq->current_frame = (seq->current_frame + 1) % seq->frame_count;
        seq->idx = 1;
    }
    else {
        seq->idx++;
    }
    return 0;
}
int animation_sequence_draw(animation_sequence_t *seq, int x, int y, display_buffer_t *frame_buffer)
{
    if (seq == NULL) {
        return -1;
    }
    if (seq->frame_count == 0) {
        return -1;
    }
    return draw_bitmap(seq->animation_frames[seq->current_frame].id, x, y, frame_buffer);
}
int animation_sequence_set_frame(animation_sequence_t *seq, uint8_t frame_idx)
{
    if (seq == NULL) {
        return -1;
    }
    if (seq->frame_count == 0) {
        return -1;
    }
    if (frame_idx >= seq->frame_count) {
        return -1;
    }
    seq->current_frame = frame_idx;
    seq->idx = 1;
    return 0;
}