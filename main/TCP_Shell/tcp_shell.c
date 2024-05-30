#include "tcp_shell.h"
#include "freertos/semphr.h"
#include "global_defines.h"
#include "message_ids.h"

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <stdint.h>
#include <rom/crc.h>
#include "freertos/semphr.h"
#include "flash_manager.h"
#include "audio_manager.h"
#include "Stack_Info.h"


#define TCP_SHELL_TASK_STACK_SIZE 4096
#define TCP_SHELL_TASK_TAG "TCP_Shell_Task"
#define BUF_SZ 2048

#define PORT CONFIG_EXAMPLE_PORT
#define KEEPALIVE_IDLE CONFIG_EXAMPLE_KEEPALIVE_IDLE
#define KEEPALIVE_INTERVAL CONFIG_EXAMPLE_KEEPALIVE_INTERVAL
#define KEEPALIVE_COUNT CONFIG_EXAMPLE_KEEPALIVE_COUNT

// Type Declarations

// File Globals
static SemaphoreHandle_t xDataReadySem;
static TaskHandle_t xtcp_handler_task = NULL;
static TaskHandle_t xtcp_server_task = NULL;
static bool shell_active = false;
static char rx_buffer[BUF_SZ];
static char tx_buffer[BUF_SZ];
static int sock_handle = -1;

// Function Prototypes
static uint16_t simple_crc16(uint8_t start, uint8_t *buf, uint32_t len);
static bool message_is_valid(tcp_message_t *msg, uint32_t len);
static void do_retransmit(const int sock);
static void tcp_server_task(void *pvParameters);
static void tcp_handler_task(void *pvParameters);

// Public Functions
int init_tcp_shell_task()
{
    xDataReadySem = xSemaphoreCreateBinary();
    if (xDataReadySem == NULL)
    {
        ESP_LOGE(TCP_SHELL_TASK_TAG, "Could not allocate data ready semaphore");
        return -1;
    }

#ifdef CONFIG_EXAMPLE_IPV4
    xTaskCreate(tcp_server_task, "tcp_server", TCP_SHELL_TASK_STACK_SIZE, (void *)AF_INET, 5, &xtcp_server_task);
#endif
#ifdef CONFIG_EXAMPLE_IPV6
    xTaskCreate(tcp_server_task, "tcp_server", TCP_SHELL_TASK_STACK_SIZE, (void *)AF_INET6, 5, &xtcp_server_task);
#endif

    xTaskCreate(tcp_handler_task, "tcp_handler", 4096, NULL, 4, &xtcp_handler_task);
    return 0;
}

int stop_tcp_shell_task()
{
    shell_active = false;
    return 0;
}

TaskHandle_t tcp_handler_task_handle()
{
    return xtcp_handler_task;
}
TaskHandle_t tcp_server_task_handle()
{
    return xtcp_server_task;
}

// Private Functions
uint16_t simple_crc16(uint8_t start, uint8_t *buf, uint32_t len)
{
    if (buf == NULL) {
        return 0;
    }
    uint8_t data[2] = {start, start};

    for (uint32_t i=0; i<len; i++) {
        data[0] ^= buf[i];
        data[1] ^= (buf[i] << 4);
    }
    return ((uint16_t)data[0] | (uint16_t)(data[1] << 8));
}

static bool message_is_valid(tcp_message_t *msg, uint32_t len)
{
    if (!msg) {
        return false;
    }

    uint16_t crc_header;
    uint16_t crc_payload;

    if (len < sizeof(tcp_message_header_t)) {
        ESP_LOGE(TCP_SHELL_TASK_TAG, "Length of received payload (%u bytes) is less than message header size (%d bytes)", len, sizeof(tcp_message_header_t));
        return false;
    }

    crc_header = simple_crc16(0, (uint8_t*)&msg->header, sizeof(tcp_message_header_t));
    if (crc_header != msg->crc_header) {
        ESP_LOGE(TCP_SHELL_TASK_TAG, "CRC Message (0x%02X) does not mach CRC Calc (0x%02X)", msg->crc_header, crc_header);
        return false;
    }

    uint32_t msg_size = sizeof(tcp_message_t) + msg->header.payload_size;
    if (len != msg_size) {
        ESP_LOGE(TCP_SHELL_TASK_TAG, "Length of received payload (%u bytes) does not match expected message size (%u bytes)", len, msg_size);
        return false;
    }

    if (msg->header.payload_size > 0) {
        crc_payload = simple_crc16(0, msg->payload, msg->header.payload_size);
        if (crc_payload != msg->crc_payload) {
            ESP_LOGE(TCP_SHELL_TASK_TAG, "CRC Message (0x%02X) does not mach CRC Calc (0x%02X)", msg->crc_payload, crc_payload);
            return false;
        }
    }
    return true;
}

