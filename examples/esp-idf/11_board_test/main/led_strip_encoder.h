#pragma once

#include <stdint.h>

#include "driver/rmt_encoder.h"

typedef struct {
    uint32_t resolution_hz;
} led_strip_encoder_config_t;

esp_err_t led_strip_new_encoder(const led_strip_encoder_config_t *config,
                                rmt_encoder_handle_t *ret_encoder);
