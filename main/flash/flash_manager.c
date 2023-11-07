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

void flash_init()
{
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
    
    // Open file for reading
    ESP_LOGI(FLASH_TAG, "Reading file");
    FILE *f = fopen("/extflash/hello.txt", "rb");
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

    esp_err_t err;
    int bytes_left = AUDIO_BYTE_LEN;
    uint32_t flash_address = 0;
    int cnt = 0;
    while (bytes_left > 0) {
        int len = 4096;
        if (bytes_left < 4096) {
            len = bytes_left;
        }

        err = esp_flash_read(flash, (void *)rbuf, flash_address, sizeof(rbuf));
        if (err != ESP_OK)
        {
            ESP_ERROR_CHECK(err);
            continue;
        }

        //ESP_LOGI(FLASH_TAG, "%d (0x%X): 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", flash_address,  flash_address, rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);

        for (int i = 0; i < len; i += 4)
        {
            int16_t *left = (int16_t *)(&rbuf[i]);
            int16_t *right = (int16_t *)(&rbuf[i + 2]);

            *left = (int16_t)(*left * vol_scale);
            *right = (int16_t)(*right * vol_scale);
        }

        /*
        size_t bytes_written = write_ringbuf(rbuf, len);
        if (bytes_written != len) {
            ESP_LOGE(FLASH_TAG, "Failed to write to ringbuf");
        }
        */
    
        while (1) {
            size_t bytes_written = write_ringbuf(rbuf, len);
            if (bytes_written == len) {
                break;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        bytes_left -= len;
        flash_address += len;
        /*
        if (++cnt % 6 == 0) {
            vTaskDelay(120 / portTICK_PERIOD_MS);
        }
        */
    }
    memset(rbuf, 0, sizeof(rbuf));

    for (int i=0; i<3; i++) {
        write_ringbuf(rbuf, sizeof(rbuf));
        //vTaskDelay(50 / portTICK_PERIOD_MS);
    }
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

FILE *fp = NULL;
static char file_path[64];
static char file_truc[32];
uint32_t payload_size = 0;
uint32_t bytes_remaining = 0;
static void reset_nvm_data()
{
    if (fp) {
        fclose(fp);
        fp = NULL;
    }
    payload_size = 0;
    bytes_remaining = 0;
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
    reset_nvm_data();
    return 0;
}