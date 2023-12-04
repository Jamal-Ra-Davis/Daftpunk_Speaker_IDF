#include <string.h>
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "flash_manager.h"
#include "bt_app_core.h"
#include "bt_audio.h"

#define FLASH_TAG "FLASH_MANAGER"

#ifdef CONFIG_AUDIO_ENABLED
extern void bt_i2s_driver_install(void);
#endif

static esp_flash_t *example_init_ext_flash(void);

#define AUDIO_BYTE_LEN 664112 //36544
static const uint8_t magic_seq[8] = {0xfa, 0xff, 0x00, 0x00, 0xf4, 0xff, 0x00, 0x00};
static uint8_t rbuf[4096] __attribute__ ((aligned (4)));
static esp_flash_t *flash = NULL;
static bool flash_loaded = false;

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
const char *base_path = "/extflash";

static const esp_partition_t *example_add_partition(esp_flash_t *ext_flash, const char *partition_label);
static void example_list_data_partitions(void);
static bool example_mount_fatfs(const char *partition_label);
static void example_get_fatfs_usage(size_t *out_total_bytes, size_t *out_free_bytes);

#define AUDIO_META_DATA_MAGIC 0xDEADBEEF
#define AUDIO_NUM_SLOTS 8
static char *audio_md_filename = "audio_md.bin"; 
typedef struct __attribute__((packed)) {
    bool active;
    char file_name[32];
} audio_meta_data_t;
#define AUDIO_META_DATA_SIZE (AUDIO_NUM_SLOTS * sizeof(audio_meta_data_t) + 2 * sizeof(uint32_t))

bool audio_meta_valid = false;
audio_meta_data_t audio_meta_data[AUDIO_NUM_SLOTS] = {{.active = 0}};

int write_audio_metadata()
{
    char md_file_path[64];
    sprintf(md_file_path, "%s/%s", base_path, audio_md_filename);

    FILE *f = fopen(md_file_path, "wb");
    if (f == NULL) {
        ESP_LOGE(FLASH_TAG, "Failed to create audio metadata file");
        return -1; 
    }

    uint32_t magic = AUDIO_META_DATA_MAGIC;
    size_t N = fwrite((uint8_t*)&magic, 1, sizeof(uint32_t), f);
    if (N != sizeof(uint32_t)) {
        ESP_LOGE(FLASH_TAG, "Failed to write magic header.");
        fclose(f);
        return -1;
    }

    N = fwrite((uint8_t*)audio_meta_data, 1, AUDIO_NUM_SLOTS*sizeof(audio_meta_data_t), f);
    if (N != AUDIO_NUM_SLOTS*sizeof(audio_meta_data_t)) {
        ESP_LOGE(FLASH_TAG, "Failed to write audio meta data.");
        fclose(f);
        return -1;
    }

    N = fwrite((uint8_t*)&magic, 1, sizeof(uint32_t), f);
    if (N != sizeof(uint32_t)) {
        ESP_LOGE(FLASH_TAG, "Failed to write magic footer.");
        fclose(f);
        return -1;
    }
    ESP_LOGW(FLASH_TAG, "Successfully created audio metadata file");
    
    // Readout data
    fclose(f);
    f = NULL;
    f = fopen(md_file_path, "rb");
    if (f == NULL) {
        ESP_LOGW(FLASH_TAG, "Failed to open file for reading");
        return -1;
    }
    N = fread(rbuf, 1, sizeof(rbuf), f);
    ESP_LOGI(FLASH_TAG, "%d bytes read from %s", N, md_file_path);
    fclose(f);
    return 0;
}

