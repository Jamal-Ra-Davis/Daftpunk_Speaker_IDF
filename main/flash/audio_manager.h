#pragma once

#include "flash_manager.h"

int audio_manager_init();
int play_audio_asset(uint8_t audio_id, bool isr);

int load_audio_start(uint8_t audio_id, char *filename, uint16_t filename_len, uint32_t payload_len);
int load_nvm_start(char *filename, uint16_t path_len, uint32_t payload_len);
int load_nvm_chunk(uint8_t *payload, uint32_t chunk_len);
int load_nvm_end();
int get_audio_metadata(uint8_t *buf, uint16_t *payload_size);

//int nvm_erase_chip();
