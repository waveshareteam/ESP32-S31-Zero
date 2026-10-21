#include "board_peripherals.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "board_ui.h"
#include "board_touch_reader.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/sdmmc_host.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"

#define LCD_H_RES 800
#define LCD_V_RES 480
#define LCD_PCLK_HZ (16 * 1000 * 1000)
#define LCD_BOUNCE_BUFFER_LINES 20
#define LCD_DRAW_BUFFER_LINES 10
#define LCD_RST GPIO_NUM_37
#define LCD_BL GPIO_NUM_42
#define TOUCH_INT GPIO_NUM_7
#define TOUCH_RST GPIO_NUM_38
#define TOUCH_SDA GPIO_NUM_52
#define TOUCH_SCL GPIO_NUM_53
#define TOUCH_TRANSACTION_TIMEOUT_MS 50
#define TOUCH_I2C_CLOCK_HZ 100000
#define STATUS_LED GPIO_NUM_54
#define SD_MOUNT_POINT "/sdcard"
#define SD_BUS_IDLE_DELAY_MS 100
#define SD_MOUNT_ATTEMPTS 2

#if CONFIG_BOARD_TOUCH_TRACKING
static const char *TAG = "board_peripherals";
#endif
static i2c_master_bus_handle_t s_touch_bus;
static esp_lcd_panel_io_handle_t s_touch_io;
static esp_lcd_touch_handle_t s_touch;
static esp_lcd_touch_io_gt911_config_t s_gt911_config;
static bool s_ui_ready;

static bool status_led_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << STATUS_LED,
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&config);
    if (err == ESP_OK) {
        err = gpio_set_level(STATUS_LED, 0);
    }
    const bool ok = err == ESP_OK;
    printf("[%s] GPIO54 status LED | steady ON(active-low), init=%s\n",
           ok ? "PASS" : "FAIL", esp_err_to_name(err));
    return ok;
}

static esp_err_t sd_card_bus_release(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_20) | (1ULL << GPIO_NUM_21) |
                        (1ULL << GPIO_NUM_22) | (1ULL << GPIO_NUM_23) |
                        (1ULL << GPIO_NUM_24) | (1ULL << GPIO_NUM_25),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&config);
}

/* This board test owns Slot 0 and runs transactions sequentially. */
static bool s_sd_app_command;

static esp_err_t sd_card_transaction(int slot, sdmmc_command_t *cmd)
{
    const bool app_command = s_sd_app_command;
    s_sd_app_command = false;
    const esp_err_t err = sdmmc_host_do_transaction(slot, cmd);
    const esp_err_t result = err == ESP_OK ? cmd->error : err;
    if (cmd->opcode == 55 && result == ESP_OK) {
        s_sd_app_command = true;
    }
    /* Show data transfers and ACMD6 even with global logging at ERROR.
     * CMD5/CMD8 timeouts can be normal during card detection.
     */
    if (cmd->data != NULL || (app_command && cmd->opcode == 6)) {
        int freq_khz = 0;
        const esp_err_t freq_err = sdmmc_host_get_real_freq(slot, &freq_khz);
        printf("[SD] %sCMD%" PRIu32 " arg=0x%08" PRIx32
               " bytes=%zu clock=%d kHz (%s) result=%s\n",
               app_command ? "A" : "", cmd->opcode, cmd->arg,
               (size_t)cmd->datalen, freq_khz, esp_err_to_name(freq_err),
               esp_err_to_name(result));
    }
    return err;
}