void flash_init()
{
    char filename[64];
    flash = example_init_ext_flash();
    if (flash == NULL)
    {
        return;
    }

    // Add the entire external flash chip as a partition
    const char *partition_label = "storage";
    example_add_partition(flash, partition_label);

    // List the available partitions
    example_list_data_partitions();

    // Initialize FAT FS in the partition
    if (!example_mount_fatfs(partition_label))
    {
        flash_loaded = false;
        return;
    }

    // Print FAT FS size information
    size_t bytes_total, bytes_free;
    example_get_fatfs_usage(&bytes_total, &bytes_free);
    ESP_LOGI(FLASH_TAG, "FAT FS: %d kB total, %d kB free", bytes_total / 1024, bytes_free / 1024);
    
    FILE *f;
    /*
    // Open file for reading
    ESP_LOGI(FLASH_TAG, "Reading file");
    f = fopen("/extflash/hello.txt", "rb");
    if (f == NULL)
    {
        ESP_LOGE(FLASH_TAG, "Failed to open file for reading");
        return;
    }
    char line[128];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char *pos = strchr(line, '\n');
    if (pos)
    {
        *pos = '\0';
    }
    ESP_LOGI(FLASH_TAG, "Read from file: '%s'", line);
    flash_loaded = true;
    */

    // Load audio metadata file
    sprintf(filename, "%s/%s", base_path, audio_md_filename);
    f = fopen(filename, "rb");
    //fclose(f);
    //f = NULL;
    if (f == NULL) {
        ESP_LOGW(FLASH_TAG, "Failed to open file for reading, creating audio metadata file...");
        f = fopen(filename, "wb");
        if (f == NULL) {
            ESP_LOGE(FLASH_TAG, "Failed to create audio metadata file");
            return;
        }

        uint32_t magic = AUDIO_META_DATA_MAGIC;
        size_t N = fwrite((uint8_t*)&magic, 1, sizeof(uint32_t), f);
        if (N != sizeof(uint32_t)) {
            ESP_LOGE(FLASH_TAG, "Failed to write magic header.");
            fclose(f);
            return;
        }

        N = fwrite((uint8_t*)audio_meta_data, 1, AUDIO_NUM_SLOTS*sizeof(audio_meta_data_t), f);
        if (N != AUDIO_NUM_SLOTS*sizeof(audio_meta_data_t)) {
            ESP_LOGE(FLASH_TAG, "Failed to write audio meta data.");
            fclose(f);
            return;
        }

        N = fwrite((uint8_t*)&magic, 1, sizeof(uint32_t), f);
        if (N != sizeof(uint32_t)) {
            ESP_LOGE(FLASH_TAG, "Failed to write magic footer.");
            fclose(f);
            return;
        }
        ESP_LOGW(FLASH_TAG, "Successfully created audio metadata file");
        
        // Readout data
        fclose(f);
        f = NULL;
        f = fopen(filename, "rb");
        if (f == NULL) {
            ESP_LOGW(FLASH_TAG, "Failed to open file for reading");
            return;
        }
        N = fread(rbuf, 1, sizeof(rbuf), f);
        ESP_LOGI(FLASH_TAG, "%d bytes read from %s", N, filename);
    }
    else {
        // File exists
        uint32_t *magic;
        size_t N;

        N = fread(rbuf, 1, sizeof(rbuf), f);
        ESP_LOGI(FLASH_TAG, "%d bytes read from %s", N, filename);
        size_t expected_size = 2 * sizeof(uint32_t) + AUDIO_NUM_SLOTS*sizeof(audio_meta_data_t);
        if (N != expected_size) {
            ESP_LOGE(FLASH_TAG, "Expected %d bytes from %s, but only got %d bytes", expected_size, filename, N);
            fclose(f);
            return;
        }
        fclose(f);
        
        magic = (uint32_t*)&rbuf[0];
        if (*magic != AUDIO_META_DATA_MAGIC) {
            ESP_LOGE(FLASH_TAG, "Invalid magic header (0x%08X) for audio meta data", *magic);
            return;
        }
        magic = (uint32_t*)&rbuf[expected_size - sizeof(uint32_t)];
        if (*magic != AUDIO_META_DATA_MAGIC) {
            ESP_LOGE(FLASH_TAG, "Invalid magic footer (0x%08X) for audio meta data", *magic);
            return;
        }

        memcpy(audio_meta_data, &rbuf[sizeof(uint32_t)], AUDIO_NUM_SLOTS*sizeof(audio_meta_data_t));
        for (int i=0; i<AUDIO_NUM_SLOTS; i++) {
            if (audio_meta_data[i].active) {
                ESP_LOGI(FLASH_TAG, "Audio Data Slot %d, filename = %s", i, audio_meta_data[i].file_name);
            }
            else {
                ESP_LOGI(FLASH_TAG, "Audio Data Slot %d, INACTIVE", i);
            }
        }
        return;
    }
    audio_meta_valid = true;
    return;


    esp_err_t err;
    err = esp_flash_read(flash, (void *)rbuf, 0, sizeof(magic_seq));
    if (err != ESP_OK)
    {
        ESP_ERROR_CHECK(err);
        return;
    }
    bool flash_check = true;
    for (int i=0; i<sizeof(magic_seq); i++) {
        if (rbuf[i] != magic_seq[i]) {
            flash_check = false;
            break;
        }
    }
    //flash_check = true;
    flash_loaded = flash_check;
    if (!flash_loaded) {
        ESP_LOGE(FLASH_TAG, "Audio data not loaded in flash");
        return;
    }

    uint32_t address = 0;
    for (int j=0; j<1; j++) {
        ESP_LOGI(FLASH_TAG, "About to read %u bytes from flash", sizeof(rbuf));
        err = esp_flash_read(flash, (void *)rbuf, address, sizeof(rbuf));
        ESP_LOGI(FLASH_TAG, "Finished reading from flash");
        if (err != ESP_OK)
        {
            ESP_ERROR_CHECK(err);
            return;
        }
        /*
        for (int i = 0; i < sizeof(rbuf); i += 8)
        {
            ESP_LOGI(FLASH_TAG, "%d (0x%X): 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", address+i,  address+i, rbuf[i], rbuf[i + 1], rbuf[i + 2], rbuf[i + 3], rbuf[i + 4], rbuf[i + 5], rbuf[i + 6], rbuf[i + 7]);   
        }
        */

        ESP_LOGI(FLASH_TAG, "---------------------------------------");
        address += sizeof(rbuf);
    }
}

