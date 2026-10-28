#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    BOARD_TEST_RGB,
    BOARD_TEST_CHIP,
    BOARD_TEST_FLASH,
    BOARD_TEST_FLASH_RW,
    BOARD_TEST_FLASH_SPEED,
    BOARD_TEST_PSRAM,
    BOARD_TEST_TEMPERATURE,
    BOARD_TEST_WIFI,
    BOARD_TEST_USB,
    BOARD_TEST_CPU_RAM,
    BOARD_TEST_LCD,
    BOARD_TEST_TOUCH,
    BOARD_TEST_SD,
    BOARD_TEST_LED,
    BOARD_TEST_COUNT,
} board_test_id_t;

#define BOARD_TEST_MASK(id) (UINT32_C(1) << (id))
#define BOARD_CPU_RAM_TEST_BYTES (64U * 1024U)
#define BOARD_CPU_RAM_BLOCK_BYTES (16U * 1024U)

typedef enum {
    BOARD_CPU_RAM_NOT_RUN,
    BOARD_CPU_RAM_OK,
    BOARD_CPU_RAM_NO_MEMORY,
    BOARD_CPU_RAM_MISMATCH,
    BOARD_CPU_RAM_NO_ROUNDS,
} board_cpu_ram_outcome_t;

typedef struct {
    board_cpu_ram_outcome_t outcome;
    uint32_t free_bytes;
    uint32_t largest_block;
    uint32_t allocated_bytes;
    uint32_t rounds;
    uint32_t mismatch_offset;
    uint32_t expected;
    uint32_t actual;
} board_cpu_ram_result_t;

typedef struct {
    bool rgb_indicator_ok;
    bool chip_ok;
    bool flash_ok;
    uint32_t flash_size_bytes;
    bool flash_rw_ok;
    bool flash_speed_ok;
    bool psram_ok;
    bool temperature_ok;
    bool wifi_ok;
    bool usb_ok;
    bool cpu_ram_ok;
    board_cpu_ram_result_t cpu_ram_result;
    bool lcd_ok;
    bool touch_ok;
    uint8_t touch_address;
    bool sd_ok;
    esp_err_t sd_error;
    bool led_ok;
    uint32_t completed_mask;
} board_status_t;

/* Individual peripheral failures are reported but never abort the others. */
esp_err_t board_peripherals_start(board_status_t *status);

/* Thread-safe refresh; returns ESP_ERR_INVALID_STATE until the UI is ready. */
esp_err_t board_peripherals_refresh_ui(const board_status_t *status);