static bool sd_card_test(esp_err_t *test_result)
{
    if (test_result != NULL) {
        *test_result = ESP_FAIL;
    }
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.flags &= ~(SDMMC_HOST_FLAG_1BIT | SDMMC_HOST_FLAG_8BIT | SDMMC_HOST_FLAG_DDR);
    host.flags |= SDMMC_HOST_FLAG_4BIT;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    host.do_transaction = sd_card_transaction;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = GPIO_NUM_24;
    slot.cmd = GPIO_NUM_25;
    slot.d0 = GPIO_NUM_20;
    slot.d1 = GPIO_NUM_21;
    slot.d2 = GPIO_NUM_22;
    slot.d3 = GPIO_NUM_23;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    sdmmc_card_t *card = NULL;
    esp_err_t err = ESP_FAIL;
    unsigned attempts_used = 0;
    for (unsigned attempt = 1; attempt <= SD_MOUNT_ATTEMPTS; ++attempt) {
        attempts_used = attempt;
        err = sd_card_bus_release();
        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(SD_BUS_IDLE_DELAY_MS));
            card = NULL;
            s_sd_app_command = false;
            err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot,
                                          &mount_config, &card);
        }
        if (err == ESP_OK) {
            break;
        }
        printf("[WARN] SD card | 4-bit max=%u MHz attempt %u/%u=%s\n",
               (unsigned)(host.max_freq_khz / 1000), attempt,
               (unsigned)SD_MOUNT_ATTEMPTS, esp_err_to_name(err));
    }
    if (err != ESP_OK) {
        const esp_err_t release_err = sd_card_bus_release();
        printf("[FAIL] SD card | SDMMC 4-bit max=%u MHz, attempts=%u, "
               "mount/init=%s, bus-release=%s; continuing without SD\n",
               (unsigned)(host.max_freq_khz / 1000), attempts_used,
               esp_err_to_name(err), esp_err_to_name(release_err));
        if (test_result != NULL) {
            *test_result = err;
        }
        return false;
    }

    /* The protocol starts in 1-bit mode as required by SD, but a successful
     * board test must actually negotiate 4-bit transfers on D0 through D3.
     */
    if (card->log_bus_width != 2) {
        printf("[FAIL] SD card | negotiated width is not 4-bit; skipping file I/O\n");
        const esp_err_t unmount_err = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
        const esp_err_t release_err = sd_card_bus_release();
        printf("[SD] unmount=%s, bus-release=%s\n",
               esp_err_to_name(unmount_err), esp_err_to_name(release_err));
        if (test_result != NULL) {
            *test_result = ESP_ERR_NOT_SUPPORTED;
        }
        return false;
    }
    int actual_freq_khz = 0;
    const esp_err_t freq_err = sdmmc_host_get_real_freq(host.slot, &actual_freq_khz);
    printf("[SD] negotiated width=4-bit, clock=%d kHz (%s)\n",
           actual_freq_khz, esp_err_to_name(freq_err));
    sdmmc_card_print_info(stdout, card);

    const char *path = SD_MOUNT_POINT "/.s31-board-test.tmp";
    static const char payload[] = "ESP32-S31 SDMMC test\n";
    char readback[sizeof(payload)] = {0};
    FILE *file = fopen(path, "wb");
    bool ok = file != NULL;
    if (ok) {
        ok = fwrite(payload, 1, sizeof(payload), file) == sizeof(payload) &&
             fflush(file) == 0 && fsync(fileno(file)) == 0;
        ok = fclose(file) == 0 && ok;
        file = NULL;
    }
    if (ok) {
        file = fopen(path, "rb");
        ok = file != NULL &&
             fread(readback, 1, sizeof(readback), file) == sizeof(readback) &&
             memcmp(payload, readback, sizeof(payload)) == 0;
    }
    if (file != NULL) {
        ok = fclose(file) == 0 && ok;
    }
    if (unlink(path) != 0 && errno != ENOENT) {
        ok = false;
    }
    const esp_err_t unmount_err = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
    const esp_err_t release_err = sd_card_bus_release();
    ok = ok && unmount_err == ESP_OK && release_err == ESP_OK;
    printf("[%s] SD card | SDMMC 4-bit max=%u MHz, "
           "attempts=%u, write/sync/read/compare/delete=%s, "
           "unmount=%s, bus-release=%s\n",
           ok ? "PASS" : "FAIL", (unsigned)(host.max_freq_khz / 1000),
           attempts_used, ok ? "OK" : "FAIL",
           esp_err_to_name(unmount_err), esp_err_to_name(release_err));
    if (test_result != NULL) {
        *test_result = ok ? ESP_OK : ESP_FAIL;
    }
    return ok;
}

static void touch_cleanup(void)
{
    if (s_touch != NULL) {
        esp_lcd_touch_del(s_touch);
        s_touch = NULL;
    }
    if (s_touch_io != NULL) {
        esp_lcd_panel_io_del(s_touch_io);
        s_touch_io = NULL;
    }
    if (s_touch_bus != NULL) {
        i2c_del_master_bus(s_touch_bus);
        s_touch_bus = NULL;
    }
}