static esp_flash_t *example_init_ext_flash(void)
{
    const spi_bus_config_t bus_config = {
        .mosi_io_num = HSPI_IOMUX_PIN_NUM_MOSI,
        .miso_io_num = HSPI_IOMUX_PIN_NUM_MISO,
        .sclk_io_num = HSPI_IOMUX_PIN_NUM_CLK,
        .quadhd_io_num = -1, // VSPI_IOMUX_PIN_NUM_HD,
        .quadwp_io_num = -1, // VSPI_IOMUX_PIN_NUM_WP,
    };

    const esp_flash_spi_device_config_t device_config = {
        .host_id = HSPI_HOST,
        .cs_id = 0,
        .cs_io_num = HSPI_IOMUX_PIN_NUM_CS,
        .io_mode = SPI_FLASH_DIO,
        .speed = ESP_FLASH_26MHZ,
    };

    ESP_LOGI(FLASH_TAG, "Initializing external SPI Flash");
    ESP_LOGI(FLASH_TAG, "Pin assignments:");
    ESP_LOGI(FLASH_TAG, "MOSI: %2d   MISO: %2d   SCLK: %2d   CS: %2d",
             bus_config.mosi_io_num, bus_config.miso_io_num,
             bus_config.sclk_io_num, device_config.cs_io_num);

    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &bus_config, SPI_DMA_CH_AUTO));

    // Add device to the SPI bus
    esp_flash_t *ext_flash;
    ESP_ERROR_CHECK(spi_bus_add_flash_device(&ext_flash, &device_config));

    // Probe the Flash chip and initialize it
    esp_err_t err = esp_flash_init(ext_flash);
    if (err != ESP_OK)
    {
        ESP_LOGE(FLASH_TAG, "Failed to initialize external Flash: %s (0x%x)", esp_err_to_name(err), err);
        return NULL;
    }

    // Print out the ID and size
    uint32_t id;
    ESP_ERROR_CHECK(esp_flash_read_id(ext_flash, &id));
    ESP_LOGI(FLASH_TAG, "Initialized external Flash, size=%d KB, ID=0x%x", ext_flash->size / 1024, id);

    return ext_flash;
}

