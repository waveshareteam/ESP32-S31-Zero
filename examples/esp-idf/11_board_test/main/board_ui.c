#include "board_ui.h"

#include <inttypes.h>
#include <stdio.h>

#include "lvgl.h"

typedef struct {
    lv_obj_t *box;
    lv_obj_t *state;
    lv_obj_t *info;
} status_tile_t;

static lv_obj_t *s_summary_label;
static status_tile_t s_tiles[BOARD_TEST_COUNT];

#define TOUCH_MARKER_SIZE 24
#define TOUCH_MARKER_HOLD_MS 2000

static lv_obj_t *s_touch_marker;
static lv_timer_t *s_touch_hide_timer;

static lv_obj_t *s_diagnostics_label;
static bool s_touch_tracking;
static board_touch_sample_t s_touch_diagnostics;
static char s_diagnostics_text[320];

void board_ui_set_touch_diagnostics(const board_touch_sample_t *sample)
{
    s_touch_diagnostics = *sample;
}

static void update_diagnostics(lv_timer_t *timer)
{
    (void)timer;
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    if (!s_touch_tracking) {
        snprintf(s_diagnostics_text, sizeof(s_diagnostics_text),
                 "UI %" PRIu32 " s\n"
                 "LVGL free %zu B | Largest %zu B | Peak %zu B",
                 lv_tick_get() / 1000U, memory.free_size,
                 memory.free_biggest_size, memory.max_used);
        lv_label_set_text_static(s_diagnostics_label, s_diagnostics_text);
        return;
    }
    const board_touch_sample_t *touch = &s_touch_diagnostics;
    snprintf(s_diagnostics_text, sizeof(s_diagnostics_text),
             "UI %" PRIu32 " s | Reads %" PRIu32 " | Errors %" PRIu32 " | Last 0x%X\n"
             "Frames %" PRIu32 " | Drop %" PRIu32 " | Read %" PRIu32
             " ms | Max %" PRIu32 " ms | Error %" PRIu32 " ms (%s)\n"
             "LVGL free %zu B | Largest %zu B | Peak %zu B",
             lv_tick_get() / 1000U, touch->reads, touch->errors,
             (unsigned)touch->last_error, touch->frames, touch->rejected,
             touch->last_ms, touch->max_ms, touch->error_ms,
             touch->errors == 0 ? "none" : (touch->error_data_stage ? "DATA" : "IO"),
             memory.free_size, memory.free_biggest_size, memory.max_used);
    /* Reuse storage: diagnostics must not allocate strings every second. */
    lv_label_set_text_static(s_diagnostics_label, s_diagnostics_text);
}

static void hide_touch_marker(lv_timer_t *timer)
{
    lv_obj_add_flag(s_touch_marker, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer);
}

static void touch_marker_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) {
        lv_timer_pause(s_touch_hide_timer);
        lv_obj_remove_flag(s_touch_marker, LV_OBJ_FLAG_HIDDEN);
    } else if (lv_event_get_code(event) == LV_EVENT_RELEASED) {
        lv_timer_reset(s_touch_hide_timer);
        lv_timer_resume(s_touch_hide_timer);
    }
}

