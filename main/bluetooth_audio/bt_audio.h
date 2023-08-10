#pragma once
#include <stdint.h>

void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len);

typedef void (*bt_audio_data_cb_t)(const uint8_t *data, uint32_t len);

void bt_audio_init();
void bt_audio_deinit();
void bt_audio_register_data_cb(bt_audio_data_cb_t cb);
void bt_audio_register_raw_data_cb(bt_audio_data_cb_t cb);
uint8_t bt_audio_get_volume();
void bt_audio_set_volume(uint8_t volume);