static esp_lcd_touch_handle_t touch_init(uint8_t *detected_address)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = TOUCH_SDA,
        .scl_io_num = TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_config, &s_touch_bus);
    if (err != ESP_OK) {
        printf("[FAIL] Touch | I2C bus init=%s; continuing without touch\n",
               esp_err_to_name(err));
        return NULL;
    }

    const uint8_t addresses[] = {0x14, 0x5D};
    uint8_t found = 0;
    for (size_t i = 0; i < sizeof(addresses); ++i) {
        const esp_err_t probe = i2c_master_probe(s_touch_bus, addresses[i], 50);
        printf("[INFO] GT911 probe | address=0x%02X, result=%s\n",
               addresses[i], esp_err_to_name(probe));
        if (probe == ESP_OK && found == 0) {
            found = addresses[i];
        }
    }
    if (found == 0) {
        printf("\xE6\x9C\xAA\xE6\xA3\x80\xE6\xB5\x8B\xE5\x88\xB0"
               "\xE8\xA7\xA6\xE6\x91\xB8\n");
        touch_cleanup();
        return NULL;
    }

    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_config.dev_addr = found;
    io_config.scl_speed_hz = TOUCH_I2C_CLOCK_HZ;
    /* Bound each transaction in the independent sampling task. */
    io_config.transaction_timeout_ms = TOUCH_TRANSACTION_TIMEOUT_MS;
    err = esp_lcd_new_panel_io_i2c(s_touch_bus, &io_config, &s_touch_io);
    if (err != ESP_OK) {
        printf("[FAIL] Touch | panel I2C IO=%s; continuing without touch\n",
               esp_err_to_name(err));
        touch_cleanup();
        return NULL;
    }

    s_gt911_config.dev_addr = found;
    const esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_RST,
        .int_gpio_num = TOUCH_INT,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
        .driver_data = &s_gt911_config,
    };
    err = esp_lcd_touch_new_i2c_gt911(s_touch_io, &touch_config, &s_touch);
    if (err != ESP_OK) {
        printf("[FAIL] Touch | GT911 init at 0x%02X=%s; continuing without touch\n",
               found, esp_err_to_name(err));
        touch_cleanup();
        return NULL;
    }
    *detected_address = found;
    printf("[PASS] Touch | GT911 detected and initialized at 0x%02X\n", found);
    return s_touch;
}

#if CONFIG_BOARD_TOUCH_TRACKING
/* I2C is owned by this worker after initialization. LVGL only copies a
 * snapshot; neither bus transactions nor driver error logging hold its lock.
 */
static portMUX_TYPE s_touch_sample_lock = portMUX_INITIALIZER_UNLOCKED;
static board_touch_sample_t s_touch_sample;

static uint32_t touch_now_ms(void *ctx)
{
    (void)ctx;
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static esp_err_t touch_read_register(void *ctx, uint16_t reg, void *data, size_t size)
{
    return esp_lcd_panel_io_rx_param(ctx, reg, data, size);
}

static esp_err_t touch_write_register(void *ctx, uint16_t reg, const void *data, size_t size)
{
    return esp_lcd_panel_io_tx_param(ctx, reg, data, size);
}

static void touch_sample_task(void *arg)
{
    board_touch_reader_t reader;
    const board_touch_io_t io = {
        .ctx = arg,
        .read = touch_read_register,
        .write = touch_write_register,
        .now_ms = touch_now_ms,
    };
    board_touch_reader_init(&reader, io, LCD_H_RES, LCD_V_RES);
    for (;;) {
        const esp_err_t err = board_touch_reader_poll(&reader);
        portENTER_CRITICAL(&s_touch_sample_lock);
        s_touch_sample = reader.sample;
        portEXIT_CRITICAL(&s_touch_sample_lock);
        vTaskDelay(pdMS_TO_TICKS(err == ESP_OK ? 10 : 20));
    }
}

static void read_touch_snapshot(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    board_touch_sample_t sample;
    portENTER_CRITICAL(&s_touch_sample_lock);
    sample = s_touch_sample;
    portEXIT_CRITICAL(&s_touch_sample_lock);
    data->point.x = sample.x;
    data->point.y = sample.y;
    /* Expire here too: a worker waiting for the bus cannot keep a touch down. */
    data->state = board_touch_sample_pressed(&sample, touch_now_ms(NULL)) ?
                  LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    board_ui_set_touch_diagnostics(&sample);
}
#endif

static esp_err_t lcd_and_ui_start(board_status_t *status)
{
    const gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << LCD_RST) | (1ULL << LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&output_config);
    if (err != ESP_OK) {
        return err;
    }
    gpio_set_level(LCD_BL, 1);
    gpio_set_level(LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    const esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = LCD_PCLK_HZ,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .flags.pclk_active_neg = 1,
        },
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 1,
        /* Two internal buffers give the PSRAM copy ISR more scheduling margin. */
        .bounce_buffer_size_px = LCD_H_RES * LCD_BOUNCE_BUFFER_LINES,
        .dma_burst_size = 64,
        .hsync_gpio_num = GPIO_NUM_44,
        .vsync_gpio_num = GPIO_NUM_45,
        .de_gpio_num = GPIO_NUM_43,
        .pclk_gpio_num = GPIO_NUM_40,
        .disp_gpio_num = GPIO_NUM_NC,
        .data_gpio_nums = {8, 9, 10, 11, 12, 13, 14, 15,
                           16, 17, 18, 19, 33, 34, 35, 36},
        .flags.fb_in_psram = 1,
    };
    esp_lcd_panel_handle_t panel = NULL;
    err = esp_lcd_new_rgb_panel(&panel_config, &panel);
    if (err == ESP_OK) {
        err = esp_lcd_panel_init(panel);
    }
    if (err != ESP_OK) {
        if (panel != NULL) {
            esp_lcd_panel_del(panel);
        }
        return err;
    }

    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_stack_size = 12 * 1024;
    adapter_config.stack_in_psram = false;
    adapter_config.tick_period_ms = 10;
    adapter_config.task_min_delay_ms = 10;
    adapter_config.task_max_delay_ms = 20;
    err = esp_lv_adapter_init(&adapter_config);
    if (err != ESP_OK) {
        esp_lcd_panel_del(panel);
        return err;
    }

    esp_lv_adapter_display_config_t display_config =
        ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
            panel, NULL, LCD_H_RES, LCD_V_RES, ESP_LV_ADAPTER_ROTATE_0);
    display_config.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
    /* Render in internal RAM: avoid PSRAM-to-PSRAM copies while the RGB
     * bounce-buffer ISR is reading the framebuffer from that same memory.
     */
    display_config.profile.use_psram = false;
    display_config.profile.buffer_height = LCD_DRAW_BUFFER_LINES;
    lv_display_t *display = esp_lv_adapter_register_display(&display_config);
    if (display == NULL) {
        return ESP_FAIL;
    }
    err = esp_lv_adapter_start();
    if (err != ESP_OK) {
        return err;
    }
    if (esp_lv_adapter_lock(1000) != ESP_OK) {
        return ESP_ERR_TIMEOUT;
    }
    lv_indev_t *touch_indev = NULL;
