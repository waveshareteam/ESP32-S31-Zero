#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOOT_BUTTON_GPIO GPIO_NUM_61

static const char *TAG = "boot_button";

void app_main(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOOT_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));

    ESP_LOGI(TAG, "BOOT button is GPIO61 and active low");
    ESP_LOGW(TAG, "Holding BOOT while resetting selects download mode");

    int stable_level = gpio_get_level(BOOT_BUTTON_GPIO);
    while (true) {
        const int first = gpio_get_level(BOOT_BUTTON_GPIO);
        vTaskDelay(pdMS_TO_TICKS(20));
        const int second = gpio_get_level(BOOT_BUTTON_GPIO);

        if (first == second && second != stable_level) {
            stable_level = second;
            ESP_LOGI(TAG, "Button %s", stable_level == 0 ? "PRESSED" : "RELEASED");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
