#include "BoardRevision.h"
#include "PI4IOE5V6408.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "global_defines.h"

#define BOARD_ID_MASK 0x1C
#define BOARD_ID_POS 2
#define TAG "BOARD_REVISION"

#define VBUS_DET_PIN_V2 6

#define RGB_LED_EN_PIN_V0 GPIO_NUM_17
#define RGB_LED_EN_PIN_V2 1

#define AMP_SD_PIN_V0 GPIO_NUM_4
#define AMP_SD_PIN_V2 0

#define LED_3V3_ENB_PIN 5

#define IO_EXP_VID_PIN0 2
#define IO_EXP_VID_PIN1 3
#define IO_EXP_VID_PIN2 4

static board_rev_t revision = BOARD_REV_INVALID;

static void vbus_det_interrupt_handler();

static void IRAM_ATTR PI4IOE5V6408_isr_handler(void *arg)
{
    ESP_LOGI(TAG, "IO Expander Interrupt");
    // TODO: Need to offload to separate thread or workqueue
    vbus_det_interrupt_handler();
}

// Note: This must be called after IO expander has been initialized
board_rev_t get_board_revision()
{
    if (revision != BOARD_REV_INVALID)
    {
        return revision;
    }

    // Attempt to read input status from IO expander
    // If it fails then assume that board is V0
    uint8_t board_id;
    int ret = PI4IOE5V6408_get_input_status(&board_id);
    if (ret != ESP_OK)
    {
        revision = BOARD_REV_V0;
        return revision;
    }

    board_id &= BOARD_ID_MASK;
    board_id >>= BOARD_ID_POS;
    revision = (board_rev_t)board_id;
    return revision;
}

