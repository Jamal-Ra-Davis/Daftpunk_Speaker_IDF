#include "PI4IOE5V6408.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "sdkconfig.h"

// TODO: Generalize driver to support multiple instances of device. With different addresses on different I2C busses

#define I2C_MASTER_NUM 0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_TIMEOUT_MS 1000
#define TAG "PI4IOE5V640_DRIVER"

// Device Address
#define PI4IOE5V640_DEV_ADDRESS_0 0x43
#define PI4IOE5V640_DEV_ADDRESS_1 0x66

// Device Registers
#define PI4IOE5V6408_REG_DEV_ID_CTRL 0x01
#define PI4IOE5V6408_REG_IO_DIR 0x03
#define PI4IOE5V6408_REG_OUTPUT_STATE 0x05
#define PI4IOE5V6408_REG_OUTPUT_HIZ 0x07
#define PI4IOE5V6408_REG_INPUT_DEFAULT_STATE 0x09
#define PI4IOE5V6408_REG_PULL_UP_DOWN_EN 0x0B
#define PI4IOE5V6408_REG_PULL_UP_DOWN_SEL 0x0D
#define PI4IOE5V6408_REG_INPUT_STATUS 0x0F
#define PI4IOE5V6408_REG_INTERRUPT_MASK 0x11
#define PI4IOE5V6408_REG_INTERRUPT_STATUS 0x13

// PI4IOE5V6408_REG_DEV_ID_CTRL Reg Bits
#define DEV_ID_CTRL_MFG_ID_MASK 0x07
#define DEV_ID_CTRL_MFG_ID_POS 5
#define DEV_ID_CTRL_FW_REV_MASK 0x07
#define DEV_ID_CTRL_FW_REV_POS 2
#define DEV_ID_CTRL_RST_INT_MASK 0x01
#define DEV_ID_CTRL_RST_INT_POS 1
#define DEV_ID_CTRL_SW_RST_MASK 0x01
#define DEV_ID_CTRL_SW_RST_POS 1

#define DEV_ID_CTRL_MFG_ID_DEFAULT 0x05
#define DEV_ID_CTRL_FW_REV_DEFAULT 0x00
struct dev_id_ctrl_reg {
    union {
        struct {
            uint8_t sw_rst : 1;
            uint8_t rst_int : 1;
            uint8_t fw_rev : 3;
            uint8_t mfg_id : 3;
        } fields;
        uint8_t raw;
    };
};

// PI4IOE5V6408_REG_IO_DIR - (0 = input, 1 = output)

// PI4IOE5V6408_REG_OUTPUT_STATE - (0 = LOW, 1 = HIGH)

// PI4IOE5V6408_REG_OUTPUT_HIGH_IMP - (0 = Output, 1 = Hi-Z)

// PI4IOE5V6408_REG_INPUT_DEFAULT_STATE

// PI4IOE5V6408_REG_PULL_UP_DOWN_EN - (0 = Strapping disabled, 1 = Strapping Enabled)

// PI4IOE5V6408_REG_PULL_UP_DOWN_SEL - (0 = PullDown, 1 = PullUp)

// PI4IOE5V6408_REG_INPUT_STATUS - (0 = LOW, 1 = HIGH)

// PI4IOE5V6408_REG_INTERRUPT_MASK - (0 = Interrupt Enabled, 1 = Interrupt Disabled)

// PI4IOE5V6408_REG_INTERRUPT_STATUS - Bit set when bit changes from default setting

static esp_err_t PI4IOE5V6408_register_read(uint8_t reg_addr, uint8_t *data)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, PI4IOE5V640_DEV_ADDRESS_0, &reg_addr, 1, data, 1, I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
}
static esp_err_t PI4IOE5V6408_register_write(uint8_t reg_addr, uint8_t data)
{
    int ret;
    uint8_t write_buf[] = {reg_addr, data};

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, PI4IOE5V640_DEV_ADDRESS_0, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
    return ret;
}
static esp_err_t PI4IOE5V6408_register_update(uint8_t reg_addr, uint8_t data, uint8_t mask)
{
    int ret;
    uint8_t reg;
    ret = PI4IOE5V6408_register_read(reg_addr, &reg);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t wdata = (reg & ~mask) | (data & mask); 
    return PI4IOE5V6408_register_write(reg_addr, wdata);
}

