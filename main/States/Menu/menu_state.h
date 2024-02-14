#pragma once

#include "system_states_.h"

int menu_state_init(state_manager_t *state_manager);
int menu_state_on_enter(state_manager_t *state_manager);
int menu_state_on_exit(state_manager_t *state_manager);
int menu_state_update(state_manager_t *state_manager);