static void create_touch_marker(lv_indev_t *indev)
{
    if (indev == NULL) {
        return;
    }
    s_touch_marker = lv_obj_create(lv_display_get_layer_top(lv_indev_get_display(indev)));
    lv_obj_remove_style_all(s_touch_marker);
    lv_obj_set_size(s_touch_marker, TOUCH_MARKER_SIZE, TOUCH_MARKER_SIZE);
    lv_obj_set_style_radius(s_touch_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_touch_marker, lv_color_hex(0xF97316), 0);
    lv_obj_set_style_bg_opa(s_touch_marker, LV_OPA_50, 0);
    lv_obj_set_style_border_color(s_touch_marker, lv_color_hex(0xEA580C), 0);
    lv_obj_set_style_border_width(s_touch_marker, 2, 0);
    lv_obj_remove_flag(s_touch_marker, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_touch_marker, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_IGNORE_LAYOUT);
    s_touch_hide_timer = lv_timer_create(hide_touch_marker, TOUCH_MARKER_HOLD_MS, NULL);
    if (s_touch_hide_timer == NULL) {
        lv_obj_delete(s_touch_marker);
        s_touch_marker = NULL;
        return;
    }
    lv_timer_pause(s_touch_hide_timer);
    /* The native cursor moves during input processing, with no second polling
     * timer. Translate its top-left hotspot so the circle centers on the touch.
     */
    lv_indev_set_cursor(indev, s_touch_marker);
    lv_obj_set_style_translate_x(s_touch_marker, -TOUCH_MARKER_SIZE / 2, 0);
    lv_obj_set_style_translate_y(s_touch_marker, -TOUCH_MARKER_SIZE / 2, 0);
    lv_indev_add_event_cb(indev, touch_marker_event, LV_EVENT_PRESSED, NULL);
    lv_indev_add_event_cb(indev, touch_marker_event, LV_EVENT_RELEASED, NULL);
}

static bool test_passed(const board_status_t *status, board_test_id_t id)
{
    switch (id) {
    case BOARD_TEST_RGB: return status->rgb_indicator_ok;
    case BOARD_TEST_CHIP: return status->chip_ok;
    case BOARD_TEST_FLASH: return status->flash_ok;
    case BOARD_TEST_FLASH_RW: return status->flash_rw_ok;
    case BOARD_TEST_FLASH_SPEED: return status->flash_speed_ok;
    case BOARD_TEST_PSRAM: return status->psram_ok;
    case BOARD_TEST_TEMPERATURE: return status->temperature_ok;
    case BOARD_TEST_WIFI: return status->wifi_ok;
    case BOARD_TEST_USB: return status->usb_ok;
    case BOARD_TEST_CPU_RAM: return status->cpu_ram_ok;
    case BOARD_TEST_LCD: return status->lcd_ok;
    case BOARD_TEST_TOUCH: return status->touch_ok;
    case BOARD_TEST_SD: return status->sd_ok;
    case BOARD_TEST_LED: return status->led_ok;
    default: return false;
    }
}

static unsigned count_completed(const board_status_t *status)
{
    unsigned completed = 0;
    for (unsigned i = 0; i < BOARD_TEST_COUNT; ++i) {
        completed += (status->completed_mask & BOARD_TEST_MASK(i)) != 0U;
    }
    return completed;
}

static unsigned count_passed(const board_status_t *status)
{
    unsigned passed = 0;
    for (unsigned i = 0; i < BOARD_TEST_COUNT; ++i) {
        if ((status->completed_mask & BOARD_TEST_MASK(i)) != 0U) {
            passed += test_passed(status, (board_test_id_t)i) ? 1U : 0U;
        }
    }
    return passed;
}

