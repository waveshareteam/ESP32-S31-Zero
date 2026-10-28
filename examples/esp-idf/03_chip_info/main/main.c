#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

void app_main(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;
    uint8_t mac[6];

    esp_chip_info(&chip_info);
    ESP_ERROR_CHECK(esp_flash_get_physical_size(NULL, &flash_size));
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));

    printf("\nESP32-S31-Zero chip information\n");
    printf("Target: %s\n", CONFIG_IDF_TARGET);
    printf("CPU cores: %d\n", chip_info.cores);
    printf("Silicon revision: %d\n", chip_info.revision);
    printf("Physical Flash: %" PRIu32 " bytes (%" PRIu32 " MiB)\n",
           flash_size, flash_size / (1024 * 1024));
    printf("Base MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("Features:%s%s%s%s\n",
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? " Wi-Fi" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? " BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? " BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? " IEEE802.15.4" : "");
    printf("Configured flash size: %s\n\n", CONFIG_ESPTOOLPY_FLASHSIZE);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
