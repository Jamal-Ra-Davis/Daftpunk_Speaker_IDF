#pragma once

#include "system_states_.h"

int sleep_state_init(state_manager_t *state_manager);
int sleep_state_on_enter(state_manager_t *state_manager);
int sleep_state_on_exit(state_manager_t *state_manager);
int sleep_state_update(state_manager_t *state_manager);