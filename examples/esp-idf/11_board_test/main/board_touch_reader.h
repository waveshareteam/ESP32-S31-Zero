#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define BOARD_TOUCH_STALE_MS 150U

typedef struct {
    uint16_t x, y;
    bool pressed;
    uint32_t updated_ms;
    uint32_t reads, frames, errors, rejected;
    uint32_t last_ms, max_ms, error_ms;
    esp_err_t last_error;
    bool error_data_stage;
} board_touch_sample_t;

typedef struct {
    void *ctx;
    esp_err_t (*read)(void *ctx, uint16_t reg, void *data, size_t size);
    esp_err_t (*write)(void *ctx, uint16_t reg, const void *data, size_t size);
    uint32_t (*now_ms)(void *ctx);
} board_touch_io_t;

/* Owned by one sampling task. Publish only sample to other tasks. */
typedef struct {
    board_touch_io_t io;
    board_touch_sample_t sample;
    uint16_t width, height;
    uint8_t track_id;
    bool tracking, jump_pending;
    uint16_t jump_x, jump_y;
} board_touch_reader_t;

void board_touch_reader_init(board_touch_reader_t *reader, board_touch_io_t io,
                             uint16_t width, uint16_t height);
esp_err_t board_touch_reader_poll(board_touch_reader_t *reader);
bool board_touch_sample_pressed(const board_touch_sample_t *sample, uint32_t now_ms);
