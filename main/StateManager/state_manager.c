 #include <stdlib.h>
 #include "state_manager.h"


int sm_setup_state_manager(state_manager_t *state_manager, uint8_t state_cnt)
{
    if (!state_manager) {
        return -1;
    }
    state_manager->state_cnt = state_cnt;
    state_manager->ctx = NULL;
    return 0;
}
int sm_init(state_manager_t *state_manager, uint8_t current_state, void *ctx)
{
    if (!state_manager) {
        return -1;
    }
    state_manager->ctx = ctx;
    for (int i=0; i<state_manager->state_cnt; i++) {
        if (state_manager->states[i].init) {
            state_manager->states[i].init(state_manager);
        }
    }
    state_manager->current_state = current_state;
    if (state_manager->states[state_manager->current_state].on_enter) {
        state_manager->states[state_manager->current_state].on_enter(state_manager);
    }
    return 0;
}
int sm_register_state(state_manager_t *state_manager, uint8_t state_idx, state_element_t state_element)
{
    if (!state_manager) {
        return -1;
    }
    if (state_idx >= state_manager->state_cnt) {
        return -1;
    }
    state_manager->states[state_idx] = state_element;
    return 0;
}
int sm_change_state(state_manager_t *state_manager, uint8_t next_state)
{
    if (!state_manager) {
        return -1;
    }
    if (next_state >= state_manager->state_cnt) {
        return -1;
    }

    if (state_manager->states[state_manager->current_state].on_exit) {
        state_manager->states[state_manager->current_state].on_exit(state_manager);
    }
    state_manager->current_state = next_state;
    if (state_manager->states[state_manager->current_state].on_enter) {
        state_manager->states[state_manager->current_state].on_enter(state_manager);
    }
    return 0;
}
int sm_update(state_manager_t *state_manager)
{
    if (!state_manager) {
        return -1;
    }

    if (state_manager->states[state_manager->current_state].update) {
        state_manager->states[state_manager->current_state].update(state_manager);
    }
    return 0;
}

/*
int sm_setup_state_manager(struct state_manager_ *state_manager)
{
    if (!state_manager) {
        return -1;
    }
    state_manager->state_cnt = 0;
    state_manager->ctx = NULL;
    return 0;
}
*/