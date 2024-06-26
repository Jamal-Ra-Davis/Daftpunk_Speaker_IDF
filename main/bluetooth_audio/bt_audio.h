#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len);

typedef void (*bt_audio_data_cb_t)(const uint8_t *data, uint32_t len);

#ifdef CONFIG_AUDIO_ENABLED
void bt_audio_init();
void bt_audio_deinit();
void bt_audio_register_data_cb(bt_audio_data_cb_t cb);
void bt_audio_register_raw_data_cb(bt_audio_data_cb_t cb);
uint8_t bt_audio_get_volume();
void bt_audio_set_volume(uint8_t volume);
bool bt_audio_enabled();
bool bt_audio_connected();
char *bt_audio_get_device_name();
#else
static inline void bt_audio_init() {}
static inline void bt_audio_deinit() {}
static inline void bt_audio_register_data_cb(bt_audio_data_cb_t cb) {}
static inline void bt_audio_register_raw_data_cb(bt_audio_data_cb_t cb) {}
static inline uint8_t bt_audio_get_volume() {return 0;}
static inline void bt_audio_set_volume(uint8_t volume) {}
static inline bool bt_audio_enabled() {return false;}
static inline bool bt_audio_connected() {return false;}
static inline char *bt_audio_get_device_name() {return NULL;}
#endif