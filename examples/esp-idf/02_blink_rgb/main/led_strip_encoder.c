#include <stdlib.h>

#include "esp_check.h"
#include "led_strip_encoder.h"

static const char *TAG = "ws2812_encoder";

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_symbol;
} ws2812_encoder_t;

RMT_ENCODER_FUNC_ATTR
static size_t encode_ws2812(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                            const void *data, size_t data_size,
                            rmt_encode_state_t *ret_state)
{
    ws2812_encoder_t *ws2812 = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    if (ws2812->state == 0) {
        encoded_symbols += ws2812->bytes_encoder->encode(
            ws2812->bytes_encoder, channel, data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws2812->state = 1;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            *ret_state = state;
            return encoded_symbols;
        }
    }

    if (ws2812->state == 1) {
        encoded_symbols += ws2812->copy_encoder->encode(
            ws2812->copy_encoder, channel, &ws2812->reset_symbol,
            sizeof(ws2812->reset_symbol), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws2812->state = 0;
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
        }
    }

    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t delete_ws2812_encoder(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws2812 = __containerof(encoder, ws2812_encoder_t, base);
    rmt_del_encoder(ws2812->bytes_encoder);
    rmt_del_encoder(ws2812->copy_encoder);
    free(ws2812);
    return ESP_OK;
}

RMT_ENCODER_FUNC_ATTR
static esp_err_t reset_ws2812_encoder(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws2812 = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_reset(ws2812->bytes_encoder);
    rmt_encoder_reset(ws2812->copy_encoder);
    ws2812->state = 0;
    return ESP_OK;
}

esp_err_t led_strip_new_encoder(const led_strip_encoder_config_t *config,
                                rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    ws2812_encoder_t *ws2812 = NULL;

    ESP_GOTO_ON_FALSE(config != NULL && ret_encoder != NULL,
                      ESP_ERR_INVALID_ARG, fail, TAG, "invalid argument");
    ws2812 = rmt_alloc_encoder_mem(sizeof(ws2812_encoder_t));
    ESP_GOTO_ON_FALSE(ws2812 != NULL, ESP_ERR_NO_MEM, fail, TAG, "no memory");

    ws2812->base.encode = encode_ws2812;
    ws2812->base.del = delete_ws2812_encoder;
    ws2812->base.reset = reset_ws2812_encoder;

    const rmt_bytes_encoder_config_t bytes_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * config->resolution_hz / 1000000,
            .level1 = 0,
            .duration1 = 0.9 * config->resolution_hz / 1000000,
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.9 * config->resolution_hz / 1000000,
            .level1 = 0,
            .duration1 = 0.3 * config->resolution_hz / 1000000,
        },
        .flags.msb_first = 1,
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_config, &ws2812->bytes_encoder),
                      fail, TAG, "create bytes encoder failed");

    const rmt_copy_encoder_config_t copy_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_config, &ws2812->copy_encoder),
                      fail, TAG, "create copy encoder failed");

    const uint32_t reset_ticks = config->resolution_hz / 1000000 * 50 / 2;
    ws2812->reset_symbol = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = reset_ticks,
    };

    *ret_encoder = &ws2812->base;
    return ESP_OK;

fail:
    if (ws2812 != NULL) {
        if (ws2812->bytes_encoder != NULL) {
            rmt_del_encoder(ws2812->bytes_encoder);
        }
        if (ws2812->copy_encoder != NULL) {
            rmt_del_encoder(ws2812->copy_encoder);
        }
        free(ws2812);
    }
    return ret;
}
