#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <string.h>
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "flash_manager.h"
#include "bt_audio.h"
#include "i2s_task.h"
#include "sdkconfig.h"
#include "global_defines.h"
#include "audio_manager.h"

#define TAG "AUDIO_MANAGER"

#define AUDIO_QUEUE_LENGTH 8
#define AUDIO_MANAGER_TASK_STACK_SIZE 3072

#define AUDIO_META_DATA_MAGIC 0xDEADBEEF
#define AUDIO_NUM_SLOTS 8

/*******************************
 * Data Type Definitions
 ******************************/
typedef struct __attribute__((packed)) {
    bool active;
    char file_name[32];
} audio_meta_data_t;
#define AUDIO_META_DATA_SIZE (AUDIO_NUM_SLOTS * sizeof(audio_meta_data_t) + 2 * sizeof(uint32_t))
            
typedef struct {
    bool active;
    char file_name[32];
    uint8_t audio_id;
} load_audio_data_t;

typedef struct {
    FILE *fp;
    char file_path[64];
    uint32_t payload_size;
    uint32_t bytes_remaining;
} nvm_command_data_t;

typedef struct __attribute__((packed)) {
    uint8_t audio_id;
    SemaphoreHandle_t blocking_sem;
} audio_request_t;

/*******************************
 * Global Data
 ******************************/
static uint8_t rbuf[4096] __attribute__ ((aligned (4)));
static uint8_t metadata_buf[AUDIO_META_DATA_SIZE];
static char *audio_md_filename = "audio_md.bin";
static audio_meta_data_t audio_meta_data[AUDIO_NUM_SLOTS] = {{.active = 0}};
static int8_t audio_sfx_map[NUM_AUDIO_SFX];

static nvm_command_data_t nvm_command_data = {.fp = NULL, .payload_size = 0, .bytes_remaining = 0};
static load_audio_data_t audio_data_cache = {.active = false};

static QueueHandle_t audio_queue = NULL;
static TaskHandle_t xaudio_task = NULL;

 /*******************************
 * Function Prototypes
 ******************************/
static void print_audio_metadata();
static int write_audio_metadata();
static int load_audio_metadata(FILE *f, char *filename);
static int play_audio_asset_local(uint8_t audio_id);

static void audio_manager_task(void *pvParameters);

 /*******************************
 * Private Function Definitions
 ******************************/
