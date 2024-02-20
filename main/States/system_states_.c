#include "system_states_.h"
#include "esp_log.h"
#include "Events.h"

/*******************************
 * Data Type Definitions
 ******************************/

/*******************************
 * Global Data
 ******************************/
static state_element_t pairing_state = {
    .init = pairing_state_init,
    .on_enter = pairing_state_on_enter,
    .on_exit = pairing_state_on_exit,
    .update = pairing_state_update,
};
static state_element_t pairing_success_state = {
    .init = pairing_success_state_init,
    .on_enter = pairing_success_state_on_enter,
    .on_exit = pairing_success_state_on_exit,
    .update = pairing_success_state_update,
};
static state_element_t pairing_fail_state = {
    .init = pairing_fail_state_init,
    .on_enter = pairing_fail_state_on_enter,
    .on_exit = pairing_fail_state_on_exit,
    .update = pairing_fail_state_update,
};
static state_element_t idle_state = {
    .init = idle_state_init,
    .on_enter = idle_state_on_enter,
    .on_exit = idle_state_on_exit,
    .update = idle_state_update,
};
static state_element_t menu_state = {
    .init = menu_state_init,
    .on_enter = menu_state_on_enter,
    .on_exit = menu_state_on_exit,
    .update = menu_state_update,
};
static state_element_t display_off_state = {
    .init = display_off_state_init,
    .on_enter = display_off_state_on_enter,
    .on_exit = display_off_state_on_exit,
    .update = display_off_state_update,
};
static state_element_t streaming_state = {
    .init = streaming_state_init,
    .on_enter = streaming_state_on_enter,
    .on_exit = streaming_state_on_exit,
    .update = streaming_state_update,
};
static state_element_t sleep_state = {
    .init = sleep_state_init,
    .on_enter = sleep_state_on_enter,
    .on_exit = sleep_state_on_exit,
    .update = sleep_state_update,
};
static state_manager_context_t sm_ctx;
/*******************************
 * Function Prototypes
 ******************************/

/*******************************
 * Private Function Definitions
 ******************************/

/*******************************
 * Public Function Definitions
 ******************************/

system_states_t get_system_state(state_manager_t *state_manager)
{
    return (system_states_t)(state_manager->current_state);
}
int init_system_states(state_manager_t *state_manager)
{
    if (state_manager == NULL) {
        return -1;
    }

    sm_ctx.event_queue = get_event_queue_handle();
    sm_ctx.delay_ms = DEFAULT_STATE_DELAY_MS;
    sm_setup_state_manager(state_manager, NUM_SYSTEM_STATES_);
    
    sm_register_state(state_manager, PAIRING_STATE_, pairing_state);
    sm_register_state(state_manager, PAIR_SUCCESS_STATE_, pairing_success_state);
    sm_register_state(state_manager, PAIR_FAIL_STATE_, pairing_fail_state);
    sm_register_state(state_manager, IDLE_STATE_, idle_state);
    sm_register_state(state_manager, MENU_STATE_, menu_state);
    sm_register_state(state_manager, DISPLAY_OFF_STATE_, display_off_state);
    sm_register_state(state_manager, STREAMING_STATE_, streaming_state);
    sm_register_state(state_manager, SLEEP_STATE_, sleep_state);

    sm_init(state_manager, PAIRING_STATE_, (void*)(&sm_ctx));
    return 0;
}
int get_system_state_delay(state_manager_t *state_manager)
{
    if (state_manager == NULL) {
        return DEFAULT_STATE_DELAY_MS;
    }
    state_manager_context_t *ctx = (state_manager_context_t*)(state_manager->ctx);
    if (!ctx) {
        return DEFAULT_STATE_DELAY_MS;
    }
    return ctx->delay_ms;
}