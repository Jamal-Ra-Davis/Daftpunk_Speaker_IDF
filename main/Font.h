#pragma once
#include "FrameBuffer.h"

int draw_char(char c, int x, int y, double_buffer_t *frame_buffer);
void draw_str(const char *s, int x, int y, double_buffer_t *frame_buffer);
void draw_int(int val, int x, int y, double_buffer_t *frame_buffer);
int get_font_width(char c);
int get_str_width(const char *str);