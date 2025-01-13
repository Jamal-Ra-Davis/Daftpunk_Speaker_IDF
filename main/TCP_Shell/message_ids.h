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
    GPIO_SET_CONFIG,
    GPIO_READ,
    GPIO_WRITE,
    ADC_READ,
    BATT_GET_SOC,
    BATT_GET_VOLTAGE,
    RTC_GET_TIME,
    RTC_SET_TIME,
    NUM_MESSAGE_IDS,
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