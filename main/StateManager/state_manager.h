#pragma once

#include <stdint.h>

#define MAX_STATE_CNT 16

typedef struct state_element_t state_element_t;
typedef struct state_manager_t state_manager_t;
typedef int (*state_handler_func_t)(state_manager_t *state_manager);

typedef struct state_element_t
{
    state_handler_func_t init;
    state_handler_func_t on_enter;
    state_handler_func_t on_exit;
    state_handler_func_t update;
} state_element_t;

typedef struct state_manager_t
{
    uint8_t current_state;
    uint8_t state_cnt;
    state_element_t states[MAX_STATE_CNT];
    void *ctx;
} state_manager_t;

int sm_setup_state_manager(state_manager_t *state_manager, uint8_t state_cnt);
int sm_init(state_manager_t *state_manager, uint8_t current_state, void *ctx);
int sm_register_state(state_manager_t *state_manager, uint8_t state_idx, state_element_t state_element);
int sm_change_state(state_manager_t *state_manager, uint8_t next_state);
int sm_update(state_manager_t *state_manager);