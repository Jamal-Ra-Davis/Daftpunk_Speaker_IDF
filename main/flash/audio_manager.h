#pragma once

#include "flash_manager.h"

typedef enum {
    AUDIO_SFX_POWERON,
    AUDIO_SFX_SLEEP, 
    AUDIO_SFX_WAKE, 
    AUDIO_SFX_CONNECT,
    AUDIO_SFX_DISCONNECT, 
    AUDIO_SFX_VOLUME_UP, 
    AUDIO_SFX_VOLUME_DOWN,
    NUM_AUDIO_SFX,
} audio_sfx_t;

int audio_manager_init();
int play_audio_asset(uint8_t audio_id, bool isr);
int play_audio_sfx(audio_sfx_t sfx, bool isr);
int play_audio_asset_blocking(uint8_t audio_id, TickType_t xTicksToWait);
int play_audio_sfx_blocking(audio_sfx_t sfx, TickType_t xTicksToWait);
int map_audio_sfx(audio_sfx_t sfx, uint8_t audio_id);

int load_audio_start(uint8_t audio_id, char *filename, uint16_t filename_len, uint32_t payload_len);
int load_nvm_start(char *filename, uint16_t path_len, uint32_t payload_len);
int load_nvm_chunk(uint8_t *payload, uint32_t chunk_len);
int load_nvm_end();
int get_audio_metadata(uint8_t *buf, uint16_t *payload_size);