static void do_retransmit(const int sock)
{
    int len;

    do
    {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0)
        {
            ESP_LOGE(TCP_SHELL_TASK_TAG, "Error occurred during receiving: errno %d", errno);
        }
        else if (len == 0)
        {
            ESP_LOGW(TCP_SHELL_TASK_TAG, "Connection closed");
        }
        else
        {
            bool valid = true;
            tcp_message_t *msg;
            tcp_message_t resp;
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            
            msg = (tcp_message_t *)rx_buffer;
            valid = message_is_valid(msg, len);
            if (!valid) {
                resp.header.message_id = NACK;
                resp.header.payload_size = 0;
                resp.header.response_expected = 0;
                resp.crc_header = simple_crc16(0, (uint8_t*)&resp.header, sizeof(tcp_message_header_t));
                resp.crc_payload = 0;

                len = sizeof(tcp_message_t) + resp.header.payload_size;
                int to_write = len;
                while (to_write > 0)
                {
                    int written = send(sock, &resp + (len - to_write), to_write, 0);
                    if (written < 0)
                    {
                        ESP_LOGE(TCP_SHELL_TASK_TAG, "Error occurred during sending: errno %d", errno);
                    }
                    to_write -= written;
                }
                continue;
            }
            
            if (xSemaphoreGive(xDataReadySem) != pdTRUE)
            {
                // Failed to give semaphore
                ESP_LOGE(TCP_SHELL_TASK_TAG, "Failed to give semaphore");
            }
        }
    } while (len > 0);
}

static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    if (addr_family == AF_INET)
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }
#ifdef CONFIG_EXAMPLE_IPV6
    else if (addr_family == AF_INET6)
    {
        struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
        bzero(&dest_addr_ip6->sin6_addr.un, sizeof(dest_addr_ip6->sin6_addr.un));
        dest_addr_ip6->sin6_family = AF_INET6;
        dest_addr_ip6->sin6_port = htons(PORT);
        ip_protocol = IPPROTO_IPV6;
    }