esp_err_t config_io_expander()
{
    // IO Expander GPIO Setup
    // Call init function
    // VBUS_DET (P6) -> Input (No Pin Strapping)
    // 3V3 LED ENB (P5) -> Input (Pulldown) **[In Standby Mode -> Output (HIGH)]
    // V_ID2 (P4) -> Input (No Pin Strapping)
    // V_ID1 (P3) -> Input (No Pin Strapping)
    // V_ID0 (P2) -> Input (No Pin Strapping)
    // STATUS_LED_EN (P1) -> Output (HIGH) **[In Standby Mode -> Output (LOW)]
    // AMP_SD (P0) -> Output (LOW) **[In Standby Mode -> Output (HIGH)]

    board_rev_t board_revision;
    esp_err_t esp_ret = ESP_OK;
    esp_ret = PI4IOE5V6408_init(GPIO_NUM_4, PI4IOE5V6408_isr_handler, NULL);
    //ESP_ERROR_CHECK(esp_ret);

    // If init fails, assume that board is board revision is V0 and IO expander is not present
    if (esp_ret != ESP_OK)
    {
        get_board_revision();
        return esp_ret;
    }

    uint8_t vid_pins[] = {IO_EXP_VID_PIN0, IO_EXP_VID_PIN1, IO_EXP_VID_PIN2};
    // Config VID pins
    for (int i = 0; i < sizeof(vid_pins); i++)
    {
        esp_ret = PI4IOE5V6408_set_io_dir_pin(vid_pins[i], false);
        ESP_ERROR_CHECK(esp_ret);
        if (esp_ret != ESP_OK)
        {
            return esp_ret;
        }

        esp_ret = PI4IOE5V6408_set_pin_strap_en_pin(vid_pins[i], false);
        ESP_ERROR_CHECK(esp_ret);
        if (esp_ret != ESP_OK)
        {
            return esp_ret;
        }
    }
    board_revision = get_board_revision();
    ESP_LOGI(TAG, "Board Revision = %d", (int)board_revision);

    // Config AMP_SD pin
    esp_ret = ESP_OK;
    esp_ret |= PI4IOE5V6408_set_io_dir_pin(0, true);
    esp_ret |= PI4IOE5V6408_set_output_state_pin(0, false);
    esp_ret |= PI4IOE5V6408_set_pin_strap_en_pin(0, false);
    esp_ret |= PI4IOE5V6408_set_output_hiz_pin(0, false);
    if (esp_ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Config STATUS_LED_EN pin
    esp_ret = ESP_OK;
    esp_ret |= PI4IOE5V6408_set_io_dir_pin(1, true);
    esp_ret |= PI4IOE5V6408_set_output_state_pin(1, true);
    esp_ret |= PI4IOE5V6408_set_pin_strap_en_pin(1, false);
    esp_ret |= PI4IOE5V6408_set_output_hiz_pin(1, false);
    if (esp_ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Config 3V3 LED ENB pin
    esp_ret = ESP_OK;
    esp_ret |= PI4IOE5V6408_set_io_dir_pin(5, true);
    esp_ret |= PI4IOE5V6408_set_output_state_pin(5, false);
    esp_ret |= PI4IOE5V6408_set_pin_strap_en_pin(5, false);
    esp_ret |= PI4IOE5V6408_set_output_hiz_pin(5, false);
    if (esp_ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Config VBUS_DET pin
    bool vbus_det;
    uint8_t pin_status;
    esp_ret = ESP_OK;
    esp_ret |= PI4IOE5V6408_set_io_dir_pin(6, false);
    esp_ret |= PI4IOE5V6408_set_pin_strap_en_pin(6, false);
    esp_ret |= PI4IOE5V6408_get_input_status_pin(6, &vbus_det);
    esp_ret |= PI4IOE5V6408_get_input_status(&pin_status);
    esp_ret |= PI4IOE5V6408_input_default_pin(6, vbus_det);
    if (esp_ret != ESP_OK)
    {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "VBUS_DET = %d, pin_status = 0x%02X", (int)vbus_det, pin_status);

    return ESP_OK;
}

bool vbus_is_present()
{
    if (get_board_revision() >= BOARD_REV_V0)
    {
        return false;
    }
    bool present;
    esp_err_t ret = PI4IOE5V6408_get_input_status_pin(VBUS_DET_PIN_V2, &present);
    if (ret != ESP_OK)
    {
        return false;
    }
    return present;
}
void vbus_det_interrupt_handler()
{
    // TODO: Need to move generalized interrupt handling to IO Expander driver, eventually want to just register callback for each pin
    // IO expander code would be responsible for reading interrupt status, and calling corresponding callbacks

    // Read interrupt status
    // if (vbus pin is active)
    // Check VBUS status
    // Set default to opposite
    // Execute callback function
}
void config_vbus_det_interrupt()
{
    // TODO: Register callback to execute when VBUS status changes
}

// 3.3V LED control
esp_err_t power_led_enable(bool enable)
{
    board_rev_t board_rev = get_board_revision();
    if (board_rev == BOARD_REV_INVALID)
    {
        return ESP_FAIL;
    }
    else if (board_rev == BOARD_REV_V0)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }
    else
    {
        // If additional power savings are needed, may need to look at pullup/pulldown settings

        // Power LED enable signal is active LOW, while function wording is active HIGH
        return PI4IOE5V6408_set_output_state_pin(LED_3V3_ENB_PIN, !enable);
    }
    return ESP_OK;
}

// AMP SD control
esp_err_t amp_shutdown_assert(bool shutdown)
{
    board_rev_t board_rev = get_board_revision();
    if (board_rev == BOARD_REV_INVALID)
    {
        return ESP_FAIL;
    }
    else if (board_rev == BOARD_REV_V0)
    {
        return gpio_set_level(AMP_SD_PIN_V0, (uint32_t)shutdown);
    }
    else
    {
        // If additional power savings are needed, may need to look at pullup/pulldown settings
            
        return PI4IOE5V6408_set_output_state_pin(AMP_SD_PIN_V2, shutdown);
    }
    return ESP_OK;
}

// RGB LED control
esp_err_t rgb_led_enable(bool enable)
{
    board_rev_t board_rev = get_board_revision();
    if (board_rev == BOARD_REV_INVALID)
    {
        return ESP_FAIL;
    }
    else if (board_rev == BOARD_REV_V0)
    {
        return gpio_set_level(RGB_LED_EN_PIN_V0, (uint32_t)enable);
    }
    else
    {
        return PI4IOE5V6408_set_output_state_pin(RGB_LED_EN_PIN_V2, enable);
    }
    return ESP_OK;
}