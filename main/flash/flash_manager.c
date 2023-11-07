#include <string.h>
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "flash_manager.h"
#include "bt_app_core.h"
#include "bt_audio.h"

#define FLASH_TAG "FLASH_MANAGER"

extern void bt_i2s_driver_install(void);

static esp_flash_t *example_init_ext_flash(void);

#define AUDIO_BYTE_LEN 664112 //36544
static const uint8_t magic_seq[8] = {0xfa, 0xff, 0x00, 0x00, 0xf4, 0xff, 0x00, 0x00};
static uint8_t rbuf[4096] __attribute__ ((aligned (4)));
static esp_flash_t *flash = NULL;
static bool flash_loaded = false;
void flash_init()
{
    flash = example_init_ext_flash();
    if (flash == NULL)
    {
        return;
    }

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
    flash_check = true;
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
    bt_i2s_driver_install();

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