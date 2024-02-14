#include "system_states_.h"
#include "esp_log.h"

system_states_t get_system_state(state_manager_t *state_manager)
{
    return (system_states_t)(state_manager->current_state);
}