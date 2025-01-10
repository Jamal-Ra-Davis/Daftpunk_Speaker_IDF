#pragma once
#include <esp_err.h>
#include <stdbool.h>

typedef enum {
    BOARD_REV_V2 = 0,
    BOARD_REV_V3,
    BOARD_REV_V0 = 8, // NFF Board/V1
    BOARD_REV_INVALID,
} board_rev_t;

#define BOARD_REV_V1 BOARD_REV_V0

board_rev_t get_board_revision();
const char *get_board_revision_string(board_rev_t board_rev);
esp_err_t config_io_expander();

// TODO: Move these function to separate module
bool vbus_is_present();
esp_err_t power_led_enable(bool enable);
esp_err_t amp_shutdown_assert(bool shutdown);
esp_err_t rgb_led_enable(bool enable);