static void make_status(lv_obj_t *parent, board_test_id_t id,
                        const char *name, const char *detail)
{
    status_tile_t *tile = &s_tiles[id];
    tile->box = lv_obj_create(parent);
    /* Status cards are fixed; dragging must not scroll their labels. */
    lv_obj_clear_flag(tile->box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(tile->box, 182, 68);
    lv_obj_set_style_radius(tile->box, 4, 0);
    lv_obj_set_style_border_width(tile->box, 1, 0);
    lv_obj_set_style_border_color(tile->box, lv_color_hex(0xCBD5E1), 0);
    lv_obj_set_style_bg_color(tile->box, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_pad_all(tile->box, 8, 0);

    lv_obj_t *title = lv_label_create(tile->box);
    lv_label_set_text(title, name);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, 110);
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    tile->state = lv_label_create(tile->box);
    lv_label_set_text(tile->state, "WAIT");
    lv_obj_set_style_text_color(tile->state, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(tile->state, LV_FONT_DEFAULT, 0);
    lv_obj_align(tile->state, LV_ALIGN_TOP_RIGHT, 0, 0);

    tile->info = lv_label_create(tile->box);
    lv_label_set_text(tile->info, detail);
    lv_label_set_long_mode(tile->info, LV_LABEL_LONG_DOT);
    lv_obj_set_width(tile->info, 164);
    lv_obj_set_style_text_color(tile->info, lv_color_hex(0x475569), 0);
    lv_obj_align(tile->info, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

static void update_tile(board_test_id_t id, const board_status_t *status,
                        const char *detail)
{
    status_tile_t *tile = &s_tiles[id];
    const bool completed =
        (status->completed_mask & BOARD_TEST_MASK(id)) != 0U;
    const bool passed = completed && test_passed(status, id);
    const uint32_t border = !completed ? 0xCBD5E1 :
                            (passed ? 0x86C89A : 0xE5A3A3);
    const uint32_t background = !completed ? 0xF8FAFC :
                                (passed ? 0xF0FDF4 : 0xFFF1F2);
    const uint32_t text = !completed ? 0x64748B :
                          (passed ? 0x16803C : 0xB42318);

    lv_obj_set_style_border_color(tile->box, lv_color_hex(border), 0);
    lv_obj_set_style_bg_color(tile->box, lv_color_hex(background), 0);
    lv_label_set_text(tile->state, !completed ? "WAIT" :
                      (passed ? "PASS" : "FAIL"));
    lv_obj_set_style_text_color(tile->state, lv_color_hex(text), 0);
    lv_label_set_text(tile->info, detail);
}

void board_ui_create(const board_status_t *status, lv_indev_t *touch_indev)
{
    s_touch_tracking = touch_indev != NULL;
    lv_obj_t *screen = lv_screen_active();
    /* Keep the dashboard fixed when touch gestures reach the screen. */
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF1F5F9), 0);
    lv_obj_set_style_pad_all(screen, 16, 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "ESP32-S31-Zero Board Test");
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x0F172A), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_summary_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_summary_label, lv_color_hex(0x334155), 0);
    lv_obj_align(s_summary_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *grid = lv_obj_create(screen);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 752, 296);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 5);

    make_status(grid, BOARD_TEST_RGB, "RGB Indicator", "GPIO60");
    make_status(grid, BOARD_TEST_CHIP, "Chip / Clock", "ESP32-S31");
    make_status(grid, BOARD_TEST_FLASH, "Flash", "Physical size pending");
    make_status(grid, BOARD_TEST_FLASH_RW, "Flash R/W", "4 KiB test");
    make_status(grid, BOARD_TEST_FLASH_SPEED, "Flash Speed", "Read benchmark");
    make_status(grid, BOARD_TEST_PSRAM, "PSRAM", "Read / write");
    make_status(grid, BOARD_TEST_TEMPERATURE, "Temperature", "Die sensor");
    make_status(grid, BOARD_TEST_WIFI, "WiFi 2.4G", "AP scan");
    make_status(grid, BOARD_TEST_USB, "USB JTAG", "GPIO33 / 34");
    make_status(grid, BOARD_TEST_CPU_RAM, "CPU / RAM", "2 s stress");
    make_status(grid, BOARD_TEST_LCD, "LCD", "800 x 480 RGB");
    make_status(grid, BOARD_TEST_TOUCH, "Touch", "I2C probe pending");
    make_status(grid, BOARD_TEST_SD, "SDMMC", "4-bit test pending");
    make_status(grid, BOARD_TEST_LED, "Status LED", "GPIO54 steady ON");

    s_diagnostics_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_diagnostics_label, lv_color_hex(0x64748B), 0);
    lv_obj_align(s_diagnostics_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    update_diagnostics(NULL);
    lv_timer_create(update_diagnostics, 1000, NULL);

    create_touch_marker(touch_indev);
    board_ui_update(status);
}