static void print_audio_metadata()
{
    for (int i=0; i<AUDIO_NUM_SLOTS; i++) {
        if (audio_meta_data[i].active) {
            ESP_LOGI(TAG, "Audio Data Slot %d, filename = %s", i, audio_meta_data[i].file_name);
        }
        else {
            ESP_LOGI(TAG, "Audio Data Slot %d, INACTIVE", i);
        }
    }
}
static int write_audio_metadata()
{
    char md_file_path[64];
    sprintf(md_file_path, "%s/%s", FLASH_BASE_PATH, audio_md_filename);

    FILE *f = fopen(md_file_path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to create audio metadata file");
        return -1; 
    }

    uint32_t magic = AUDIO_META_DATA_MAGIC;
    size_t N = fwrite((uint8_t*)&magic, 1, sizeof(uint32_t), f);
    if (N != sizeof(uint32_t)) {
        ESP_LOGE(TAG, "Failed to write magic header.");
        fclose(f);
        return -1;
    }

    N = fwrite((uint8_t*)audio_meta_data, 1, AUDIO_NUM_SLOTS*sizeof(audio_meta_data_t), f);
    if (N != AUDIO_NUM_SLOTS*sizeof(audio_meta_data_t)) {
        ESP_LOGE(TAG, "Failed to write audio meta data.");
        fclose(f);
        return -1;
    }

    N = fwrite((uint8_t*)&magic, 1, sizeof(uint32_t), f);
    if (N != sizeof(uint32_t)) {
        ESP_LOGE(TAG, "Failed to write magic footer.");
        fclose(f);
        return -1;
    }
    ESP_LOGW(TAG, "Successfully created audio metadata file");
    
    // Readout data
    fclose(f);
    f = NULL;
    f = fopen(md_file_path, "rb");
    if (f == NULL) {
        ESP_LOGW(TAG, "Failed to open file for reading");
        return -1;
    }
    N = fread(metadata_buf, 1, sizeof(metadata_buf), f);
    ESP_LOGI(TAG, "%d bytes read from %s", N, md_file_path);
    fclose(f);
    return 0;
}
static int load_audio_metadata(FILE *f, char *filename)
{
    // File exists
    uint32_t *magic;
    size_t N;

    N = fread(metadata_buf, 1, sizeof(metadata_buf), f);
    ESP_LOGI(TAG, "%d bytes read from %s", N, filename);

    if (N != AUDIO_META_DATA_SIZE) {
        ESP_LOGE(TAG, "Expected %d bytes from %s, but only got %d bytes", AUDIO_META_DATA_SIZE, filename, N);
        return -1;
    }
    
    magic = (uint32_t*)&metadata_buf[0];
    if (*magic != AUDIO_META_DATA_MAGIC) {
        ESP_LOGE(TAG, "Invalid magic header (0x%08X) for audio meta data", *magic);
        return -1;
    }
    magic = (uint32_t*)&metadata_buf[AUDIO_META_DATA_SIZE - sizeof(uint32_t)];
    if (*magic != AUDIO_META_DATA_MAGIC) {
        ESP_LOGE(TAG, "Invalid magic footer (0x%08X) for audio meta data", *magic);
        return -1;
    }

    memcpy(audio_meta_data, &metadata_buf[sizeof(uint32_t)], AUDIO_NUM_SLOTS*sizeof(audio_meta_data_t));
    return 0;
}

static int play_audio_asset_local(uint8_t audio_id)
{
    if (audio_id >= AUDIO_NUM_SLOTS) {
        ESP_LOGE(TAG, "Invalid audio ID (%u), must be between 0 and %u", audio_id, AUDIO_NUM_SLOTS);
        return -1;
    }
    if (!audio_meta_data[audio_id].active) {
        ESP_LOGE(TAG, "No asset present at audio ID (%u)", audio_id);
        return -1;
    }

    static bool first = true;

    if (first) {
        first = false;
        bt_i2s_task_start_up();
    }

#ifdef CONFIG_AUDIO_ENABLED
    uint8_t vol = bt_audio_get_volume();
#else 
    uint8_t vol = 15;
#endif
    vol = 15;
    ESP_LOGI(TAG, "Vol = %d", vol);
    float vol_scale = (float)vol / 0x7f;

    char file_path[64];
    snprintf(file_path, sizeof(file_path), "%s/%s", FLASH_BASE_PATH, audio_meta_data[audio_id].file_name);
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file (%s) for reading", file_path);
        return -1;
    }  
    
    while (1) {
        size_t N = fread(rbuf, 1, sizeof(rbuf), fp);
        if (N == 0) {
            break;
        }
        for (int i = 0; i < N; i += 4)
        {
            int16_t *left = (int16_t *)(&rbuf[i]);
            int16_t *right = (int16_t *)(&rbuf[i + 2]);

            *left = (int16_t)(*left * vol_scale);
            *right = (int16_t)(*right * vol_scale);
        }
        while (1) {
            size_t bytes_written = write_ringbuf(rbuf, N);
            if (bytes_written == N) {
                break;
            }
            vTaskDelay(85 / portTICK_PERIOD_MS);
        }
    }

    fclose(fp);
    flush_ringbuf();
    ESP_LOGI(TAG, "Finshed loading audio from flash");
    return 0;
}

