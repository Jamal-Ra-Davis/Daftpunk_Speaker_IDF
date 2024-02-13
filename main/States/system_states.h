#pragma once

#include "state_manager.h"
#include "pairing_state.h"

typedef enum
{
    BOOT_STATE_,
    PAIRING_STATE_,
    PAIR_SUCCESS_STATE_,
    PAIR_FAIL_STATE_,
    IDLE_STATE_,
    STREAMING_STATE_,
    MENU_STATE_,
    DISPLAY_OFF_STATE_,
    SLEEP_STATE_,
    NUM_SYSTEM_STATES_,
} system_states_t;
