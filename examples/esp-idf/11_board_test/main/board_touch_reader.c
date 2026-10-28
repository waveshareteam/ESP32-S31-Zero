#include "board_touch_reader.h"

#include <string.h>

#define GT911_STATUS 0x814E
#define GT911_POINTS 0x814F
#define GT911_MAX_POINTS 5
#define POINT_BYTES 8
#define JUMP_LIMIT 120
#define JUMP_CONFIRM_DISTANCE 48

bool board_touch_sample_pressed(const board_touch_sample_t *sample, uint32_t now_ms)
{
    return sample->pressed && (uint32_t)(now_ms - sample->updated_ms) < BOARD_TOUCH_STALE_MS;
}

void board_touch_reader_init(board_touch_reader_t *reader, board_touch_io_t io,
                             uint16_t width, uint16_t height)
{
    memset(reader, 0, sizeof(*reader));
    reader->io = io;
    reader->width = width;
    reader->height = height;
}

static unsigned distance(uint16_t a, uint16_t b)
{
    return a > b ? a - b : b - a;
}

static esp_err_t accept_frame(board_touch_reader_t *reader, const uint8_t *data,
                              uint8_t count, uint32_t now)
{
    board_touch_sample_t *sample = &reader->sample;
    uint16_t ids = 0;
    int selected = -1;
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t *point = data + i * POINT_BYTES;
        const uint8_t id = point[0];
        const uint16_t x = point[1] | ((uint16_t)point[2] << 8);
        const uint16_t y = point[3] | ((uint16_t)point[4] << 8);
        if (id >= GT911_MAX_POINTS || (ids & (1U << id)) ||
            x >= reader->width || y >= reader->height) {
            ++sample->rejected;
            return ESP_ERR_INVALID_RESPONSE;
        }
        ids |= 1U << id;
        if ((reader->tracking && id == reader->track_id) ||
            (!reader->tracking && selected < 0)) {
            selected = (int)i;
        }
    }
    if (count == 0 || selected < 0) {
        /* Release before selecting another finger; never jump to index zero
         * merely because the controller reordered its multi-touch records.
         */
        sample->pressed = false;
        sample->updated_ms = now;
        reader->tracking = false;
        reader->jump_pending = false;
        return ESP_OK;
    }
    const uint8_t *point = data + selected * POINT_BYTES;
    const uint16_t x = point[1] | ((uint16_t)point[2] << 8);
    const uint16_t y = point[3] | ((uint16_t)point[4] << 8);
    if (reader->tracking &&
        (distance(x, sample->x) > JUMP_LIMIT || distance(y, sample->y) > JUMP_LIMIT)) {
        if (!reader->jump_pending || distance(x, reader->jump_x) > JUMP_CONFIRM_DISTANCE ||
            distance(y, reader->jump_y) > JUMP_CONFIRM_DISTANCE) {
            reader->jump_pending = true;
            reader->jump_x = x;
            reader->jump_y = y;
            ++sample->rejected;
            return ESP_OK;
        }
    }
    reader->jump_pending = false;
    reader->tracking = true;
    reader->track_id = point[0];
    sample->x = x;
    sample->y = y;
    sample->pressed = true;
    sample->updated_ms = now;
    return ESP_OK;
}

esp_err_t board_touch_reader_poll(board_touch_reader_t *reader)
{
    board_touch_sample_t *sample = &reader->sample;
    const uint32_t started = reader->io.now_ms(reader->io.ctx);
    if (!board_touch_sample_pressed(sample, started)) {
        sample->pressed = false;
        reader->tracking = false;
        reader->jump_pending = false;
    }
    ++sample->reads;
    bool data_stage = false;
    uint8_t status = 0;
    esp_err_t err = reader->io.read(reader->io.ctx, GT911_STATUS, &status, 1);
    if (err != ESP_OK || !(status & 0x80)) {
        /* Not-ready is not a release and must not acknowledge the register:
         * doing so can erase a frame that becomes ready between read/write.
         */
        goto done;
    }
    const uint8_t count = status & 0x0F;
    uint8_t points[GT911_MAX_POINTS * POINT_BYTES] = {0};
    if (count > 0 && count <= GT911_MAX_POINTS) {
        err = reader->io.read(reader->io.ctx, GT911_POINTS, points, count * POINT_BYTES);
        if (err != ESP_OK) {
            goto done;  /* Keep the ready frame available for a later retry. */
        }
    }
    const uint8_t clear = 0;
    err = reader->io.write(reader->io.ctx, GT911_STATUS, &clear, 1);
    if (err != ESP_OK) {
        goto done;  /* Never publish a partially completed transaction. */
    }
    data_stage = true;
    if (count > GT911_MAX_POINTS) {
        ++sample->rejected;
        err = ESP_ERR_INVALID_RESPONSE;
    } else {
        ++sample->frames;
        err = accept_frame(reader, points, count, reader->io.now_ms(reader->io.ctx));
    }
done:
    sample->last_ms = reader->io.now_ms(reader->io.ctx) - started;
    if (sample->last_ms > sample->max_ms) {
        sample->max_ms = sample->last_ms;
    }
    if (err != ESP_OK) {
        ++sample->errors;
        sample->last_error = err;
        sample->error_ms = sample->last_ms;
        sample->error_data_stage = data_stage;
    }
    return err;
}