static esp_err_t reg_get_template(uint8_t reg, uint8_t *reg_val)
{
    if (!reg_val) {
        return ESP_FAIL;
    }
    return PI4IOE5V6408_register_read(reg, reg_val);
}
static esp_err_t reg_get_pin_template(uint8_t reg, uint8_t pin_num, bool *pin_val)
{
    if (pin_num >= 8) {
        return ESP_FAIL;
    }
    int ret;
    uint8_t reg_val;
    uint8_t mask = BIT(pin_num);
    ret = PI4IOE5V6408_register_read(reg, &reg_val);
    if (ret != ESP_OK) {
        return ret;
    }
    *pin_val = (bool)(mask & reg);
    return ESP_OK;
}
static esp_err_t reg_set_pin_template(uint8_t reg, uint8_t pin_num, bool pin_val)
{
    if (pin_num >= 8) {
        return ESP_FAIL;
    }
    uint8_t mask = BIT(pin_num);
    uint8_t data = (pin_val) ? mask : 0;
    return PI4IOE5V6408_register_update(reg, data, mask);
}



esp_err_t PI4IOE5V6408_init(gpio_num_t int_pin, gpio_isr_t isr_handler, void *args)
{
    int ret;
    // Perform SW reset to put in known state
    ret = PI4IOE5V6408_sw_reset();
    if (ret != ESP_OK) {
        return ret;
    }

    // Disable all interrupts
    ret = PI4IOE5V6408_register_write(PI4IOE5V6408_REG_INTERRUPT_MASK, 0xFF);
    if (ret != ESP_OK) {
        return ret;
    }

    // Read Manufacturing ID and Firmware revision and compare to expected values
    struct dev_id_ctrl_reg reg;
    ret = PI4IOE5V6408_register_read(PI4IOE5V6408_REG_DEV_ID_CTRL, (uint8_t*)&reg);
    if (ret != ESP_OK) {
        return ret;
    }

    if (reg.fields.fw_rev != DEV_ID_CTRL_FW_REV_DEFAULT) {
        return ESP_FAIL;
    }

    if (reg.fields.mfg_id != DEV_ID_CTRL_MFG_ID_DEFAULT) {
        return ESP_FAIL;
    }

    // If valid GPIO pin is passed in, configure GPIO for interrupts
    if (int_pin != GPIO_NUM_NC) {
        ret = gpio_isr_handler_add(int_pin, isr_handler, args);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ret;
}



esp_err_t PI4IOE5V6408_sw_reset()
{
    return PI4IOE5V6408_register_update(PI4IOE5V6408_REG_DEV_ID_CTRL, DEV_ID_CTRL_SW_RST_MASK, DEV_ID_CTRL_SW_RST_MASK);
}

esp_err_t PI4IOE5V6408_reset_interrupt()
{
    uint8_t reg;
    return PI4IOE5V6408_register_read(PI4IOE5V6408_REG_DEV_ID_CTRL, &reg);
}

esp_err_t PI4IOE5V6408_get_io_dir(uint8_t *io_dir)
{
    return reg_get_template(PI4IOE5V6408_REG_IO_DIR, io_dir);
}
esp_err_t PI4IOE5V6408_get_io_dir_pin(uint8_t pin_num, bool *io_dir)
{
    return reg_get_pin_template(PI4IOE5V6408_REG_IO_DIR, pin_num, io_dir);
}
esp_err_t PI4IOE5V6408_set_io_dir(uint8_t io_dir)
{
    return PI4IOE5V6408_register_write(PI4IOE5V6408_REG_IO_DIR, io_dir);
}
esp_err_t PI4IOE5V6408_set_io_dir_pin(uint8_t pin_num, bool io_dir)
{
    return reg_set_pin_template(PI4IOE5V6408_REG_IO_DIR, pin_num, io_dir);
}

esp_err_t PI4IOE5V6408_get_output_state(uint8_t *output_state)
{
    return reg_get_template(PI4IOE5V6408_REG_OUTPUT_STATE, output_state);
}
esp_err_t PI4IOE5V6408_get_output_state_pin(uint8_t pin_num, bool *output_state)
{
    return reg_get_pin_template(PI4IOE5V6408_REG_OUTPUT_STATE, pin_num, output_state);
}
esp_err_t PI4IOE5V6408_set_output_state(uint8_t output_state)
{
    return PI4IOE5V6408_register_write(PI4IOE5V6408_REG_OUTPUT_STATE, output_state);
}
esp_err_t PI4IOE5V6408_set_output_state_pin(uint8_t pin_num, bool output_state)
{
    return reg_set_pin_template(PI4IOE5V6408_REG_OUTPUT_STATE, pin_num, output_state);
}

esp_err_t PI4IOE5V6408_get_output_hiz(uint8_t *out_hiz)
{
    return reg_get_template(PI4IOE5V6408_REG_OUTPUT_HIZ, out_hiz);
}
esp_err_t PI4IOE5V6408_get_output_hiz_pin(uint8_t pin_num, bool *out_hiz)
{
    return reg_get_pin_template(PI4IOE5V6408_REG_OUTPUT_HIZ, pin_num, out_hiz);
}
esp_err_t PI4IOE5V6408_set_output_hiz(uint8_t out_hiz)
{
    return PI4IOE5V6408_register_write(PI4IOE5V6408_REG_OUTPUT_HIZ, out_hiz);
}
esp_err_t PI4IOE5V6408_set_output_hiz_pin(uint8_t pin_num, bool out_hiz)
{
    return reg_set_pin_template(PI4IOE5V6408_REG_OUTPUT_HIZ, pin_num, out_hiz);
}

esp_err_t PI4IOE5V6408_get_input_default(uint8_t *input_default)
{
    return reg_get_template(PI4IOE5V6408_REG_INPUT_DEFAULT_STATE, input_default);
}
esp_err_t PI4IOE5V6408_get_input_default_pin(uint8_t pin_num, bool *input_default)
{
    return reg_get_pin_template(PI4IOE5V6408_REG_INPUT_DEFAULT_STATE, pin_num, input_default);
}
esp_err_t PI4IOE5V6408_input_default(uint8_t input_default)
{
    return PI4IOE5V6408_register_write(PI4IOE5V6408_REG_INPUT_DEFAULT_STATE, input_default);
}
esp_err_t PI4IOE5V6408_input_default_pin(uint8_t pin_num, bool input_default)
{
    return reg_set_pin_template(PI4IOE5V6408_REG_INPUT_DEFAULT_STATE, pin_num, input_default);
}

esp_err_t PI4IOE5V6408_get_pin_strap_en(uint8_t *enable)
{
    return reg_get_template(PI4IOE5V6408_REG_PULL_UP_DOWN_EN, enable);
}
esp_err_t PI4IOE5V6408_get_pin_strap_en_pin(uint8_t pin_num, bool *enable)
{
    return reg_get_pin_template(PI4IOE5V6408_REG_PULL_UP_DOWN_EN, pin_num, enable);
}
esp_err_t PI4IOE5V6408_set_pin_strap_en(uint8_t enable)
{
    return PI4IOE5V6408_register_write(PI4IOE5V6408_REG_PULL_UP_DOWN_EN, enable);
}
esp_err_t PI4IOE5V6408_set_pin_strap_en_pin(uint8_t pin_num, bool enable)
{
    return reg_set_pin_template(PI4IOE5V6408_REG_PULL_UP_DOWN_EN, pin_num, enable);
}

esp_err_t PI4IOE5V6408_get_pull_up_down_en(uint8_t *pu_enable)
{
    return reg_get_template(PI4IOE5V6408_REG_PULL_UP_DOWN_SEL, pu_enable);
}
esp_err_t PI4IOE5V6408_get_pull_up_down_en_pin(uint8_t pin_num, bool *pu_enable)
{
    return reg_get_pin_template(PI4IOE5V6408_REG_PULL_UP_DOWN_SEL, pin_num, pu_enable);
}
esp_err_t PI4IOE5V6408_set_pull_up_down_en(uint8_t pu_enable)
{
    return PI4IOE5V6408_register_write(PI4IOE5V6408_REG_PULL_UP_DOWN_SEL, pu_enable);
}
esp_err_t PI4IOE5V6408_set_pull_up_down_en_pin(uint8_t pin_num, bool pu_enable)
{
    return reg_set_pin_template(PI4IOE5V6408_REG_PULL_UP_DOWN_SEL, pin_num, pu_enable);
}

esp_err_t PI4IOE5V6408_get_input_status(uint8_t *input_status)
{
    return reg_get_template(PI4IOE5V6408_REG_INPUT_STATUS, input_status);
}
esp_err_t PI4IOE5V6408_get_input_status_pin(uint8_t pin_num, bool *input_status)
{
    return reg_get_pin_template(PI4IOE5V6408_REG_INPUT_STATUS, pin_num, input_status);
}

esp_err_t PI4IOE5V6408_get_interrupt_mask(uint8_t *interrupt_mask)
{
    return reg_get_template(PI4IOE5V6408_REG_INTERRUPT_MASK, interrupt_mask);
}
esp_err_t PI4IOE5V6408_get_interrupt_mask_pin(uint8_t pin_num, bool *interrupt_mask)
{
    return reg_get_pin_template(PI4IOE5V6408_REG_INTERRUPT_MASK, pin_num, interrupt_mask);
}
esp_err_t PI4IOE5V6408_set_interrupt_mask(uint8_t interrupt_mask)
{
    return PI4IOE5V6408_register_write(PI4IOE5V6408_REG_INTERRUPT_MASK, interrupt_mask);
}
esp_err_t PI4IOE5V6408_set_interrupt_mask_pin(uint8_t pin_num, bool interrupt_mask)
{
    return reg_set_pin_template(PI4IOE5V6408_REG_INTERRUPT_MASK, pin_num, interrupt_mask);
}

esp_err_t PI4IOE5V6408_get_interrupt_status(uint8_t *interrupt_status)
{
    return reg_get_template(PI4IOE5V6408_REG_INTERRUPT_STATUS, interrupt_status);
}