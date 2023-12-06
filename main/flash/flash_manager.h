#pragma once

#include "esp_flash.h"

#define FLASH_BASE_PATH "/extflash"

int flash_init();
esp_flash_t *get_flash_handle();
int nvm_erase_chip();