void play_sound()
{
    ESP_LOGI(FLASH_TAG, "Flash Loaded: %d", (int)flash_loaded);
    if (flash == NULL) {
        ESP_LOGE(FLASH_TAG, "Invalid handle to flash driver");
        return;
    }
    if (!flash_loaded) {
        ESP_LOGE(FLASH_TAG, "Audio not loaded into flash, can't play sound");
        return;
    }
    bt_i2s_task_start_up();
#ifdef CONFIG_AUDIO_ENABLED
    bt_i2s_driver_install();
#endif

    uint8_t vol = bt_audio_get_volume();
    vol = 15;
    ESP_LOGI(FLASH_TAG, "Vol = %d", vol);
    float vol_scale = (float)vol / 0x7f;

    char file_path[64];
    snprintf(file_path, sizeof(file_path), "%s/%s.bin", base_path, "audio0");
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL)
    {
        ESP_LOGE(FLASH_TAG, "Failed to open file (%s) for reading", file_path);
        return;
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
    ESP_LOGI(FLASH_TAG, "Finshed loading audio from flash");
}




static const esp_partition_t *example_add_partition(esp_flash_t *ext_flash, const char *partition_label)
{
    ESP_LOGI(FLASH_TAG, "Adding external Flash as a partition, label=\"%s\", size=%d KB", partition_label, ext_flash->size / 1024);
    const esp_partition_t *fat_partition;
    ESP_ERROR_CHECK(esp_partition_register_external(ext_flash, 0, ext_flash->size, partition_label, ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, &fat_partition));
    return fat_partition;
}

static void example_list_data_partitions(void)
{
    ESP_LOGI(FLASH_TAG, "Listing data partitions:");
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);

    for (; it != NULL; it = esp_partition_next(it))
    {
        const esp_partition_t *part = esp_partition_get(it);
        ESP_LOGI(FLASH_TAG, "- partition '%s', subtype %d, offset 0x%x, size %d kB",
                 part->label, part->subtype, part->address, part->size / 1024);
    }

    esp_partition_iterator_release(it);
}

static bool example_mount_fatfs(const char *partition_label)
{
    ESP_LOGI(FLASH_TAG, "Mounting FAT filesystem");
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};
    esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, partition_label, &mount_config, &s_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(FLASH_TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return false;
    }
    return true;
}

static void example_get_fatfs_usage(size_t *out_total_bytes, size_t *out_free_bytes)
{
    FATFS *fs;
    size_t free_clusters;
    int res = f_getfree("0:", &free_clusters, &fs);
    ESP_LOGI(FLASH_TAG, "RES: %d", res);
    assert(res == FR_OK);
    size_t total_sectors = (fs->n_fatent - 2) * fs->csize;
    size_t free_sectors = free_clusters * fs->csize;

    // assuming the total size is < 4GiB, should be true for SPI Flash
    if (out_total_bytes != NULL)
    {
        *out_total_bytes = total_sectors * fs->ssize;
    }
    if (out_free_bytes != NULL)
    {
        *out_free_bytes = free_sectors * fs->ssize;
    }
}

typedef struct {
    bool active;
    char file_name[32];
    uint8_t audio_id;
} load_audio_data_t;

FILE *fp = NULL;
static char file_path[64];
uint32_t payload_size = 0;
uint32_t bytes_remaining = 0;
load_audio_data_t audio_data_cache = {.active = false};

