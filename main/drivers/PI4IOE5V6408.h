#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>
#include "driver/gpio.h"

esp_err_t PI4IOE5V6408_init(gpio_num_t int_pin, gpio_isr_t isr_handler, void *args);
esp_err_t PI4IOE5V6408_sw_reset();
esp_err_t PI4IOE5V6408_reset_interrupt();

esp_err_t PI4IOE5V6408_get_io_dir(uint8_t *io_dir);
esp_err_t PI4IOE5V6408_get_io_dir_pin(uint8_t pin_num, bool *io_dir);
esp_err_t PI4IOE5V6408_set_io_dir(uint8_t io_dir);
esp_err_t PI4IOE5V6408_set_io_dir_pin(uint8_t pin_num, bool io_dir);

esp_err_t PI4IOE5V6408_get_output_state(uint8_t *output_state);
esp_err_t PI4IOE5V6408_get_output_state_pin(uint8_t pin_num, bool *output_state);
esp_err_t PI4IOE5V6408_set_output_state(uint8_t output_state);
esp_err_t PI4IOE5V6408_set_output_state_pin(uint8_t pin_num, bool output_state);

esp_err_t PI4IOE5V6408_get_output_hiz(uint8_t *out_hiz);
esp_err_t PI4IOE5V6408_get_output_hiz_pin(uint8_t pin_num, bool *out_hiz);
esp_err_t PI4IOE5V6408_set_output_hiz(uint8_t out_hiz);
esp_err_t PI4IOE5V6408_set_output_hiz_pin(uint8_t pin_num, bool out_hiz);

esp_err_t PI4IOE5V6408_get_input_default(uint8_t *input_default);
esp_err_t PI4IOE5V6408_get_input_default_pin(uint8_t pin_num, bool *input_default);
esp_err_t PI4IOE5V6408_input_default(uint8_t input_default);
esp_err_t PI4IOE5V6408_input_default_pin(uint8_t pin_num, bool input_default);

esp_err_t PI4IOE5V6408_get_pin_strap_en(uint8_t *enable);
esp_err_t PI4IOE5V6408_get_pin_strap_en_pin(uint8_t pin_num, bool *enable);
esp_err_t PI4IOE5V6408_set_pin_strap_en(uint8_t enable);
esp_err_t PI4IOE5V6408_set_pin_strap_en_pin(uint8_t pin_num, bool enable);

esp_err_t PI4IOE5V6408_get_pull_up_down_en(uint8_t *pu_enable);
esp_err_t PI4IOE5V6408_get_pull_up_down_en_pin(uint8_t pin_num, bool *pu_enable);
esp_err_t PI4IOE5V6408_set_pull_up_down_en(uint8_t pu_enable);
esp_err_t PI4IOE5V6408_set_pull_up_down_en_pin(uint8_t pin_num, bool pu_enable);

esp_err_t PI4IOE5V6408_get_input_status(uint8_t *input_status);
esp_err_t PI4IOE5V6408_get_input_status_pin(uint8_t pin_num, bool *input_status);

esp_err_t PI4IOE5V6408_get_interrupt_mask(uint8_t *interrupt_mask);
esp_err_t PI4IOE5V6408_get_interrupt_mask_pin(uint8_t pin_num, bool *interrupt_mask);
esp_err_t PI4IOE5V6408_set_interrupt_mask(uint8_t interrupt_mask);
esp_err_t PI4IOE5V6408_set_interrupt_mask_pin(uint8_t pin_num, bool interrupt_mask);

esp_err_t PI4IOE5V6408_get_interrupt_status(uint8_t *interrupt_status);