#if CONFIG_BOARD_TOUCH_TRACKING
    if (s_touch != NULL) {
        /* Use a polling LVGL input over the snapshot. GT911 IRQs do not gate
         * retries or stale-touch release, and no IRQ owner competes for I2C.
         */
        touch_indev = lv_indev_create();
        if (touch_indev != NULL) {
            lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
            lv_indev_set_display(touch_indev, display);
            lv_indev_set_read_cb(touch_indev, read_touch_snapshot);
            if (xTaskCreate(touch_sample_task, "touch_sample", 4096, s_touch_io,
                            3, NULL) != pdPASS) {
                lv_indev_delete(touch_indev);
                touch_indev = NULL;
            }
        }
        if (touch_indev == NULL) {
            ESP_LOGE(TAG, "Touch input or sampling task allocation failed");
            status->touch_ok = false;
            status->touch_address = 0;
            touch_cleanup();
        }
    }
#endif
    status->lcd_ok = true;
    status->completed_mask |= BOARD_TEST_MASK(BOARD_TEST_LCD);
    /* Configure LVGL timers under the adapter lock. Keep the RTOS worker's
     * minimum sleep at one 10 ms tick instead of introducing a busy loop.
     */
    lv_timer_set_period(lv_display_get_refr_timer(display), 20);
    if (touch_indev != NULL) {
        lv_timer_set_period(lv_indev_get_read_timer(touch_indev), 10);
    }
    board_ui_create(status, touch_indev);
    s_ui_ready = true;
    lv_refr_now(NULL);
    esp_lv_adapter_unlock();
    gpio_set_level(LCD_BL, 0);
    /* Let the panel scan at least one rendered frame before the GPIO60 color cycle starts. */
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

esp_err_t board_peripherals_refresh_ui(const board_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ui_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_lv_adapter_lock(1000);
    if (err != ESP_OK) {
        return err;
    }
    board_ui_update(status);
    lv_refr_now(NULL);
    esp_lv_adapter_unlock();
    return ESP_OK;
}

esp_err_t board_peripherals_start(board_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_ui_ready = false;
    status->lcd_ok = false;
    status->touch_ok = false;
    status->touch_address = 0;
    status->sd_ok = false;
    status->sd_error = ESP_ERR_INVALID_STATE;
    status->led_ok = false;
    status->led_ok = status_led_init();
    status->completed_mask |= BOARD_TEST_MASK(BOARD_TEST_LED);
    status->sd_ok = sd_card_test(&status->sd_error);
    status->completed_mask |= BOARD_TEST_MASK(BOARD_TEST_SD);
    status->touch_ok = touch_init(&status->touch_address) != NULL;
    status->completed_mask |= BOARD_TEST_MASK(BOARD_TEST_TOUCH);
#if !CONFIG_BOARD_TOUCH_TRACKING
    /* Keep the startup result; release driver/bus RAM when no runtime input
     * is requested. No touch worker or LVGL input will be created.
     */
    touch_cleanup();
#endif

    esp_err_t err = lcd_and_ui_start(status);
    status->lcd_ok = err == ESP_OK;
    status->completed_mask |= BOARD_TEST_MASK(BOARD_TEST_LCD);
    printf("[%s] RGB LCD | 800x480 RGB565 at 16 MHz, init=%s\n",
           status->lcd_ok ? "PASS" : "FAIL", esp_err_to_name(err));
    return err;
}
