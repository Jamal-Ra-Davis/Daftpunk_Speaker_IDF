#pragma once
#include <stdint.h>

void init_shift_registers();
void sr_write(uint8_t *data, int N);
void sr_write_byte(uint8_t val);