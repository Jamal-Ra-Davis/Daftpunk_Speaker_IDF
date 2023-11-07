#pragma once

#include "esp_flash.h"

void flash_init();
void play_sound();
esp_flash_t *get_flash_handle();