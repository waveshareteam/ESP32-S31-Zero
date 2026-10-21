#pragma once

#include "board_peripherals.h"
#include "lvgl.h"
#include "board_touch_reader.h"

/* The LVGL adapter must be started and locked by the caller. */
void board_ui_create(const board_status_t *status, lv_indev_t *touch_indev);
void board_ui_update(const board_status_t *status);

/* Called from the LVGL read callback under the adapter lock. */
void board_ui_set_touch_diagnostics(const board_touch_sample_t *sample);
