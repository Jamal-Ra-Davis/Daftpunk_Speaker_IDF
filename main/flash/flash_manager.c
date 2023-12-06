#include <string.h>
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "flash_manager.h"
#include "bt_audio.h"
#include "i2s_task.h"

#define FLASH_TAG "FLASH_MANAGER"

/*******************************
 * Data Type Definitions
 ******************************/

/*******************************
 * Global Data
 ******************************/
static esp_flash_t *example_init_ext_flash(void);
static esp_flash_t *flash = NULL;
// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

/*******************************
 * Function Prototypes
 ******************************/
static const esp_partition_t *example_add_partition(esp_flash_t *ext_flash, const char *partition_label);
static void example_list_data_partitions(void);
static bool example_mount_fatfs(const char *partition_label);
static void example_get_fatfs_usage(size_t *out_total_bytes, size_t *out_free_bytes);

/*******************************
 * Private Function Definitions
 ******************************/
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
    esp_err_t err = esp_vfs_fat_spiflash_mount(FLASH_BASE_PATH, partition_label, &mount_config, &s_wl_handle);
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

/*******************************
 * Public Function Definitions
 ******************************/
int flash_init()
{
    flash = example_init_ext_flash();
    if (flash == NULL)
    {
        return -1;
    }

    // Add the entire external flash chip as a partition
    const char *partition_label = "storage";
    example_add_partition(flash, partition_label);

    // List the available partitions
    example_list_data_partitions();

    // Initialize FAT FS in the partition
    if (!example_mount_fatfs(partition_label))
    {
        return -1;
    }

    // Print FAT FS size information
    size_t bytes_total, bytes_free;
    example_get_fatfs_usage(&bytes_total, &bytes_free);
    ESP_LOGI(FLASH_TAG, "FAT FS: %d kB total, %d kB free", bytes_total / 1024, bytes_free / 1024);
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