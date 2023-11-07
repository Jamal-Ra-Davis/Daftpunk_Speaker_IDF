#pragma once

#include "esp_flash.h"

void flash_init();
void play_sound();
esp_flash_t *get_flash_handle();

int load_nvm_start(char *filename, uint16_t path_len, uint32_t payload_len);
int load_nvm_chunk(uint8_t *payload, uint32_t chunk_len);
int load_nvm_end();