static void audio_manager_task(void *pvParameters)
{
    if (audio_queue == NULL)
    {
        ESP_LOGI(TAG, "Could not get handle to system event queue");
        vTaskDelete(NULL);
    }

    audio_request_t req;
    while (1)
    {
        xQueueReceive(audio_queue, &req, portMAX_DELAY);
        play_audio_asset_local(req.audio_id);
        if (req.blocking_sem == NULL) {
            continue;
        }
        if (xSemaphoreGive(req.blocking_sem) != pdTRUE) {
            // Failed to give semaphore
            ESP_LOGE(TAG, "Failed to give semaphore");
        }
    }
}

static void reset_nvm_data()
{
    if (nvm_command_data.fp) {
        fclose(nvm_command_data.fp);
        nvm_command_data.fp = NULL;
    }
    nvm_command_data.payload_size = 0;
    nvm_command_data.bytes_remaining = 0;

    audio_data_cache.active = false;
}

static int play_audio_asset_general(uint8_t audio_id, bool isr, bool blocking, TickType_t xTicksToWait)
{
    if (audio_id >= AUDIO_NUM_SLOTS) {
        ESP_LOGE(TAG, "Invalid audio ID (%u), must be between 0 and %u", audio_id, AUDIO_NUM_SLOTS);
        return -1;
    }
    if (!audio_meta_data[audio_id].active) {
        ESP_LOGE(TAG, "No asset present at audio ID (%u)", audio_id);
        return -1;
    }
    if (audio_queue == NULL)
    {
        ESP_LOGE(TAG, "Audio queue uninitialized");
        return -1;
    }

    BaseType_t ret;
    audio_request_t req = {
        .audio_id = audio_id,
        .blocking_sem = NULL,
    };
    if (isr)
    {
        // In ISR context (cannot block while in ISR)
        ret = xQueueSendFromISR(audio_queue, (void *)&req, NULL);
    }
    else if (!blocking)
    {
        // Not blocking
        ret = xQueueSend(audio_queue, (void *)&req, (TickType_t)0);
    }
    else {
        // Blockng
        req.blocking_sem = xSemaphoreCreateBinary();
        if (req.blocking_sem == NULL)
        {
            ESP_LOGE(TAG, "Could not allocate blocking semaphore");
            return -1;
        }

        ret = xQueueSend(audio_queue, (void *)&req, (TickType_t)0);

        if (xSemaphoreTake(req.blocking_sem, xTicksToWait) == pdFALSE)
        {
            ESP_LOGE(TAG, "Failed to take semaphore");
            return -1;
        }
    }

    return (ret == pdTRUE) ? 0 : -1;
}

/*******************************
 * Public Function Definitions
 ******************************/

int audio_manager_init()
{
    int ret;
    char filename[64];
    FILE *f;

    // Init audio sfx map
    for (int i=0; i<NUM_AUDIO_SFX; i++) {
        audio_sfx_map[i] = -1;
    }

    // Default Audio Mapping
    map_audio_sfx(AUDIO_SFX_POWERON, 0);
    map_audio_sfx(AUDIO_SFX_SLEEP, 1);
    map_audio_sfx(AUDIO_SFX_WAKE, 2);
    map_audio_sfx(AUDIO_SFX_CONNECT, 3);
    map_audio_sfx(AUDIO_SFX_DISCONNECT, 4);
    map_audio_sfx(AUDIO_SFX_VOLUME_UP, 5);
    map_audio_sfx(AUDIO_SFX_VOLUME_DOWN, 5);

    // Create message queue for audio data
    audio_queue = xQueueCreate(AUDIO_QUEUE_LENGTH, sizeof(audio_request_t));
    if (audio_queue == NULL)
    {
        ESP_LOGI(TAG, "Failed to create audio queue");
        return -1;
    }

    // Load audio/create audio metadata file/structure
    // Load audio metadata file
    sprintf(filename, "%s/%s", FLASH_BASE_PATH, audio_md_filename);
    f = fopen(filename, "rb");
    if (f == NULL) {
        ESP_LOGW(TAG, "Failed to open file for reading, creating audio metadata file...");
        ret = write_audio_metadata();
        if (ret < 0) {
            return -1;
        }
        ESP_LOGI(TAG, "Successfully created audio metadata file");
        
        // Readout data
        f = fopen(filename, "rb");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open metadata file for reading");
            return -1;
        }
        size_t N = fread(rbuf, 1, sizeof(rbuf), f);
        fclose(f);
        ESP_LOGI(TAG, "%d bytes read from %s", N, filename);
    }
    else {
        // File exists
        ret = load_audio_metadata(f, filename);
        fclose(f);
        if (ret < 0) {
            return -1;
        }
    }
    print_audio_metadata();
    

    // Start audio thread
    xTaskCreate(
        audio_manager_task,
        "Audio_Manager_Task",
        AUDIO_MANAGER_TASK_STACK_SIZE,
        NULL,
        AUDIO_MANAGER_TASK_PRIORITY,
        &xaudio_task);
    return 0;   
}

