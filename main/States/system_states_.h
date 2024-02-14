#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "state_manager.h"
#include "Pairing/pairing_state.h"
#include "PairingSuccess/pairing_success_state.h"
#include "PairingFail/pairing_fail_state.h"
#include "Idle/idle_state.h"
#include "Menu/menu_state.h"
#include "DisplayOff/display_off_state.h"

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

typedef struct {
    QueueHandle_t event_queue;
} state_manager_context_t;

system_states_t get_system_state(state_manager_t *state_manager);