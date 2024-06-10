#pragma once
#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint32_t payload_size;
    uint16_t path_len;
    char file_path[0];
} nvm_start_message_t;
typedef struct __attribute__((packed))
{
    uint32_t payload_size;
    uint8_t audio_id;
    uint16_t file_name_len;
    char file_name[0];
} audio_load_start_message_t;
typedef struct __attribute__((packed))
{
    uint8_t audio_id;
} play_audio_asset_message_t;
typedef struct __attribute__((packed))
{
    uint8_t bus;
    uint8_t dev_addr;
    uint16_t timeout;
    uint16_t len;
    uint8_t wbuf[0];
} i2c_write_message_t;
typedef struct __attribute__((packed))
{
    uint8_t bus;
    uint8_t dev_addr;
    uint16_t timeout;
    uint8_t reg_addr;
    uint16_t len;
} i2c_write_read_message_t;
typedef struct __attribute__((packed))
{
    uint32_t addr;
    uint8_t data[0];
} mem_scratch_read_resp_t;
typedef struct __attribute__((packed))
{
    uint32_t addr;
} mem_read_message_t;
typedef struct __attribute__((packed))
{
    uint32_t val;
} mem_read_resp_t;
typedef struct __attribute__((packed))
{
    uint32_t addr;
    uint32_t val;
} mem_write_message_t;

typedef enum {PIN_STRAP_DISABLE, PIN_STRAP_PULLUP, PIN_STRAP_PULLDOWN};
typedef struct __attribute__((packed))
{
    uint8_t pin_num;
    uint8_t pin_mode; 
    uint8_t pin_strap;
    uint8_t pin_int_type; // Default to disabled in script
} gpio_set_config_message_t;
typedef struct __attribute__((packed))
{
    uint8_t pin_num;
} gpio_read_message_t;
typedef struct __attribute__((packed))
{
    uint8_t val;
} gpio_read_resp_t;
typedef struct __attribute__((packed))
{
    uint8_t pin_num;
    uint8_t val;
} gpio_write_message_t;