static void reset_nvm_data()
{
    if (fp) {
        fclose(fp);
        fp = NULL;
    }
    payload_size = 0;
    bytes_remaining = 0;
    audio_data_cache.active = false;
}
int load_audio_start(uint8_t audio_id, char *filename, uint16_t filename_len, uint32_t payload_len)
{
    if (audio_id >= AUDIO_NUM_SLOTS) {
        ESP_LOGE(FLASH_TAG, "Invalid audio id: %d, must be between 0 and %d", audio_id, AUDIO_NUM_SLOTS-1);
        return -1;
    }
    if (filename == NULL) {
        ESP_LOGE(FLASH_TAG, "Invalid filename, cannot be NULL");
        return -1;
    }

    if (fp) {
        ESP_LOGE(FLASH_TAG, "File already open, cannot open new one");
        return -1;
    }

    snprintf(file_path, sizeof(file_path), "%s/%s", base_path, filename);
    ESP_LOGI(FLASH_TAG, "Opening Audio NVM File for writing: %s", file_path);

    fp = fopen(file_path, "wb");
    if (fp == NULL)
    {
        ESP_LOGE(FLASH_TAG, "Failed to open file for writing");
        return -1;
    }

    payload_size = payload_len;
    bytes_remaining = payload_len;
    
    snprintf(audio_data_cache.file_name, sizeof(audio_data_cache.file_name), "%s", filename);
    audio_data_cache.audio_id = audio_id;
    audio_data_cache.active = true;

    ESP_LOGI(FLASH_TAG, "payload_size: %u, Bytes Remaining: %u", payload_size, bytes_remaining);
    return 0;  
}
int load_nvm_start(char *filename, uint16_t path_len, uint32_t payload_len)
{
    if (fp) {
        ESP_LOGE(FLASH_TAG, "File already open, cannot open new one");
        return -1;
    }
    snprintf(file_path, sizeof(file_path), "%s/%s.bin", base_path, "audio0");
    ESP_LOGI(FLASH_TAG, "Opening NVM File for writing: %s", file_path);

    fp = fopen(file_path, "wb");
    if (fp == NULL)
    {
        ESP_LOGE(FLASH_TAG, "Failed to open file for writing");
        return -1;
    }  

    payload_size = payload_len;
    bytes_remaining = payload_len;

    ESP_LOGI(FLASH_TAG, "payload_size: %u, Bytes Remaining: %u", payload_size, bytes_remaining);
    return 0;
}
int load_nvm_chunk(uint8_t *payload, uint32_t chunk_len)
{
    if (!fp) {
        return -1;
    }
    size_t N = fwrite(payload, 1, chunk_len, fp);
    bytes_remaining -= chunk_len;
    
    if (chunk_len != N) {
        ESP_LOGE(FLASH_TAG, "chunk_len != N");
        ESP_LOGE(FLASH_TAG, "Chunk_len: %u, N: %u, Bytes Remaining: %u", chunk_len, N, bytes_remaining);
    }
    return 0;
}
int load_nvm_end()
{
    ESP_LOGI(FLASH_TAG, "Closing NVM File");
    bool loading_audio = audio_data_cache.active;
    reset_nvm_data();

    uint8_t flash_data[256];
    fp = fopen(file_path, "rb");
    if (fp == NULL)
    {
        ESP_LOGE(FLASH_TAG, "Failed to open file for reading");
        return -1;
    }  
    ESP_LOGI(FLASH_TAG, "About to read %u bytes from flash", sizeof(flash_data));
    fread(flash_data, 1, sizeof(flash_data), fp);
    ESP_LOGI(FLASH_TAG, "Finished reading from flash");
    for (int i = 0; i < sizeof(flash_data); i += 8)
    {
        ESP_LOGI(FLASH_TAG, "%d (0x%X): 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", 
            i,  i, 
            flash_data[i], flash_data[i + 1], flash_data[i + 2], flash_data[i + 3], 
            flash_data[i + 4], flash_data[i + 5], flash_data[i + 6], flash_data[i + 7]);
    }

    if (loading_audio) {
        ESP_LOGI(FLASH_TAG, "Updating audio metadata");
        size_t N = sizeof(audio_meta_data[audio_data_cache.audio_id].file_name);
        snprintf(audio_meta_data[audio_data_cache.audio_id].file_name, N, "%s", audio_data_cache.file_name);
        audio_meta_data[audio_data_cache.audio_id].active = true;

        if (write_audio_metadata() < 0) {
            ESP_LOGE(FLASH_TAG, "Failed to write audio metadata");
        }
    }

    reset_nvm_data();
    return 0;
}
int nvm_erase_chip()
{
    if (flash == NULL) {
        return -1;
    }

    esp_err_t err = esp_flash_erase_chip(flash);
    if (err != ESP_OK)
    {
        ESP_ERROR_CHECK(err);
        return -1;
    }
    return 0;
}

int get_audio_metadata(uint8_t *buf, uint16_t *payload_size)
{
    if (!buf) {
        ESP_LOGE(FLASH_TAG, "Invalid buffer pointer");
        return -1;
    }
    if (!payload_size) {
        ESP_LOGE(FLASH_TAG, "Invalid size pointer");
        return -1;
    }
    memcpy(buf, audio_meta_data, sizeof(audio_meta_data));
    *payload_size = sizeof(audio_meta_data);
    return 0;
}