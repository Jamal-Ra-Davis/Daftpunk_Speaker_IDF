#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

// SCL: 19, SDA: 21

#define SPI_TAG "SPI_TEST"

void app_main(void)
{
    int cnt = 0;

    while (1) {
        printf("Hello world %d!\n", ++cnt);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
