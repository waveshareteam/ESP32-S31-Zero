#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <driver/gpio.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TEST_UART UART_NUM_1
#define TEST_TXD  GPIO_NUM_58
#define TEST_RXD  GPIO_NUM_59

static const char *TAG = "uart_loopback";

void app_main(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(TEST_UART, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(TEST_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(TEST_UART, TEST_TXD, TEST_RXD,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "Connect H2 pin 2 (GPIO58 UART1 TX) to H2 pin 1 (GPIO59 UART1 RX)");

    uint32_t sequence = 0;
    char tx_buffer[64];
    uint8_t rx_buffer[64];

    while (true) {
        const int tx_length = snprintf(tx_buffer, sizeof(tx_buffer),
                                       "ESP32-S31 UART loopback %" PRIu32 "\r\n",
                                       sequence++);
        ESP_ERROR_CHECK(uart_flush_input(TEST_UART));
        const int written = uart_write_bytes(TEST_UART, tx_buffer, tx_length);
        ESP_ERROR_CHECK(uart_wait_tx_done(TEST_UART, pdMS_TO_TICKS(100)));

        const int received = uart_read_bytes(TEST_UART, rx_buffer, tx_length,
                                             pdMS_TO_TICKS(200));
        const bool match = written == tx_length && received == tx_length &&
                           memcmp(tx_buffer, rx_buffer, tx_length) == 0;
        if (match) {
            ESP_LOGI(TAG, "PASS: %d bytes echoed", received);
        } else {
            ESP_LOGW(TAG, "FAIL: wrote=%d read=%d; check the jumper", written, received);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
