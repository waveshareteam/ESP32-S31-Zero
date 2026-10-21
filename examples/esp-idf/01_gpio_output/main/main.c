#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"

static const uint64_t OUTPUT_GPIO_MASK =
    (1ULL << GPIO_NUM_0)  | (1ULL << GPIO_NUM_1)  |
    (1ULL << GPIO_NUM_2)  | (1ULL << GPIO_NUM_3)  |
    (1ULL << GPIO_NUM_4)  | (1ULL << GPIO_NUM_5)  |
    (1ULL << GPIO_NUM_6)  | (1ULL << GPIO_NUM_46) |
    (1ULL << GPIO_NUM_47) | (1ULL << GPIO_NUM_48) |
    (1ULL << GPIO_NUM_49) | (1ULL << GPIO_NUM_50) |
    (1ULL << GPIO_NUM_51) | (1ULL << GPIO_NUM_58) |
    (1ULL << GPIO_NUM_59);

static const gpio_num_t s_output_gpios[] = {
    GPIO_NUM_0,
    GPIO_NUM_1,
    GPIO_NUM_2,
    GPIO_NUM_3,
    GPIO_NUM_4,
    GPIO_NUM_5,
    GPIO_NUM_6,
    GPIO_NUM_46,
    GPIO_NUM_47,
    GPIO_NUM_48,
    GPIO_NUM_49,
    GPIO_NUM_50,
    GPIO_NUM_51,
    GPIO_NUM_58,
    GPIO_NUM_59,
};

static const char *TAG = "gpio_output_high";

void app_main(void)
{
    const gpio_config_t output_config = {
        .pin_bit_mask = OUTPUT_GPIO_MASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&output_config));
    for (size_t i = 0; i < sizeof(s_output_gpios) / sizeof(s_output_gpios[0]); ++i) {
        ESP_ERROR_CHECK(gpio_set_drive_capability(s_output_gpios[i], GPIO_DRIVE_CAP_3));
        ESP_ERROR_CHECK(gpio_set_level(s_output_gpios[i], 1));
    }

    ESP_LOGI(TAG, "GPIO0-6, GPIO46-51, GPIO58 and GPIO59 are configured as outputs and set high");
}
