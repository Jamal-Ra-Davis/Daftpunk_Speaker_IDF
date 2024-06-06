#pragma once
#include <stdint.h>

typedef enum {
    ACK, 
    NACK, 
    TEST, 
    ECHO, 
    NVM_START, 
    NVM_SEND_DATA, 
    NVM_STOP,
    NVM_ERASE_CHIP,
    AUDIO_LOAD_START,
    AUDIO_META_DATA,
    PLAY_AUDIO_ASSET,
    STACK_INFO,
    I2C_BUS_SCAN,
    I2C_WRITE,
    I2C_WRITE_READ,
    MEM_READ_SCRATCH,
    MEM_READ,
    MEM_WRITE,
    GPIO_GET_CONFIG,
    GPIO_SET_CONFIG,
    GPIO_READ,
    GPIO_WRITE,
    ADC_READ,
    BATT_GET_SOC,
    BATT_GET_VOLTAGE,
} message_id_t;

typedef struct __attribute__((packed))
{
    uint16_t message_id;
    uint16_t payload_size;
    uint8_t response_expected;
} tcp_message_header_t;
typedef struct __attribute__((packed))
{
    tcp_message_header_t header;
    uint16_t crc_header;
    uint16_t crc_payload;
    uint8_t payload[0];
} tcp_message_t;
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
