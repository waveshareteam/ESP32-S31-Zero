#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/rmt_tx.h"
#include "driver/spi_master.h"
#include "driver/temperature_sensor.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_chip_info.h"
#include "esp_clk_tree.h"
#include "esp_efuse.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_memory_utils.h"
#include "esp_netif.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_peripherals.h"
#include "led_strip_encoder.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "soc/clk_tree_defs.h"

#define FLASH_SECTOR_SIZE    4096U
#define FLASH_TEST_SUBTYPE  0x40
#define FLASH_TEST_TOKEN    "ERASE-TESTDATA"
#define DEFAULT_GPIO_OUT    8
#define DEFAULT_GPIO_IN     9
#define I2C_SDA_GPIO        52
#define I2C_SCL_GPIO        53
#define SPI_MOSI_GPIO       25
#define SPI_MISO_GPIO       20
#define SPI_SCLK_GPIO       24
#define SPI_CS_GPIO         23
#define RGB_GPIO            60
#define RGB_RMT_RESOLUTION  10000000U
#define RGB_LEVEL           24
#define RGB_COLOR_HOLD_MS    1000
#define WIFI_SCAN_MAX_APS   32
#define WIFI_MIN_RSSI_DBM   (-85)
#define FLASH_BENCH_BYTES   (512U * 1024U)
#define FLASH_BENCH_CHUNK   (16U * 1024U)
#define FLASH_BENCH_PASSES  4U
#define PSRAM_BENCH_BYTES   (2U * 1024U * 1024U)
#define PSRAM_BENCH_CHUNK   (32U * 1024U)
#define PSRAM_BENCH_PASSES  4U

static volatile uint32_t s_irq_count;
static bool s_gpio_isr_service_installed;
static rmt_channel_handle_t s_rgb_channel;
static rmt_encoder_handle_t s_rgb_encoder;
static bool s_rgb_cycle_started;
static atomic_int s_rgb_cycle_error;
static bool s_wifi_platform_ready;
static esp_netif_t *s_wifi_netif;

static esp_err_t rgb_init(void)
{
    if (s_rgb_channel != NULL && s_rgb_encoder != NULL) {
        return ESP_OK;
    }

    const rmt_tx_channel_config_t channel_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = RGB_GPIO,
        .mem_block_symbols = 48,
        .resolution_hz = RGB_RMT_RESOLUTION,
        .trans_queue_depth = 4,
    };
    esp_err_t err = rmt_new_tx_channel(&channel_config, &s_rgb_channel);
    if (err != ESP_OK) {
        return err;
    }

    const led_strip_encoder_config_t encoder_config = {
        .resolution_hz = RGB_RMT_RESOLUTION,
    };
    err = led_strip_new_encoder(&encoder_config, &s_rgb_encoder);
    if (err == ESP_OK) {
        err = rmt_enable(s_rgb_channel);
    }
    if (err != ESP_OK) {
        if (s_rgb_encoder != NULL) {
            rmt_del_encoder(s_rgb_encoder);
            s_rgb_encoder = NULL;
        }
        rmt_del_channel(s_rgb_channel);
        s_rgb_channel = NULL;
    }
    return err;
}

static esp_err_t rgb_set(uint8_t red, uint8_t green, uint8_t blue)
{
    if (s_rgb_channel == NULL || s_rgb_encoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Keep the payload valid even if waiting for RMT completion times out. */
    static uint8_t grb[3];
    grb[0] = green;
    grb[1] = red;
    grb[2] = blue;
    const rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    esp_err_t err = rmt_transmit(s_rgb_channel, s_rgb_encoder, grb,
                                 sizeof(grb), &tx_config);
    if (err == ESP_OK) {
        err = rmt_tx_wait_all_done(s_rgb_channel, 1000);
    }
    return err;
}

static void rgb_cycle_task(void *arg)
{
    (void)arg;
    static const uint8_t colors[][3] = {
        {RGB_LEVEL, 0, 0},
        {0, RGB_LEVEL, 0},
        {0, 0, RGB_LEVEL},
    };
    /* Red is already displayed by rgb_start_cycle(). Only this task writes
     * the strip after startup, including during repeated self-tests.
     */
    size_t color = 1;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(RGB_COLOR_HOLD_MS));
        const esp_err_t err = rgb_set(colors[color][0], colors[color][1],
                                      colors[color][2]);
        if (err != ESP_OK) {
            atomic_store(&s_rgb_cycle_error, err);
            ESP_LOGE("rgb", "Color cycle stopped: %s", esp_err_to_name(err));
            vTaskDelete(NULL);
        }
        color = (color + 1) % (sizeof(colors) / sizeof(colors[0]));
    }
}

