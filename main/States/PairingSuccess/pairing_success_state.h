#pragma once

#include "system_states.h"

int pairing_success_state_init(state_manager_t *state_manager);
int pairing_success_state_on_enter(state_manager_t *state_manager);
int pairing_success_state_on_exit(state_manager_t *state_manager);
int pairing_success_state_update(state_manager_t *state_manager);