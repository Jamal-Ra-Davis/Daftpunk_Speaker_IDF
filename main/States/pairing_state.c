#include "pairing_state.h"
#include "esp_log.h"
#include "rgb_manager.h"

#define TAG "PAIRING_STATE"

int pairing_state_init(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_init");
    return 0;
}
int pairing_state_on_enter(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_on_enter");
    // Flash LED to indicate pairing attempt in progress
    // Start timer 
    return 0;
}
int pairing_state_on_exit(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_on_exit");
    return 0;
}
int pairing_state_update(state_manager_t *state_manager)
{
    ESP_LOGI(TAG, "pairing_state_update");
    // Wait for pairing event or timeout
    // If pairing successful, enter PAIR_SUCCESS state
    // If pairing failed, enter PAIR_FAIL state
    return 0;
}