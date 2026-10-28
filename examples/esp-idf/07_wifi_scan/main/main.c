#include <inttypes.h>
#include <stdlib.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define MAX_SCAN_RESULTS 20

static const char *TAG = "wifi_scan";

static void init_nvs(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
}

void app_main(void)
{
    init_nvs();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Attach a 2.4 GHz antenna to ANT1 before scanning");

    while (true) {
        ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));

        uint16_t total = 0;
        uint16_t count = MAX_SCAN_RESULTS;
        wifi_ap_record_t records[MAX_SCAN_RESULTS] = {0};
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&total));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&count, records));

        ESP_LOGI(TAG, "Found %u AP(s); showing %u", total, count);
        for (uint16_t i = 0; i < count; ++i) {
            ESP_LOGI(TAG, "%2u: ch=%2u rssi=%4d auth=%d ssid=%s",
                     i + 1, records[i].primary, records[i].rssi,
                     records[i].authmode, (const char *)records[i].ssid);
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
