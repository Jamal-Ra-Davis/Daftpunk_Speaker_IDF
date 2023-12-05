#pragma once

#include "esp_flash.h"

void flash_init();
void play_sound();
int play_audio_asset(uint8_t audio_id);
esp_flash_t *get_flash_handle();

int load_audio_start(uint8_t audio_id, char *filename, uint16_t filename_len, uint32_t payload_len);
int load_nvm_start(char *filename, uint16_t path_len, uint32_t payload_len);
int load_nvm_chunk(uint8_t *payload, uint32_t chunk_len);
int load_nvm_end();
int nvm_erase_chip();
int get_audio_metadata(uint8_t *buf, uint16_t *payload_size);