int play_audio_asset(uint8_t audio_id, bool isr)
{
   return play_audio_asset_general(audio_id, isr, false, (TickType_t)0);
}

int play_audio_sfx(audio_sfx_t sfx, bool isr)
{
    if (sfx >= NUM_AUDIO_SFX) {
        ESP_LOGE(TAG, "Invalid audio sfx ID (%u), must be between 0 and %u", (unsigned)sfx, NUM_AUDIO_SFX);
        return -1;
    }
    int8_t audio_id = audio_sfx_map[sfx];
    if (audio_id < 0) {
        return -1;
    }
    return play_audio_asset((uint8_t)audio_id, isr);
}

int play_audio_asset_blocking(uint8_t audio_id, TickType_t xTicksToWait)
{
    return play_audio_asset_general(audio_id, false, true, xTicksToWait);
}

int play_audio_sfx_blocking(audio_sfx_t sfx, TickType_t xTicksToWait)
{
    if (sfx >= NUM_AUDIO_SFX) {
        ESP_LOGE(TAG, "Invalid audio sfx ID (%u), must be between 0 and %u", (unsigned)sfx, NUM_AUDIO_SFX);
        return -1;
    }
    int8_t audio_id = audio_sfx_map[sfx];
    if (audio_id < 0) {
        return -1;
    }
    return play_audio_asset_blocking((uint8_t)audio_id, xTicksToWait);
}

int map_audio_sfx(audio_sfx_t sfx, uint8_t audio_id)
{
    if (audio_id >= AUDIO_NUM_SLOTS) {
        ESP_LOGE(TAG, "Invalid audio ID (%u), must be between 0 and %u", audio_id, AUDIO_NUM_SLOTS);
        return -1;
    }
    if (sfx >= NUM_AUDIO_SFX) {
        ESP_LOGE(TAG, "Invalid audio sfx ID (%u), must be between 0 and %u", (unsigned)sfx, NUM_AUDIO_SFX);
        return -1;
    }
    audio_sfx_map[sfx] = (int8_t)audio_id;
    return 0;
}

int load_audio_start(uint8_t audio_id, char *filename, uint16_t filename_len, uint32_t payload_len)
{
    if (audio_id >= AUDIO_NUM_SLOTS) {
        ESP_LOGE(TAG, "Invalid audio id: %d, must be between 0 and %d", audio_id, AUDIO_NUM_SLOTS-1);
        return -1;
    }
    if (filename == NULL) {
        ESP_LOGE(TAG, "Invalid filename, cannot be NULL");
        return -1;
    }

    if (nvm_command_data.fp) {
        ESP_LOGE(TAG, "File already open, cannot open new one");
        return -1;
    }

    snprintf(nvm_command_data.file_path, sizeof(nvm_command_data.file_path), "%s/%s", FLASH_BASE_PATH, filename);
    ESP_LOGI(TAG, "Opening Audio NVM File for writing: %s", nvm_command_data.file_path);

    nvm_command_data.fp = fopen(nvm_command_data.file_path, "wb");
    if (nvm_command_data.fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return -1;
    }

    nvm_command_data.payload_size = payload_len;
    nvm_command_data.bytes_remaining = payload_len;
    
    snprintf(audio_data_cache.file_name, sizeof(audio_data_cache.file_name), "%s", filename);
    audio_data_cache.audio_id = audio_id;
    audio_data_cache.active = true;

    ESP_LOGI(TAG, "payload_size: %u, Bytes Remaining: %u", nvm_command_data.payload_size, nvm_command_data.bytes_remaining);
    return 0;  
}

