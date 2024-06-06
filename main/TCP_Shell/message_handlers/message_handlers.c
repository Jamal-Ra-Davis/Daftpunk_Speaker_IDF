#include "tcp_shell.h"
#include "freertos/semphr.h"
#include "global_defines.h"
#include "message_ids.h"

#include <string.h>
#include "esp_log.h"

#include <stdint.h>
#include "flash_manager.h"
#include "audio_manager.h"
#include "Stack_Info.h"
#include "Events.h"

#include "driver/i2c.h"
#include "message_handlers.h"

#define TAG "TCP_Msg_Handler"

typedef int (*tcp_shell_handler_t)(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);

static int ack_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int nack_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int test_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int echo_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int nvm_start_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int nvm_send_data_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int nvm_stop_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int nvm_erase_chip_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int audio_load_start_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int audio_meta_data_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int play_audio_asset_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int stack_info_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int i2c_bus_scan_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int i2c_write_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int i2c_write_read_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int mem_read_scratch_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int mem_read_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int mem_write_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);

static int gpio_get_config_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int gpio_set_config_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int gpio_read_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int gpio_write_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int adc_read_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int battery_get_soc_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);
static int battery_get_voltage_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message);

tcp_shell_handler_t handler_list[NUM_MESSAGE_IDS] = {
    ack_handler, // Misc
    nack_handler,
    test_handler,
    echo_handler,
    nvm_start_handler, // NVM
    nvm_send_data_handler,
    nvm_stop_handler,
    nvm_erase_chip_handler,
    audio_load_start_handler, // Audio
    audio_meta_data_handler,
    play_audio_asset_handler,
    stack_info_handler,   // Misc
    i2c_bus_scan_handler, // I2C
    i2c_write_handler,
    i2c_write_read_handler,
    mem_read_scratch_handler, // Mem
    mem_read_handler,
    mem_write_handler,
};

static uint8_t mem_scratch_buf[16] = {0xDE, 0xAD, 0xBE, 0xEF,
                                      0x00, 0x00, 0x00, 0x00,
                                      0xFF, 0xFF, 0xFF, 0xFF,
                                      0x01, 0x23, 0x45, 0x67};

int handle_shell_message(message_id_t id, tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    if (id >= NUM_MESSAGE_IDS)
    {
        ESP_LOGE(TAG, "Invalid message id (%d) received", id);
        return -1;
    }
    if (msg == NULL || resp == NULL || print_message == NULL)
    {
        ESP_LOGE(TAG, "Invalid pointer received");
        return -1;
    }
    if (handler_list[id] == NULL)
    {
        ESP_LOGE(TAG, "No handler registered for message ID: %d", id);
        return -1;
    }
    return handler_list[id](msg, resp, print_message);
}