void board_ui_update(const board_status_t *status)
{
    if (status == NULL || s_summary_label == NULL) {
        return;
    }

    char summary[40];
    snprintf(summary, sizeof(summary), "%u / %u DONE | %u PASS",
             count_completed(status), (unsigned)BOARD_TEST_COUNT,
             count_passed(status));
    lv_label_set_text(s_summary_label, summary);

    update_tile(BOARD_TEST_RGB, status, "GPIO60");
    update_tile(BOARD_TEST_CHIP, status, "ESP32-S31");
    char flash_detail[32];
    if ((status->completed_mask & BOARD_TEST_MASK(BOARD_TEST_FLASH)) == 0U) {
        snprintf(flash_detail, sizeof(flash_detail), "Physical size pending");
    } else if (status->flash_size_bytes > 0) {
        snprintf(flash_detail, sizeof(flash_detail), "%" PRIu32 " MiB physical",
                 status->flash_size_bytes / (1024U * 1024U));
    } else {
        snprintf(flash_detail, sizeof(flash_detail), "Unavailable");
    }
    update_tile(BOARD_TEST_FLASH, status, flash_detail);
    update_tile(BOARD_TEST_FLASH_RW, status, "4 KiB test");
    update_tile(BOARD_TEST_FLASH_SPEED, status, "Read benchmark");
    update_tile(BOARD_TEST_PSRAM, status, "Read / write");
    update_tile(BOARD_TEST_TEMPERATURE, status, "Die sensor");
    update_tile(BOARD_TEST_WIFI, status, "AP scan");
    update_tile(BOARD_TEST_USB, status, "GPIO33 / 34");
    char cpu_detail[40];
    const board_cpu_ram_result_t *cpu = &status->cpu_ram_result;
    switch (cpu->outcome) {
    case BOARD_CPU_RAM_NO_MEMORY:
        snprintf(cpu_detail, sizeof(cpu_detail), "ALLOC %" PRIu32 "K/64K",
                 cpu->allocated_bytes / 1024U);
        break;
    case BOARD_CPU_RAM_MISMATCH:
        snprintf(cpu_detail, sizeof(cpu_detail), "R/W mismatch @%" PRIx32,
                 cpu->mismatch_offset);
        break;
    case BOARD_CPU_RAM_NO_ROUNDS:
        snprintf(cpu_detail, sizeof(cpu_detail), "No completed round");
        break;
    case BOARD_CPU_RAM_OK:
        snprintf(cpu_detail, sizeof(cpu_detail), "4x16K / %" PRIu32 " rounds", cpu->rounds);
        break;
    default:
        snprintf(cpu_detail, sizeof(cpu_detail), "2 s / 64K internal");
        break;
    }
    update_tile(BOARD_TEST_CPU_RAM, status, cpu_detail);
    update_tile(BOARD_TEST_LCD, status, "800 x 480 RGB");

    char touch_detail[24];
    if ((status->completed_mask & BOARD_TEST_MASK(BOARD_TEST_TOUCH)) == 0U) {
        snprintf(touch_detail, sizeof(touch_detail), "I2C probe pending");
    } else if (status->touch_ok) {
        snprintf(touch_detail, sizeof(touch_detail), "GT911 init @0x%02X",
                 status->touch_address);
    } else {
        snprintf(touch_detail, sizeof(touch_detail), "Unavailable");
    }
    update_tile(BOARD_TEST_TOUCH, status, touch_detail);

    char sd_detail[24];
    if ((status->completed_mask & BOARD_TEST_MASK(BOARD_TEST_SD)) == 0U) {
        snprintf(sd_detail, sizeof(sd_detail), "4-bit test pending");
    } else {
        snprintf(sd_detail, sizeof(sd_detail), "%s",
                 status->sd_ok ? "4-bit I/O OK" :
                                 esp_err_to_name(status->sd_error));
    }
    update_tile(BOARD_TEST_SD, status, sd_detail);
    update_tile(BOARD_TEST_LED, status, "GPIO54 steady ON");
}