int load_nvm_start(char *filename, uint16_t path_len, uint32_t payload_len)
{
    if (nvm_command_data.fp) {
        ESP_LOGE(TAG, "File already open, cannot open new one");
        return -1;
    }
    snprintf(nvm_command_data.file_path, sizeof(nvm_command_data.file_path), "%s/%s.bin", FLASH_BASE_PATH, "audio0");
    ESP_LOGI(TAG, "Opening NVM File for writing: %s", nvm_command_data.file_path);

    nvm_command_data.fp = fopen(nvm_command_data.file_path, "wb");
    if (nvm_command_data.fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return -1;
    }  

    nvm_command_data.payload_size = payload_len;
    nvm_command_data.bytes_remaining = payload_len;

    ESP_LOGI(TAG, "payload_size: %u, Bytes Remaining: %u", nvm_command_data.payload_size, nvm_command_data.bytes_remaining);
    return 0;
}

int load_nvm_chunk(uint8_t *payload, uint32_t chunk_len)
{
    if (!nvm_command_data.fp) {
        return -1;
    }
    size_t N = fwrite(payload, 1, chunk_len, nvm_command_data.fp);
    nvm_command_data.bytes_remaining -= chunk_len;
    
    if (chunk_len != N) {
        ESP_LOGE(TAG, "chunk_len != N");
        ESP_LOGE(TAG, "Chunk_len: %u, N: %u, Bytes Remaining: %u", chunk_len, N, nvm_command_data.bytes_remaining);
    }
    return 0;
}

int load_nvm_end()
{
    ESP_LOGI(TAG, "Closing NVM File");
    bool loading_audio = audio_data_cache.active;
    reset_nvm_data();

    uint8_t flash_data[256];
    nvm_command_data.fp = fopen(nvm_command_data.file_path, "rb");
    if (nvm_command_data.fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return -1;
    }  
    ESP_LOGI(TAG, "About to read %u bytes from flash", sizeof(flash_data));
    fread(flash_data, 1, sizeof(flash_data), nvm_command_data.fp);
    ESP_LOGI(TAG, "Finished reading from flash");
    for (int i = 0; i < sizeof(flash_data); i += 8)
    {
        ESP_LOGI(TAG, "%d (0x%X): 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", 
            i,  i, 
            flash_data[i], flash_data[i + 1], flash_data[i + 2], flash_data[i + 3], 
            flash_data[i + 4], flash_data[i + 5], flash_data[i + 6], flash_data[i + 7]);
    }

    if (loading_audio) {
        ESP_LOGI(TAG, "Updating audio metadata");
        size_t N = sizeof(audio_meta_data[audio_data_cache.audio_id].file_name);
        snprintf(audio_meta_data[audio_data_cache.audio_id].file_name, N, "%s", audio_data_cache.file_name);
        audio_meta_data[audio_data_cache.audio_id].active = true;

        if (write_audio_metadata() < 0) {
            ESP_LOGE(TAG, "Failed to write audio metadata");
        }
    }

    reset_nvm_data();
    return 0;
}

int get_audio_metadata(uint8_t *buf, uint16_t *payload_size)
{
    if (!buf) {
        ESP_LOGE(TAG, "Invalid buffer pointer");
        return -1;
    }
    if (!payload_size) {
        ESP_LOGE(TAG, "Invalid size pointer");
        return -1;
    }
    memcpy(buf, audio_meta_data, sizeof(audio_meta_data));
    *payload_size = sizeof(audio_meta_data);
    return 0;
}