#endif

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0)
    {
        ESP_LOGE(TCP_SHELL_TASK_TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

    ESP_LOGI(TCP_SHELL_TASK_TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(TCP_SHELL_TASK_TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TCP_SHELL_TASK_TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TCP_SHELL_TASK_TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0)
    {
        ESP_LOGE(TCP_SHELL_TASK_TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    shell_active = true;
    while (1)
    {

        ESP_LOGI(TCP_SHELL_TASK_TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0)
        {
            ESP_LOGE(TCP_SHELL_TASK_TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
        if (source_addr.ss_family == PF_INET)
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
#ifdef CONFIG_EXAMPLE_IPV6
        else if (source_addr.ss_family == PF_INET6)
        {
            inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
        }
#endif
        ESP_LOGI(TCP_SHELL_TASK_TAG, "Socket accepted ip address: %s", addr_str);
        // TODO: Replace with pushing event whose callback plays the sound.
        // Need to change system_events_t enum name as ESP code already uses that
        play_audio_sfx(AUDIO_SFX_CONNECT, false);

        sock_handle = sock;
        do_retransmit(sock);
        sock_handle = -1;

        shutdown(sock, 0);
        close(sock);
        play_audio_sfx(AUDIO_SFX_DISCONNECT, false);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

static void tcp_handler_task(void *pvParameters)
{
    ESP_LOGI(TCP_SHELL_TASK_TAG, "Starting handler task");
    tcp_message_t *msg = (tcp_message_t*)rx_buffer;
    tcp_message_t *resp = (tcp_message_t*)tx_buffer;
    int ret;
    while (1) {
        // Process messages from queue
        if (xSemaphoreTake(xDataReadySem, portMAX_DELAY) == pdFALSE)
        {
            ESP_LOGE(TCP_SHELL_TASK_TAG, "Failed to take semaphore");
            continue;
        }
        
        bool print_message = true;
        
        resp->header.payload_size = 0;
        resp->header.response_expected = 0;
        switch (msg->header.message_id) {
            case ACK:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "ACK MSG_ID");
                resp->header.message_id = ACK;
                break;
            case NACK:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "NACK MSG_ID");
                resp->header.message_id = NACK;
                break;
            case TEST:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "TEST MSG_ID");
                resp->header.message_id = TEST;
                resp->header.payload_size = sprintf((char*)resp->payload, "Test string");
                break;
            case ECHO:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "ECHO MSG_ID");
                resp->header.message_id = ECHO;
                resp->header.payload_size = sprintf((char*)resp->payload, "%s", msg->payload);
                break;
            case NVM_START:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "NVM_START MSG_ID");
                nvm_start_message_t *nvm_start_msg = (nvm_start_message_t*)msg->payload;
                ESP_LOGI(TCP_SHELL_TASK_TAG, "payload_size: %u, path_len: %u, file_path: %s",
                    nvm_start_msg->payload_size, nvm_start_msg->path_len, nvm_start_msg->file_path);

                ret = load_nvm_start(nvm_start_msg->file_path, nvm_start_msg->path_len, nvm_start_msg->payload_size);
                resp->header.message_id = (ret == 0) ? ACK : NACK;
                break;
            case NVM_SEND_DATA:
                ret = load_nvm_chunk(msg->payload, msg->header.payload_size);
                resp->header.message_id = (ret == 0) ? ACK : NACK;
                print_message = false;
                break;
            case NVM_STOP:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "NVM_STOP MSG_ID");

                ret = load_nvm_end();
                resp->header.message_id = (ret == 0) ? ACK : NACK;
                break;
            case NVM_ERASE_CHIP:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "NVM_STOP MSG_ID");
                ESP_LOGI(TCP_SHELL_TASK_TAG, "About to format NVM, this will take a while...");
                ret = nvm_erase_chip();
                resp->header.message_id = (ret == 0) ? ACK : NACK;
                break;
            case AUDIO_LOAD_START:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "AUDIO_LOAD_START MSG_ID");
                audio_load_start_message_t *audio_load_start_msg = (audio_load_start_message_t*)msg->payload;
                ESP_LOGI(TCP_SHELL_TASK_TAG, "payload_size: %u, audio_id: %u, file_name_len: %u, file_name: %s",
                    audio_load_start_msg->payload_size, audio_load_start_msg->audio_id, audio_load_start_msg->file_name_len, audio_load_start_msg->file_name);

                ret = load_audio_start(audio_load_start_msg->audio_id, audio_load_start_msg->file_name, audio_load_start_msg->file_name_len, audio_load_start_msg->payload_size);
                resp->header.message_id = (ret == 0) ? ACK : NACK;
                break;
            case AUDIO_META_DATA:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "AUDIO_META_DATA MSG_ID");
                ret = get_audio_metadata(resp->payload, &resp->header.payload_size);
                resp->header.message_id = (ret == 0) ? AUDIO_META_DATA : NACK;
                break;
            case PLAY_AUDIO_ASSET:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "PLAY_AUDIO_ASSET MSG_ID");
                play_audio_asset_message_t *play_audio_asset_msg = (play_audio_asset_message_t*)msg->payload;
                ret = play_audio_asset(play_audio_asset_msg->audio_id, false);
                resp->header.message_id = (ret == 0) ? ACK : NACK;
                break;
            case STACK_INFO:
                ESP_LOGI(TCP_SHELL_TASK_TAG, "STACK_INFO MSG_ID");
                resp->header.message_id = STACK_INFO;
                int size = get_stack_info((char*)resp->payload, sizeof(tx_buffer) - sizeof(tcp_message_t));
                if (size > 0) {
                    resp->header.payload_size = (uint32_t)size;
                }
                else {
                    resp->header.message_id = NACK;
                }
                break;
            default:
                break;
        }

        if (print_message) {
            ESP_LOGI(TCP_SHELL_TASK_TAG, "MSG_ID: %u, payload_size: %u, response_expected: %u, crc_header: 0x%02X, crc_payload: 0x%02X",
                msg->header.message_id, msg->header.payload_size, msg->header.response_expected, msg->crc_header, msg->crc_payload);
            if (msg->header.payload_size > 0) {  
                ESP_LOGI(TCP_SHELL_TASK_TAG, "Payload: %s",msg->payload);
            }
        }

        resp->crc_header = simple_crc16(0, (uint8_t*)&resp->header, sizeof(tcp_message_header_t));
        resp->crc_payload = 0;

        if (resp->header.payload_size > 0) {
            resp->crc_payload = simple_crc16(0, resp->payload, resp->header.payload_size);
        }

        uint32_t len = sizeof(tcp_message_t) + resp->header.payload_size;
        int to_write = len;
        while (to_write > 0)
        {
            int written = send(sock_handle, (uint8_t*)resp + (len - to_write), to_write, 0);
            if (written < 0)
            {
                ESP_LOGE(TCP_SHELL_TASK_TAG, "Error occurred during sending: errno %d", errno);
            }
            to_write -= written;
        }
    }
    xtcp_handler_task = NULL;
    vTaskDelete(NULL);
}