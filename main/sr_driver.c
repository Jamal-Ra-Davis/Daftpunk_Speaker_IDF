#include "sr_driver.h"
#include "global_defines.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#define SR_SPI_BUS SPI3_HOST
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK 18
#define PIN_NUM_CS 5

#define SR_TAG "SR_DRIVER"

static spi_bus_config_t buscfg = {
    .miso_io_num = -1,
    .mosi_io_num = PIN_NUM_MOSI,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 32,
};

static spi_device_interface_config_t dev_config = {
    .address_bits = 0,
    .command_bits = 0,
    .dummy_bits = 0,
    .clock_speed_hz = 16000000,
    .spics_io_num = PIN_NUM_CS,
    .queue_size = 4,
};

static spi_device_handle_t handle;

void init_shift_registers()
{
  static const uint8_t CLEAR_DATA[SR_CNT] = {
      0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_err_t ret;

  ESP_LOGI(SR_TAG, "Initializing bus SPI%d...", SR_SPI_BUS + 1);

  // Initialize the SPI bus
  ret = spi_bus_initialize(SR_SPI_BUS, &buscfg, SPI_DMA_CH_AUTO);
  ESP_ERROR_CHECK(ret);

  ret = spi_bus_add_device(SR_SPI_BUS, &dev_config, &handle);
  ESP_ERROR_CHECK(ret);

  spi_transaction_t spi_transaction = {
      .length = 6 * 8,
      .tx_buffer = (void *)CLEAR_DATA,
  };

  ret = spi_device_transmit(handle, &spi_transaction);
  ESP_ERROR_CHECK(ret);
}

void sr_write(uint8_t *data, int N)
{
  spi_transaction_t spi_transaction = {
      .length = N * 8,
      .tx_buffer = (void *)data,
  };
  spi_device_transmit(handle, &spi_transaction);
}

void sr_write_byte(uint8_t val)
{
  sr_write(&val, 1);
}
