#include <stdio.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_SDA_GPIO GPIO_NUM_52
#define I2C_SCL_GPIO GPIO_NUM_53

static const char *TAG = "i2c_scan";

static int scan_bus(i2c_master_bus_handle_t bus)
{
    int devices = 0;

    printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
    for (int row = 0; row < 8; ++row) {
        printf("%02X: ", row << 4);
        for (int column = 0; column < 16; ++column) {
            const uint8_t address = (row << 4) | column;
            if (address < 0x08 || address > 0x77) {
                printf("   ");
                continue;
            }

            const esp_err_t result = i2c_master_probe(bus, address, 50);
            if (result == ESP_OK) {
                printf("%02X ", address);
                ++devices;
            } else if (result == ESP_ERR_TIMEOUT) {
                printf("UU ");
            } else {
                printf("-- ");
            }
        }
        printf("\n");
    }

    return devices;
}

void app_main(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));

    ESP_LOGI(TAG, "SDA=J2 pin 19 (GPIO52), SCL=J2 pin 21 (GPIO53)");
    ESP_LOGI(TAG, "Use 3.3 V devices and add external pull-ups when required");

    while (true) {
        const int count = scan_bus(bus);
        ESP_LOGI(TAG, "Found %d device(s)", count);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
