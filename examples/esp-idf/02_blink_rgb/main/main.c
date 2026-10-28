#include <stdint.h>

#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip_encoder.h"

#define RGB_GPIO      60
#define RMT_RES_HZ    10000000

static const char *TAG = "blink_rgb";

static void send_color(rmt_channel_handle_t channel,
                       rmt_encoder_handle_t encoder,
                       uint8_t red, uint8_t green, uint8_t blue)
{
    const uint8_t grb[] = {green, red, blue};
    const rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    ESP_ERROR_CHECK(rmt_transmit(channel, encoder, grb, sizeof(grb), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(channel, portMAX_DELAY));
}

void app_main(void)
{
    rmt_channel_handle_t channel = NULL;
    rmt_encoder_handle_t encoder = NULL;

    const rmt_tx_channel_config_t channel_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = RGB_GPIO,
        .mem_block_symbols = 48,
        .resolution_hz = RMT_RES_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&channel_config, &channel));

    const led_strip_encoder_config_t encoder_config = {
        .resolution_hz = RMT_RES_HZ,
    };
    ESP_ERROR_CHECK(led_strip_new_encoder(&encoder_config, &encoder));
    ESP_ERROR_CHECK(rmt_enable(channel));

    ESP_LOGI(TAG, "Driving onboard WS2812B-1010 through GPIO60 level shifter");

    while (true) {
        send_color(channel, encoder, 32, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        send_color(channel, encoder, 0, 32, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        send_color(channel, encoder, 0, 0, 32);
        vTaskDelay(pdMS_TO_TICKS(1000));
        send_color(channel, encoder, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