static int ack_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "ACK MSG_ID");
    resp->header.message_id = ACK;
    return 0;
}
static int nack_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "NACK MSG_ID");
    resp->header.message_id = NACK;
    return 0;
}
static int test_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "TEST MSG_ID");
    resp->header.message_id = TEST;
    resp->header.payload_size = sprintf((char *)resp->payload, "Test string");
    return 0;
}
static int echo_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "ECHO MSG_ID");
    resp->header.message_id = ECHO;
    resp->header.payload_size = sprintf((char *)resp->payload, "%s", msg->payload);
    return 0;
}
static int nvm_start_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "NVM_START MSG_ID");
    nvm_start_message_t *nvm_start_msg = (nvm_start_message_t *)msg->payload;
    ESP_LOGI(TAG, "payload_size: %u, path_len: %u, file_path: %s",
             nvm_start_msg->payload_size, nvm_start_msg->path_len, nvm_start_msg->file_path);

    int ret = load_nvm_start(nvm_start_msg->file_path, nvm_start_msg->path_len, nvm_start_msg->payload_size);
    resp->header.message_id = (ret == 0) ? ACK : NACK;
    return 0;
}
static int nvm_send_data_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    int ret = load_nvm_chunk(msg->payload, msg->header.payload_size);
    resp->header.message_id = (ret == 0) ? ACK : NACK;
    *print_message = false;
    return 0;
}
static int nvm_stop_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "NVM_STOP MSG_ID");

    int ret = load_nvm_end();
    resp->header.message_id = (ret == 0) ? ACK : NACK;
    return 0;
}
static int nvm_erase_chip_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "NVM_STOP MSG_ID");
    ESP_LOGI(TAG, "About to format NVM, this will take a while...");
    int ret = nvm_erase_chip();
    resp->header.message_id = (ret == 0) ? ACK : NACK;
    return 0;
}
static int audio_load_start_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "AUDIO_LOAD_START MSG_ID");
    audio_load_start_message_t *audio_load_start_msg = (audio_load_start_message_t *)msg->payload;
    ESP_LOGI(TAG, "payload_size: %u, audio_id: %u, file_name_len: %u, file_name: %s",
             audio_load_start_msg->payload_size, audio_load_start_msg->audio_id, audio_load_start_msg->file_name_len, audio_load_start_msg->file_name);

    int ret = load_audio_start(audio_load_start_msg->audio_id, audio_load_start_msg->file_name, audio_load_start_msg->file_name_len, audio_load_start_msg->payload_size);
    resp->header.message_id = (ret == 0) ? ACK : NACK;
    return 0;
}
static int audio_meta_data_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "AUDIO_META_DATA MSG_ID");
    int ret = get_audio_metadata(resp->payload, &resp->header.payload_size);
    resp->header.message_id = (ret == 0) ? AUDIO_META_DATA : NACK;
    return 0;
}
static int play_audio_asset_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "PLAY_AUDIO_ASSET MSG_ID");
    play_audio_asset_message_t *play_audio_asset_msg = (play_audio_asset_message_t *)msg->payload;
    int ret = play_audio_asset(play_audio_asset_msg->audio_id, false);
    resp->header.message_id = (ret == 0) ? ACK : NACK;
    return 0;
}
static int stack_info_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "STACK_INFO MSG_ID");
    resp->header.message_id = STACK_INFO;
    int size = get_stack_info((char *)resp->payload, TCP_SHELL_BUF_SZ - sizeof(tcp_message_t));
    if (size > 0)
    {
        resp->header.payload_size = (uint32_t)size;
    }
    else
    {
        resp->header.message_id = NACK;
    }
    return 0;
}
static int i2c_bus_scan_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGE(TAG, "I2C_BUS_SCAN MSG_ID not currently supported");
    resp->header.message_id = NACK;
    return 0;
}
static int i2c_write_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "I2C_WRITE MSG_ID");
    i2c_write_message_t *i2c_write_msg = (i2c_write_message_t *)msg->payload;

    int ret = i2c_master_write_to_device(i2c_write_msg->bus,
                                         i2c_write_msg->dev_addr,
                                         i2c_write_msg->wbuf,
                                         i2c_write_msg->len,
                                         MS_TO_TICKS(i2c_write_msg->timeout));

    resp->header.message_id = (ret == ESP_OK) ? ACK : NACK;
    return 0;
}
static int i2c_write_read_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "I2C_WRITE_READ MSG_ID");
    i2c_write_read_message_t *i2c_write_read_msg = (i2c_write_read_message_t *)msg->payload;
    int ret = i2c_master_write_read_device(i2c_write_read_msg->bus,
                                           i2c_write_read_msg->dev_addr,
                                           &i2c_write_read_msg->reg_addr,
                                           1,
                                           resp->payload,
                                           i2c_write_read_msg->len,
                                           MS_TO_TICKS(i2c_write_read_msg->timeout));
    if (ret != ESP_OK)
    {
        resp->header.message_id = NACK;
        return 0;
    }
    resp->header.payload_size = i2c_write_read_msg->len;
    resp->header.message_id = I2C_WRITE_READ;
    return 0;
}
static int mem_read_scratch_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "MEM_READ_SCRATCH MSG_ID");
    mem_scratch_read_resp_t *mem_scratch_resp = (mem_scratch_read_resp_t *)resp->payload;
    mem_scratch_resp->addr = (uint32_t)(&mem_scratch_buf);
    ESP_LOGI(TAG, "Scratch Mem Addr: 0x%x", mem_scratch_resp->addr);
    for (size_t i = 0; i < sizeof(mem_scratch_buf); i++)
    {
        ESP_LOGI(TAG, "buf[%d] = 0x%x", i, mem_scratch_buf[i]);
    }

    memcpy(mem_scratch_resp->data, mem_scratch_buf, sizeof(mem_scratch_buf));

    resp->header.payload_size = sizeof(mem_scratch_read_resp_t) + sizeof(mem_scratch_buf);
    resp->header.message_id = MEM_READ_SCRATCH;
    return 0;
}
static int mem_read_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "MEM_READ MSG_ID");
    mem_read_message_t *mem_read_msg = (mem_read_message_t *)msg->payload;
    mem_read_resp_t *mem_read_resp = (mem_read_resp_t *)resp->payload;
    mem_read_resp->val = *(uint32_t *)mem_read_msg->addr;
    resp->header.payload_size = sizeof(mem_read_resp_t);
    resp->header.message_id = MEM_READ;
    return 0;
}
static int mem_write_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "MEM_WRITE MSG_ID");
    mem_write_message_t *mem_write_msg = (mem_write_message_t *)msg->payload;
    uint32_t *waddr = (uint32_t *)mem_write_msg->addr;
    *waddr = mem_write_msg->val;
    resp->header.message_id = ACK;
    return 0;
}
static int gpio_get_config_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "GPIO_GET_CONFIG MSG_ID");
    return 0;
}
static int gpio_set_config_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "GPIO_SET_CONFIG MSG_ID");
    // gpio_config_t cfg = {};
    // gpio_config(&cfg);
    return 0;
}
static int gpio_read_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "GPIO_READ MSG_ID");
    // gpio_get_level(gpio_num);
    return 0;
}
static int gpio_write_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "GPIO_WRITE MSG_ID");
    // gpio_set_level(gpio_num);
    return 0;
}
static int adc_read_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "ADC_READ MSG_ID");
    return 0;
}
static int battery_get_soc_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "BATT_GET_SOC MSG_ID");
    // max17048_get_soc(uint8_t * soc);
    return 0;
}
static int battery_get_voltage_handler(tcp_message_t *msg, tcp_message_t *resp, bool *print_message)
{
    ESP_LOGI(TAG, "BATT_GET_VOLTAGE MSG_ID");
    // max17048_get_voltage(float *voltage);
    return 0;
}