static esp_err_t rgb_start_cycle(void)
{
    if (s_rgb_cycle_started) {
        return atomic_load(&s_rgb_cycle_error);
    }
    esp_err_t err = rgb_init();
    if (err == ESP_OK) {
        err = rgb_set(RGB_LEVEL, 0, 0);
    }
    if (err != ESP_OK) {
        return err;
    }
    atomic_store(&s_rgb_cycle_error, ESP_OK);
    if (xTaskCreate(rgb_cycle_task, "rgb_cycle", 2048, NULL, 1, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s_rgb_cycle_started = true;
    return ESP_OK;
}

static void result(const char *test, bool pass, const char *format, ...)
{
    printf("[%s] %-16s", pass ? "PASS" : "FAIL", test);
    if (format != NULL && format[0] != '\0') {
        printf(" | ");
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
    printf("\n");
}

static double mib_per_second(size_t bytes, int64_t elapsed_us)
{
    if (elapsed_us <= 0) {
        return 0.0;
    }
    return ((double)bytes * 1000000.0) /
           ((double)elapsed_us * 1024.0 * 1024.0);
}

static uint32_t fnv1a32(const uint8_t *data, size_t size)
{
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static bool parse_int(const char *text, int minimum, int maximum, int *value)
{
    if (text == NULL || value == NULL || text[0] == '\0') {
        return false;
    }

    char *end = NULL;
    const long parsed = strtol(text, &end, 0);
    if (*end != '\0' || parsed < minimum || parsed > maximum) {
        return false;
    }

    *value = (int)parsed;
    return true;
}

static bool pin_is_routed(int pin)
{
    if (pin >= 0 && pin <= 25) {
        return true;
    }
    if (pin >= 33 && pin <= 40) {
        return true;
    }
    if (pin >= 42 && pin <= 57) {
        return true;
    }
    return pin == 60 || pin == 61;
}

static bool pin_is_safe_general_test(int pin)
{
    if (!pin_is_routed(pin)) {
        return false;
    }

    // Reserve the always-on fixture LED as well as USB, straps and RGB/BOOT.
    return pin != 33 && pin != 34 && pin != 36 && pin != 37 &&
           pin != 54 && pin != 60 && pin != 61;
}

static void print_pin_warning(void)
{
    printf("Safe generic pins: GPIO0-25, 35, 38-40, 42-53, 55-57. GPIO54 is the always-on LED.\n");
    printf("GPIO33/34 are Type-C USB; GPIO36/37 are straps; GPIO60/61 are RGB/UART0/BOOT.\n");
    printf("GPIO29 and GPIO41 are not bonded. GPIO26-28 and GPIO30-32 are not application GPIO pads.\n");
}

static bool test_info(void)
{
    esp_chip_info_t chip_info = {0};
    uint8_t mac[6] = {0};
    uint32_t xtal_hz = 0;
    bool psram_initialized = false;
    size_t psram_size = 0;

#if CONFIG_SPIRAM
    psram_initialized = esp_psram_is_initialized();
    psram_size = esp_psram_get_size();
#endif

    esp_chip_info(&chip_info);
    const esp_err_t mac_err = esp_efuse_mac_get_default(mac);
    const esp_err_t clk_err = esp_clk_tree_src_get_freq_hz(
        SOC_MOD_CLK_XTAL, ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT, &xtal_hz);

    const bool pass = strcmp(CONFIG_IDF_TARGET, "esp32s31") == 0 &&
                      chip_info.cores == 2 && clk_err == ESP_OK &&
                      xtal_hz == 40000000 && mac_err == ESP_OK;
    result("Chip / clock", pass,
           "ESP32-S31, %u cores, rev=%u, XTAL=%" PRIu32
           " MHz, PSRAM=%s/%u MiB, MAC=%02X:%02X:%02X:%02X:%02X:%02X",
           chip_info.cores, chip_info.revision, xtal_hz / 1000000U,
           psram_initialized ? "OK" : "OFF",
           (unsigned)(psram_size / (1024U * 1024U)),
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return pass;
}

static bool test_flash_info(uint32_t *physical_size_out)
{
    uint32_t jedec_id = 0;
    uint32_t physical_size = 0;

    if (physical_size_out != NULL) {
        *physical_size_out = 0;
    }

    const esp_err_t id_err = esp_flash_read_id(NULL, &jedec_id);
    const esp_err_t physical_err = esp_flash_get_physical_size(NULL, &physical_size);
    if (physical_err == ESP_OK && physical_size_out != NULL) {
        *physical_size_out = physical_size;
    }

    const bool pass = id_err == ESP_OK && physical_err == ESP_OK &&
                      physical_size > 0;
    result("Flash", pass,
           "JEDEC=0x%06" PRIX32 ", physical maximum=%" PRIu32
           " MiB, detect=%s",
           jedec_id, physical_size / (1024U * 1024U),
           esp_err_to_name(physical_err));
    return pass;
}

static bool test_flash_speed(void)
{
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (partition == NULL || partition->size < FLASH_BENCH_BYTES) {
        result("Flash speed", false, "application partition too small");
        return false;
    }

    uint8_t *buffer = heap_caps_malloc(
        FLASH_BENCH_CHUNK, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (buffer == NULL) {
        result("Flash speed", false, "internal buffer allocation failed");
        return false;
    }

    esp_err_t err = ESP_OK;
    int64_t elapsed_us = 0;
    uint32_t reference_hash = 0;
    bool stable = true;
    unsigned completed = 0;
    for (unsigned pass_index = 0; pass_index < FLASH_BENCH_PASSES; ++pass_index) {
        const int64_t started_us = esp_timer_get_time();
        for (size_t offset = 0; offset < FLASH_BENCH_BYTES;
             offset += FLASH_BENCH_CHUNK) {
            err = esp_flash_read(partition->flash_chip, buffer,
                                 partition->address + offset,
                                 FLASH_BENCH_CHUNK);
            if (err != ESP_OK) {
                break;
            }
        }
        elapsed_us += esp_timer_get_time() - started_us;
        if (err != ESP_OK) {
            break;
        }

        const uint32_t hash = fnv1a32(buffer, FLASH_BENCH_CHUNK);
        if (pass_index == 0) {
            reference_hash = hash;
        } else if (hash != reference_hash) {
            stable = false;
        }
        ++completed;
    }

    const size_t total_bytes = FLASH_BENCH_BYTES * completed;
    const double read_mib_s = mib_per_second(total_bytes, elapsed_us);
    free(buffer);

    const bool pass = err == ESP_OK && stable &&
                      completed == FLASH_BENCH_PASSES && elapsed_us > 0;
    result("Flash speed", pass,
           "read=%.2f MiB/s, 512 KiB x %u, %s@%d MHz, stable=%s/%s",
           read_mib_s, completed, CONFIG_ESPTOOLPY_FLASHMODE,
           CONFIG_ESPTOOLPY_FLASHFREQ_VAL, stable ? "OK" : "FAIL",
           esp_err_to_name(err));
    return pass;
}

static bool test_psram_speed(void)
{
#if !CONFIG_SPIRAM
    result("PSRAM speed", false, "PSRAM support is disabled");
    return false;
#else
    if (!esp_psram_is_initialized()) {
        result("PSRAM speed", false, "PSRAM is not initialized");
        return false;
    }

    uint8_t *external = heap_caps_malloc(
        PSRAM_BENCH_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *internal = heap_caps_malloc(
        PSRAM_BENCH_CHUNK, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const bool external_ok = external != NULL && esp_ptr_external_ram(external);
    if (!external_ok || internal == NULL) {
        free(external);
        free(internal);
        result("PSRAM speed", false,
               "allocation failed, external=%s, internal=%s",
               external_ok ? "OK" : "FAIL",
               internal != NULL ? "OK" : "FAIL");
        return false;
    }

    uint32_t state = 0x3153A55AU;
    for (size_t i = 0; i < PSRAM_BENCH_CHUNK; ++i) {
        state = state * 1664525U + 1013904223U;
        internal[i] = (uint8_t)(state >> 24);
    }

    volatile uint32_t guard = 0;
    int64_t write_us = 0;
    for (unsigned pass_index = 0; pass_index < PSRAM_BENCH_PASSES; ++pass_index) {
        const int64_t started_us = esp_timer_get_time();
        for (size_t offset = 0; offset < PSRAM_BENCH_BYTES;
             offset += PSRAM_BENCH_CHUNK) {
            memcpy(external + offset, internal, PSRAM_BENCH_CHUNK);
        }
        write_us += esp_timer_get_time() - started_us;
        guard ^= external[(pass_index * 4093U) % PSRAM_BENCH_BYTES];
    }

    bool verified = true;
    for (size_t offset = 0; offset < PSRAM_BENCH_BYTES;
         offset += PSRAM_BENCH_CHUNK) {
        if (memcmp(external + offset, internal, PSRAM_BENCH_CHUNK) != 0) {
            verified = false;
            break;
        }
    }

    int64_t read_us = 0;
    for (unsigned pass_index = 0; pass_index < PSRAM_BENCH_PASSES; ++pass_index) {
        const int64_t started_us = esp_timer_get_time();
        for (size_t offset = 0; offset < PSRAM_BENCH_BYTES;
             offset += PSRAM_BENCH_CHUNK) {
            memcpy(internal, external + offset, PSRAM_BENCH_CHUNK);
            guard ^= internal[((offset / PSRAM_BENCH_CHUNK) + pass_index) %
                              PSRAM_BENCH_CHUNK];
        }
        read_us += esp_timer_get_time() - started_us;
    }

    const size_t total_bytes = PSRAM_BENCH_BYTES * PSRAM_BENCH_PASSES;
    const double write_mib_s = mib_per_second(total_bytes, write_us);
    const double read_mib_s = mib_per_second(total_bytes, read_us);
    free(external);
    free(internal);

    const bool pass = verified && write_us > 0 && read_us > 0;
    result("PSRAM speed", pass,
           "write=%.2f, read=%.2f MiB/s, 2 MiB x %u, %d MHz, verify=%s, guard=%08" PRIX32,
           write_mib_s, read_mib_s, PSRAM_BENCH_PASSES,
           CONFIG_SPIRAM_SPEED, verified ? "OK" : "FAIL", (uint32_t)guard);
    return pass;
#endif
}

static bool test_flash_rw(const char *confirmation)
{
    if (confirmation == NULL || strcmp(confirmation, FLASH_TEST_TOKEN) != 0) {
        printf("Refused. This test erases only partition 'testdata'. Run: flash_rw %s\n",
               FLASH_TEST_TOKEN);
        result("Flash R/W", false, "confirmation required");
        return false;
    }

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, FLASH_TEST_SUBTYPE, "testdata");
    if (partition == NULL || partition->size < FLASH_SECTOR_SIZE) {
        result("Flash R/W", false, "testdata partition missing");
        return false;
    }

    uint8_t *write_buffer = heap_caps_malloc(FLASH_SECTOR_SIZE, MALLOC_CAP_8BIT);
    uint8_t *read_buffer = heap_caps_malloc(FLASH_SECTOR_SIZE, MALLOC_CAP_8BIT);
    if (write_buffer == NULL || read_buffer == NULL) {
        free(write_buffer);
        free(read_buffer);
        result("Flash R/W", false, "allocation failed");
        return false;
    }

    uint32_t state = 0x5310A55AU;
    for (size_t i = 0; i < FLASH_SECTOR_SIZE; ++i) {
        state = state * 1664525U + 1013904223U;
        write_buffer[i] = (uint8_t)(state >> 24);
    }
    memset(read_buffer, 0, FLASH_SECTOR_SIZE);

    esp_err_t err = esp_partition_erase_range(partition, 0, FLASH_SECTOR_SIZE);
    if (err == ESP_OK) {
        err = esp_partition_write(partition, 0, write_buffer, FLASH_SECTOR_SIZE);
    }
    if (err == ESP_OK) {
        err = esp_partition_read(partition, 0, read_buffer, FLASH_SECTOR_SIZE);
    }
    const bool data_match = err == ESP_OK &&
                            memcmp(write_buffer, read_buffer, FLASH_SECTOR_SIZE) == 0;
    const esp_err_t cleanup_err = esp_partition_erase_range(
        partition, 0, FLASH_SECTOR_SIZE);

    free(write_buffer);
    free(read_buffer);

    const bool pass = data_match && cleanup_err == ESP_OK;
    result("Flash R/W", pass, "address=0x%08" PRIX32 ", %u bytes, cleanup=%s",
           partition->address, FLASH_SECTOR_SIZE, esp_err_to_name(cleanup_err));
    return pass;
}

static bool test_gpio_loopback(int output_pin, int input_pin, int cycles)
{
    if (output_pin == input_pin || !pin_is_safe_general_test(output_pin) ||
        !pin_is_safe_general_test(input_pin)) {
        print_pin_warning();
        result("gpio", false, "unsafe_or_unrouted_pin");
        return false;
    }

    const gpio_config_t output_config = {
        .pin_bit_mask = 1ULL << output_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const gpio_config_t input_config = {
        .pin_bit_mask = 1ULL << input_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&output_config);
    if (err == ESP_OK) {
        err = gpio_config(&input_config);
    }

    int mismatches = 0;
    for (int i = 0; err == ESP_OK && i < cycles; ++i) {
        const int expected = i & 1;
        err = gpio_set_level((gpio_num_t)output_pin, expected);
        vTaskDelay(pdMS_TO_TICKS(2));
        if (err == ESP_OK && gpio_get_level((gpio_num_t)input_pin) != expected) {
            ++mismatches;
        }
    }

    gpio_reset_pin((gpio_num_t)output_pin);
    gpio_reset_pin((gpio_num_t)input_pin);
    const bool pass = err == ESP_OK && mismatches == 0;
    result("gpio", pass, "out=%d;in=%d;cycles=%d;mismatches=%d",
           output_pin, input_pin, cycles, mismatches);
    return pass;
}

static void IRAM_ATTR gpio_irq_handler(void *argument)
{
    (void)argument;
    ++s_irq_count;
}

static bool test_gpio_irq(int output_pin, int input_pin, int cycles)
{
    if (output_pin == input_pin || !pin_is_safe_general_test(output_pin) ||
        !pin_is_safe_general_test(input_pin)) {
        print_pin_warning();
        result("irq", false, "unsafe_or_unrouted_pin");
        return false;
    }

    if (!s_gpio_isr_service_installed) {
        const esp_err_t service_err = gpio_install_isr_service(0);
        if (service_err != ESP_OK && service_err != ESP_ERR_INVALID_STATE) {
            result("irq", false, "isr_service=%s", esp_err_to_name(service_err));
            return false;
        }
        s_gpio_isr_service_installed = true;
    }

    const gpio_config_t output_config = {
        .pin_bit_mask = 1ULL << output_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const gpio_config_t input_config = {
        .pin_bit_mask = 1ULL << input_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };

    esp_err_t err = gpio_config(&output_config);
    if (err == ESP_OK) {
        err = gpio_config(&input_config);
    }
    if (err == ESP_OK) {
        err = gpio_isr_handler_add((gpio_num_t)input_pin, gpio_irq_handler, NULL);
    }

    s_irq_count = 0;
    if (err == ESP_OK) {
        gpio_set_level((gpio_num_t)output_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(5));
        for (int i = 0; i < cycles; ++i) {
            gpio_set_level((gpio_num_t)output_pin, 1);
            vTaskDelay(pdMS_TO_TICKS(2));
            gpio_set_level((gpio_num_t)output_pin, 0);
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }

    gpio_isr_handler_remove((gpio_num_t)input_pin);
    gpio_reset_pin((gpio_num_t)output_pin);
    gpio_reset_pin((gpio_num_t)input_pin);

    const uint32_t expected = (uint32_t)cycles * 2U;
    const bool pass = err == ESP_OK && s_irq_count >= expected - 1U &&
                      s_irq_count <= expected + 1U;
    result("irq", pass, "out=%d;in=%d;expected=%" PRIu32 ";actual=%" PRIu32,
           output_pin, input_pin, expected, s_irq_count);
    return pass;
}

static bool test_pwm(int pin, int frequency_hz, int duty_percent, int seconds)
{
    if (!pin_is_safe_general_test(pin)) {
        print_pin_warning();
        result("pwm", false, "unsafe_or_unrouted_pin");
        return false;
    }

    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = (uint32_t)frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const uint32_t duty = ((1U << LEDC_TIMER_13_BIT) - 1U) *
                          (uint32_t)duty_percent / 100U;
    const ledc_channel_config_t channel_config = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = duty,
        .hpoint = 0,
    };

    esp_err_t err = ledc_timer_config(&timer_config);
    if (err == ESP_OK) {
        err = ledc_channel_config(&channel_config);
    }
    if (err == ESP_OK) {
        printf("Measure GPIO%d now: %d Hz, %d%% duty, %d second(s).\n",
               pin, frequency_hz, duty_percent, seconds);
        vTaskDelay(pdMS_TO_TICKS((uint32_t)seconds * 1000U));
    }

    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    gpio_reset_pin((gpio_num_t)pin);
    const bool pass = err == ESP_OK;
    result("pwm", pass, "gpio=%d;hz=%d;duty_pct=%d;seconds=%d",
           pin, frequency_hz, duty_percent, seconds);
    return pass;
}

static bool test_adc(int pin, int samples)
{
    if (pin == 54) {
        result("adc", false, "GPIO54 is reserved for the always-on status LED");
        return false;
    }
    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t channel = ADC_CHANNEL_0;
    esp_err_t err = adc_oneshot_io_to_channel(pin, &unit, &channel);
    if (err != ESP_OK) {
        result("adc", false, "gpio=%d;mapping=%s", pin, esp_err_to_name(err));
        return false;
    }

    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = unit,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_unit_handle_t handle = NULL;
    err = adc_oneshot_new_unit(&unit_config, &handle);
    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_0,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (err == ESP_OK) {
        err = adc_oneshot_config_channel(handle, channel, &channel_config);
    }

    int minimum = INT32_MAX;
    int maximum = INT32_MIN;
    int64_t sum = 0;
    for (int i = 0; err == ESP_OK && i < samples; ++i) {
        int raw = 0;
        err = adc_oneshot_read(handle, channel, &raw);
        if (err == ESP_OK) {
            if (raw < minimum) {
                minimum = raw;
            }
            if (raw > maximum) {
                maximum = raw;
            }
            sum += raw;
        }
    }
    if (handle != NULL) {
        adc_oneshot_del_unit(handle);
    }

    const int average = err == ESP_OK ? (int)(sum / samples) : 0;
    const bool pass = err == ESP_OK;
    result("adc", pass, "gpio=%d;unit=%d;channel=%d;samples=%d;min=%d;avg=%d;max=%d",
           pin, unit, channel, samples, minimum, average, maximum);
    return pass;
}

static bool test_uart(int tx_pin, int rx_pin)
{
    const bool h2_uart0 = tx_pin == 60 && rx_pin == 61;
    if (h2_uart0 && s_rgb_channel != NULL) {
        result("uart", false, "GPIO60 is reserved for the RGB color cycle");
        return false;
    }
    if ((!h2_uart0 && (!pin_is_safe_general_test(tx_pin) ||
                      !pin_is_safe_general_test(rx_pin))) || tx_pin == rx_pin) {
        printf("Use safe routed pins, or explicitly use H2 UART0 as: uart 60 61\n");
        result("uart", false, "unsafe_or_unrouted_pin");
        return false;
    }

    if (h2_uart0) {
        printf("H2 UART0 shares GPIO60 RGB/strap and GPIO61 BOOT. Connect H2-18 to H2-17 only after boot.\n");
    }

    const uart_port_t port = h2_uart0 ? UART_NUM_0 : UART_NUM_1;
    const uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(port, 512, 0, 0, NULL, 0);
    if (err == ESP_OK) {
        err = uart_param_config(port, &config);
    }
    if (err == ESP_OK) {
        err = uart_set_pin(port, tx_pin, rx_pin, UART_PIN_NO_CHANGE,
                           UART_PIN_NO_CHANGE);
    }

    static const uint8_t payload[] = "ESP32-S31-Zero UART loopback 0123456789\r\n";
    uint8_t received[sizeof(payload)] = {0};
    int written = -1;
    int read = -1;
    if (err == ESP_OK) {
        uart_flush_input(port);
        written = uart_write_bytes(port, payload, sizeof(payload));
        uart_wait_tx_done(port, pdMS_TO_TICKS(100));
        read = uart_read_bytes(port, received, sizeof(payload), pdMS_TO_TICKS(300));
    }

    if (uart_is_driver_installed(port)) {
        uart_driver_delete(port);
    }
    gpio_reset_pin((gpio_num_t)tx_pin);
    gpio_reset_pin((gpio_num_t)rx_pin);

    const bool pass = err == ESP_OK && written == sizeof(payload) &&
                      read == sizeof(payload) &&
                      memcmp(payload, received, sizeof(payload)) == 0;
    result("uart", pass, "port=%d;tx=%d;rx=%d;written=%d;read=%d",
           port, tx_pin, rx_pin, written, read);
    return pass;
}

static bool test_i2c(void)
{
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = i2c_new_master_bus(&config, &bus);
    int devices = 0;
    int timeouts = 0;

    for (uint8_t address = 0x08; err == ESP_OK && address <= 0x77; ++address) {
        const esp_err_t probe_err = i2c_master_probe(bus, address, 30);
        if (probe_err == ESP_OK) {
            printf("I2C address 0x%02X acknowledged\n", address);
            ++devices;
        } else if (probe_err == ESP_ERR_TIMEOUT) {
            ++timeouts;
        }
    }
    if (bus != NULL) {
        i2c_del_master_bus(bus);
    }

    const bool pass = err == ESP_OK && timeouts == 0;
    result("i2c", pass, "sda=%d;scl=%d;devices=%d;timeouts=%d",
           I2C_SDA_GPIO, I2C_SCL_GPIO, devices, timeouts);
    return pass;
}

static bool test_spi(void)
{
    printf("Connect J2-39 GPIO25/MOSI to J2-29 GPIO20/MISO. Probe J2-37 GPIO24/CLK and J2-33 GPIO23/CS.\n");
    const spi_bus_config_t bus_config = {
        .mosi_io_num = SPI_MOSI_GPIO,
        .miso_io_num = SPI_MISO_GPIO,
        .sclk_io_num = SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64,
    };
    const spi_device_interface_config_t device_config = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = SPI_CS_GPIO,
        .queue_size = 1,
    };

    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_DISABLED);
    spi_device_handle_t device = NULL;
    if (err == ESP_OK) {
        err = spi_bus_add_device(SPI2_HOST, &device_config, &device);
    }

    const uint8_t tx[] = {0x00, 0xFF, 0xA5, 0x5A, 0x31, 0x53, 0xC3, 0x3C};
    uint8_t rx[sizeof(tx)] = {0};
    spi_transaction_t transaction = {
        .length = sizeof(tx) * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    if (err == ESP_OK) {
        err = spi_device_transmit(device, &transaction);
    }

    if (device != NULL) {
        spi_bus_remove_device(device);
    }
    spi_bus_free(SPI2_HOST);

    const bool pass = err == ESP_OK && memcmp(tx, rx, sizeof(tx)) == 0;
    result("spi", pass, "mosi=%d;miso=%d;sclk=%d;cs=%d;bytes=%u",
           SPI_MOSI_GPIO, SPI_MISO_GPIO, SPI_SCLK_GPIO, SPI_CS_GPIO,
           (unsigned)sizeof(tx));
    return pass;
}

static bool test_usb(void)
{
    const bool connected = usb_serial_jtag_is_connected();
    result("USB Serial/JTAG", connected,
           "%s, D-=GPIO33, D+=GPIO34",
           connected ? "connected" : "not connected");
    return connected;
}

static bool read_temperature(float *celsius)
{
    const temperature_sensor_config_t config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    temperature_sensor_handle_t handle = NULL;
    esp_err_t err = temperature_sensor_install(&config, &handle);
    if (err == ESP_OK) {
        err = temperature_sensor_enable(handle);
    }
    if (err == ESP_OK) {
        err = temperature_sensor_get_celsius(handle, celsius);
    }
    if (handle != NULL) {
        temperature_sensor_disable(handle);
        temperature_sensor_uninstall(handle);
    }
    return err == ESP_OK;
}

static bool test_temperature(void)
{
    float celsius = 0.0f;
    const bool pass = read_temperature(&celsius);
    result("Temperature", pass, "die=%.1f C", celsius);
    return pass;
}

static esp_err_t prepare_wifi_platform(void)
{
    if (s_wifi_platform_ready) {
        return ESP_OK;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_wifi_netif = esp_netif_create_default_wifi_sta();
    if (s_wifi_netif == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_wifi_platform_ready = true;
    return ESP_OK;
}

static bool test_wifi(void)
{
    static wifi_ap_record_t records[WIFI_SCAN_MAX_APS];
    const char *stage = "platform";
    bool wifi_initialized = false;
    bool wifi_started = false;
    uint16_t total = 0;
    uint16_t count = WIFI_SCAN_MAX_APS;
    uint16_t aps_2g = 0;
    int strongest_rssi = -127;
    uint8_t strongest_channel = 0;

    esp_err_t err = prepare_wifi_platform();
    if (err != ESP_OK) {
        goto finish;
    }

    stage = "init";
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) {
        goto finish;
    }
    wifi_initialized = true;

    stage = "storage";
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        goto finish;
    }
    stage = "mode";
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        goto finish;
    }
    stage = "start";
    err = esp_wifi_start();
    if (err != ESP_OK) {
        goto finish;
    }
    wifi_started = true;

    const wifi_scan_config_t scan_config = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active = {
            .min = 50,
            .max = 120,
        },
    };
    stage = "scan";
    err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        goto finish;
    }
    stage = "get_count";
    err = esp_wifi_scan_get_ap_num(&total);
    if (err != ESP_OK) {
        goto finish;
    }

    memset(records, 0, sizeof(records));
    stage = "get_records";
    err = esp_wifi_scan_get_ap_records(&count, records);
    if (err != ESP_OK) {
        goto finish;
    }
    for (uint16_t i = 0; i < count; ++i) {
        if (records[i].primary >= 1 && records[i].primary <= 14) {
            ++aps_2g;
            if (records[i].rssi > strongest_rssi) {
                strongest_rssi = records[i].rssi;
                strongest_channel = records[i].primary;
            }
        }
    }
    stage = "complete";

finish:
    if (wifi_started) {
        const esp_err_t stop_err = esp_wifi_stop();
        if (err == ESP_OK && stop_err != ESP_OK) {
            err = stop_err;
            stage = "stop";
        }
    }
    if (wifi_initialized) {
        const esp_err_t deinit_err = esp_wifi_deinit();
        if (err == ESP_OK && deinit_err != ESP_OK) {
            err = deinit_err;
            stage = "deinit";
        }
    }

    const bool pass = err == ESP_OK && aps_2g > 0 &&
                      strongest_rssi >= WIFI_MIN_RSSI_DBM;
    result("WiFi 2.4G", pass,
           "APs=%u/%u, best=%d dBm, CH=%u, limit=%d dBm, %s/%s",
           total, aps_2g, strongest_rssi, strongest_channel,
           WIFI_MIN_RSSI_DBM, stage, esp_err_to_name(err));
    return pass;
}

static bool test_stress(int seconds, board_cpu_ram_result_t *detail)
{
    board_cpu_ram_result_t local = {0};
    if (detail == NULL) {
        detail = &local;
    }
    *detail = (board_cpu_ram_result_t){0};
    enum { BLOCK_COUNT = BOARD_CPU_RAM_TEST_BYTES / BOARD_CPU_RAM_BLOCK_BYTES };
    const size_t words = BOARD_CPU_RAM_BLOCK_BYTES / sizeof(uint32_t);
    uint32_t *buffers[BLOCK_COUNT] = {0};
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    detail->free_bytes = heap_caps_get_free_size(caps);
    detail->largest_block = heap_caps_get_largest_free_block(caps);
    /* Reserve all 64 KiB at once in smaller blocks: the screen can leave
     * enough internal RAM overall without one contiguous 64 KiB region.
     * Never reuse one small block and report it as 64 KiB of tested RAM.
     */
    for (size_t block = 0; block < BLOCK_COUNT; ++block) {
        buffers[block] = heap_caps_malloc(BOARD_CPU_RAM_BLOCK_BYTES, caps);
        if (buffers[block] == NULL) {
            detail->outcome = BOARD_CPU_RAM_NO_MEMORY;
            detail->free_bytes = heap_caps_get_free_size(caps);
            detail->largest_block = heap_caps_get_largest_free_block(caps);
            for (size_t allocated = 0; allocated < block; ++allocated) {
                free(buffers[allocated]);
            }
            result("CPU / RAM", false,
                   "allocation_failed: reserved=%" PRIu32 "/%u B internal, "
                   "block=%u B, remaining free=%" PRIu32 " B, largest=%" PRIu32 " B",
                   detail->allocated_bytes, BOARD_CPU_RAM_TEST_BYTES,
                   BOARD_CPU_RAM_BLOCK_BYTES, detail->free_bytes, detail->largest_block);
            return false;
        }
        detail->allocated_bytes += BOARD_CPU_RAM_BLOCK_BYTES;
    }

    float start_c = 0.0f;
    float end_c = 0.0f;
    read_temperature(&start_c);
    const int64_t deadline = esp_timer_get_time() + (int64_t)seconds * 1000000LL;
    uint32_t rounds = 0;
    bool pass = true;
    while (esp_timer_get_time() < deadline && pass) {
        const uint32_t seed = esp_random() | 1U;
        uint32_t state = seed;
        for (size_t block = 0; block < BLOCK_COUNT; ++block) {
            for (size_t i = 0; i < words; ++i) {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                buffers[block][i] = state;
            }
        }
        state = seed;
        for (size_t block = 0; block < BLOCK_COUNT && pass; ++block) {
            for (size_t i = 0; i < words; ++i) {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                const uint32_t actual = buffers[block][i];
                if (actual != state) {
                    detail->outcome = BOARD_CPU_RAM_MISMATCH;
                    detail->mismatch_offset = block * BOARD_CPU_RAM_BLOCK_BYTES + i * sizeof(uint32_t);
                    detail->expected = state;
                    detail->actual = actual;
                    pass = false;
                    break;
                }
            }
        }
        if (pass) {
            ++rounds;
        }
        /* A yield alone does not let lower-priority tasks run. Leave one
         * scheduler tick for idle/background work between verified blocks.
         */
        vTaskDelay(1);
    }
    read_temperature(&end_c);
    for (size_t block = 0; block < BLOCK_COUNT; ++block) {
        free(buffers[block]);
    }

    detail->rounds = rounds;
    if (pass) {
        pass = rounds > 0;
        detail->outcome = pass ? BOARD_CPU_RAM_OK : BOARD_CPU_RAM_NO_ROUNDS;
    }
    if (detail->outcome == BOARD_CPU_RAM_MISMATCH) {
        result("CPU / RAM", false,
               "mismatch: offset=0x%" PRIx32 ", expected=0x%08" PRIx32
               ", actual=0x%08" PRIx32 ", verified rounds=%" PRIu32,
               detail->mismatch_offset, detail->expected, detail->actual, rounds);
        return false;
    }

    result("CPU / RAM", pass,
           "%d s, rounds=%" PRIu32 ", die=%.1f -> %.1f C, %s",
           seconds, rounds, start_c, end_c,
           pass ? "64 KiB verified (4 x 16 KiB internal)" : "no completed round");
    return pass;
}

static void record_test_result(board_status_t *status, board_test_id_t id,
                               bool *stored, bool test_pass, bool *overall)
{
    if (stored != NULL) {
        *stored = test_pass;
    }
    if (status != NULL) {
        status->completed_mask |= BOARD_TEST_MASK(id);
        (void)board_peripherals_refresh_ui(status);
    }
    if (!test_pass) {
        *overall = false;
    }
}

static bool run_selftest(board_status_t *status, bool usb_pretested)
{
    bool tests_pass = !usb_pretested || status == NULL || status->usb_ok;
    printf("\n============================================================\n");
    printf(" ESP32-S31-Zero Automatic Board Self-Test\n");
    printf(" Target: %s | Flash: %s | Console: USB Serial/JTAG\n",
           CONFIG_IDF_TARGET, CONFIG_ESPTOOLPY_FLASHSIZE);
    printf(" LED: RED -> GREEN -> BLUE (1 s each) | 2.4 GHz antenna required on ANT1\n");
    printf("------------------------------------------------------------\n");

    esp_err_t indicator_err = rgb_start_cycle();
    const bool indicator_started_ok = indicator_err == ESP_OK;
    result("RGB indicator", indicator_started_ok,
           "GPIO%d, RED -> GREEN -> BLUE, %s", RGB_GPIO,
           esp_err_to_name(indicator_err));
    if (!indicator_started_ok) {
        tests_pass = false;
    }
    if (status != NULL) {
        status->rgb_indicator_ok = indicator_started_ok;
        status->completed_mask |= BOARD_TEST_MASK(BOARD_TEST_RGB);
        (void)board_peripherals_refresh_ui(status);
    }

    vTaskDelay(pdMS_TO_TICKS(250));
    record_test_result(status, BOARD_TEST_CHIP,
                       status ? &status->chip_ok : NULL, test_info(),
                       &tests_pass);
    record_test_result(status, BOARD_TEST_FLASH,
                       status ? &status->flash_ok : NULL,
                       test_flash_info(status ? &status->flash_size_bytes : NULL),
                       &tests_pass);
    record_test_result(status, BOARD_TEST_FLASH_RW,
                       status ? &status->flash_rw_ok : NULL,
                       test_flash_rw(FLASH_TEST_TOKEN), &tests_pass);
    record_test_result(status, BOARD_TEST_FLASH_SPEED,
                       status ? &status->flash_speed_ok : NULL,
                       test_flash_speed(), &tests_pass);
    record_test_result(status, BOARD_TEST_PSRAM,
                       status ? &status->psram_ok : NULL, test_psram_speed(),
                       &tests_pass);
    record_test_result(status, BOARD_TEST_TEMPERATURE,
                       status ? &status->temperature_ok : NULL,
                       test_temperature(), &tests_pass);
    record_test_result(status, BOARD_TEST_WIFI,
                       status ? &status->wifi_ok : NULL, test_wifi(),
                       &tests_pass);
    if (!usb_pretested || status == NULL) {
        record_test_result(status, BOARD_TEST_USB,
                           status ? &status->usb_ok : NULL, test_usb(),
                           &tests_pass);
    }
    record_test_result(status, BOARD_TEST_CPU_RAM,
                       status ? &status->cpu_ram_ok : NULL,
                       test_stress(2, status ? &status->cpu_ram_result : NULL),
                       &tests_pass);

    if (indicator_started_ok) {
        indicator_err = atomic_load(&s_rgb_cycle_error);
    }
    if (status != NULL) {
        status->rgb_indicator_ok =
            indicator_started_ok && indicator_err == ESP_OK;
        status->completed_mask |= BOARD_TEST_MASK(BOARD_TEST_RGB);
        (void)board_peripherals_refresh_ui(status);
    }
    const bool pass = tests_pass && indicator_err == ESP_OK;
    printf("------------------------------------------------------------\n");
    result("OVERALL", pass, "9 tests, LED=RGB cycle, indicator=%s",
           esp_err_to_name(indicator_err));
    printf("============================================================\n");
    return pass;
}

static bool test_light_sleep(int seconds)
{
    esp_err_t err = esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    printf("Entering light sleep for %d second(s). USB output pauses.\n", seconds);
    fflush(stdout);
    if (err == ESP_OK) {
        err = esp_light_sleep_start();
    }
    const uint32_t wakeup_causes = esp_sleep_get_wakeup_causes();
    const bool pass = err == ESP_OK &&
                      (wakeup_causes & (1U << ESP_SLEEP_WAKEUP_TIMER)) != 0U;
    result("light_sleep", pass, "seconds=%d;cause=%d;error=%s",
           seconds, (int)wakeup_causes, esp_err_to_name(err));
    return pass;
}

static void start_deep_sleep(int seconds)
{
    printf("Entering deep sleep for %d second(s). The USB device should disconnect and enumerate again after wake.\n",
           seconds);
    printf("[INFO] %-16s | seconds=%d, waiting for timer wake\n",
           "Deep sleep", seconds);
    fflush(stdout);
    esp_deep_sleep((uint64_t)seconds * 1000000ULL);
}

static void print_help(void)
{
    printf("\nESP32-S31-Zero board test commands\n");
    printf("  selftest | all              - automatic test; RGB keeps cycling red/green/blue\n");
    printf("  info                        - chip, reset, crystal, MAC, PSRAM\n");
    printf("  flash_info                  - JEDEC ID and detected physical capacity\n");
    printf("  flash_rw %s   - destructive test of dedicated testdata sector only\n",
           FLASH_TEST_TOKEN);
    printf("  speed                       - read-only Flash and PSRAM throughput\n");
    printf("  gpio [out in cycles]        - digital loopback; default 8 9 100\n");
    printf("  irq [out in cycles]         - edge interrupt loopback; default 8 9 50\n");
    printf("  pwm <gpio hz duty seconds>  - timed LEDC output for oscilloscope\n");
    printf("  adc <gpio samples>          - raw ADC samples; valid ADC GPIO42-53,55-57\n");
    printf("  uart [tx rx]                - UART loopback; default 8 9; GPIO60 reserved for RGB\n");
    printf("  i2c                         - scan GPIO52 SDA / GPIO53 SCL\n");
    printf("  spi                         - 1 MHz SPI loopback on J2 GPIO20/23/24/25\n");
    printf("  usb                         - Type-C USB Serial/JTAG connection status\n");
    printf("  temp                        - on-die temperature (not board ambient)\n");
    printf("  wifi                        - active 2.4 GHz scan; ANT1 antenna required\n");
    printf("  stress <seconds>            - internal RAM/CPU stress, 1..3600 s\n");
    printf("  light_sleep <seconds>       - timer wake, 1..3600 s\n");
    printf("  deep_sleep <seconds>        - reset after timer wake, 1..3600 s\n");
    printf("  pins                        - print routed-pin safety notes\n");
    printf("  help\n\n");
}

static int tokenize(char *line, char *argv[], int maximum)
{
    int argc = 0;
    char *save = NULL;
    for (char *token = strtok_r(line, " \t\r\n", &save);
         token != NULL && argc < maximum;
         token = strtok_r(NULL, " \t\r\n", &save)) {
        argv[argc++] = token;
    }
    return argc;
}

static void run_command(int argc, char *argv[])
{
    if (argc == 0) {
        return;
    }

    if (strcmp(argv[0], "help") == 0) {
        print_help();
    } else if (strcmp(argv[0], "pins") == 0) {
        print_pin_warning();
    } else if (strcmp(argv[0], "all") == 0 ||
               strcmp(argv[0], "selftest") == 0) {
        run_selftest(NULL, false);
    } else if (strcmp(argv[0], "info") == 0) {
        test_info();
    } else if (strcmp(argv[0], "flash_info") == 0) {
        test_flash_info(NULL);
    } else if (strcmp(argv[0], "flash_rw") == 0) {
        test_flash_rw(argc >= 2 ? argv[1] : NULL);
    } else if (strcmp(argv[0], "speed") == 0) {
        test_flash_speed();
        test_psram_speed();
    } else if (strcmp(argv[0], "gpio") == 0 || strcmp(argv[0], "irq") == 0) {
        int output_pin = DEFAULT_GPIO_OUT;
        int input_pin = DEFAULT_GPIO_IN;
        int cycles = strcmp(argv[0], "gpio") == 0 ? 100 : 50;
        const bool args_ok = (argc == 1) ||
            (argc == 4 && parse_int(argv[1], 0, 61, &output_pin) &&
             parse_int(argv[2], 0, 61, &input_pin) &&
             parse_int(argv[3], 1, 10000, &cycles));
        if (!args_ok) {
            printf("Usage: %s [out_gpio in_gpio cycles]\n", argv[0]);
        } else if (strcmp(argv[0], "gpio") == 0) {
            test_gpio_loopback(output_pin, input_pin, cycles);
        } else {
            test_gpio_irq(output_pin, input_pin, cycles);
        }
    } else if (strcmp(argv[0], "pwm") == 0) {
        int pin = 0;
        int hz = 0;
        int duty = 0;
        int seconds = 0;
        if (argc != 5 || !parse_int(argv[1], 0, 61, &pin) ||
            !parse_int(argv[2], 1, 1000000, &hz) ||
            !parse_int(argv[3], 1, 99, &duty) ||
            !parse_int(argv[4], 1, 300, &seconds)) {
            printf("Usage: pwm <gpio> <1..1000000 Hz> <1..99 pct> <1..300 s>\n");
        } else {
            test_pwm(pin, hz, duty, seconds);
        }
    } else if (strcmp(argv[0], "adc") == 0) {
        int pin = 0;
        int samples = 0;
        if (argc != 3 || !parse_int(argv[1], 42, 57, &pin) ||
            !parse_int(argv[2], 1, 10000, &samples)) {
            printf("Usage: adc <GPIO42..GPIO57> <samples 1..10000>\n");
        } else {
            test_adc(pin, samples);
        }
    } else if (strcmp(argv[0], "uart") == 0) {
        int tx_pin = DEFAULT_GPIO_OUT;
        int rx_pin = DEFAULT_GPIO_IN;
        const bool args_ok = argc == 1 ||
            (argc == 3 && parse_int(argv[1], 0, 61, &tx_pin) &&
             parse_int(argv[2], 0, 61, &rx_pin));
        if (!args_ok) {
            printf("Usage: uart [tx_gpio rx_gpio]\n");
        } else {
            test_uart(tx_pin, rx_pin);
        }
    } else if (strcmp(argv[0], "i2c") == 0) {
        test_i2c();
    } else if (strcmp(argv[0], "spi") == 0) {
        test_spi();
    } else if (strcmp(argv[0], "usb") == 0) {
        test_usb();
    } else if (strcmp(argv[0], "temp") == 0) {
        test_temperature();
    } else if (strcmp(argv[0], "wifi") == 0) {
        test_wifi();
    } else if (strcmp(argv[0], "stress") == 0 ||
               strcmp(argv[0], "light_sleep") == 0 ||
               strcmp(argv[0], "deep_sleep") == 0) {
        int seconds = 0;
        if (argc != 2 || !parse_int(argv[1], 1, 3600, &seconds)) {
            printf("Usage: %s <seconds 1..3600>\n", argv[0]);
        } else if (strcmp(argv[0], "stress") == 0) {
            test_stress(seconds, NULL);
        } else if (strcmp(argv[0], "light_sleep") == 0) {
            test_light_sleep(seconds);
        } else {
            start_deep_sleep(seconds);
        }
    } else {
        printf("Unknown command: %s\n", argv[0]);
        print_help();
    }
}

void app_main(void)
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    esp_log_level_set("*", ESP_LOG_ERROR);

    board_status_t board_status = {0};
    board_status.usb_ok = test_usb();
    board_status.completed_mask |= BOARD_TEST_MASK(BOARD_TEST_USB);
    printf("Starting GPIO54, SDMMC, touch and RGB LCD checks.\n");
    const esp_err_t board_err = board_peripherals_start(&board_status);
    run_selftest(&board_status, true);
    if (board_err == ESP_OK) {
        /* LVGL owns the UI task; GPIO33/34 are now RGB data, so USB is gone. */
        return;
    }
    printf("LCD startup failed (%s); retaining the engineering command loop.\n",
           esp_err_to_name(board_err));
    printf("Type 'help' for fixture-assisted engineering tests.\n");

    char line[160];
    char *argv[8];
    printf("s31-test> ");
    while (true) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        const int argc = tokenize(line, argv, 8);
        if (argc == 0) {
            continue;
        }
        run_command(argc, argv);
        printf("s31-test> ");
    }
}
