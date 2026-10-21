/*
 * Based on Espressif's ESP-IDF Wi-Fi iperf example.
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>

#include "cmd_system.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "iperf.h"
#include "iperf_cmd.h"
#include "nvs_flash.h"
#include "ping_cmd.h"
#include "wifi_cmd.h"

#if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS || CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
#include "esp_wifi_he.h"
#endif

#if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
extern int wifi_cmd_get_tx_statistics(int argc, char **argv);
extern int wifi_cmd_clr_tx_statistics(int argc, char **argv);
#endif

#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
extern int wifi_cmd_get_rx_statistics(int argc, char **argv);
extern int wifi_cmd_clr_rx_statistics(int argc, char **argv);
#endif

static const char *wifi_mode_name(wifi_mode_t mode)
{
    switch (mode) {
    case WIFI_MODE_NULL:
        return "null";
    case WIFI_MODE_STA:
        return "station";
    case WIFI_MODE_AP:
        return "softap";
    case WIFI_MODE_APSTA:
        return "station+softap";
    default:
        return "unknown";
    }
}

static const char *wifi_bandwidth_name(wifi_bandwidth_t bandwidth)
{
    switch (bandwidth) {
    case WIFI_BW20:
        return "20 MHz";
    case WIFI_BW40:
        return "40 MHz";
    case WIFI_BW80:
        return "80 MHz";
    case WIFI_BW160:
        return "160 MHz";
    case WIFI_BW80_BW80:
        return "80+80 MHz";
    default:
        return "unknown";
    }
}

static void print_protocols(uint8_t protocols)
{
    printf("protocols=0x%02x [", protocols);
    if (protocols & WIFI_PROTOCOL_11B) {
        printf(" 11b");
    }
    if (protocols & WIFI_PROTOCOL_11G) {
        printf(" 11g");
    }
    if (protocols & WIFI_PROTOCOL_11N) {
        printf(" 11n");
    }
    if (protocols & WIFI_PROTOCOL_11AX) {
        printf(" 11ax");
    }
    printf(" ]\n");
}

static int rf_info_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK) {
        printf("rf_info: esp_wifi_get_mode failed: %s\n", esp_err_to_name(err));
        return 1;
    }

    const wifi_interface_t interface =
        (mode == WIFI_MODE_AP) ? WIFI_IF_AP : WIFI_IF_STA;
    printf("mode=%s interface=%s\n", wifi_mode_name(mode),
           interface == WIFI_IF_STA ? "station" : "softap");

    uint8_t primary = 0;
    wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
    err = esp_wifi_get_channel(&primary, &secondary);
    if (err == ESP_OK) {
        const char *secondary_name =
            secondary == WIFI_SECOND_CHAN_ABOVE ? "above" :
            secondary == WIFI_SECOND_CHAN_BELOW ? "below" : "none";
        printf("channel=%u secondary=%s\n", primary, secondary_name);
    } else {
        printf("channel=unavailable (%s)\n", esp_err_to_name(err));
    }

    wifi_bandwidth_t bandwidth = WIFI_BW20;
    err = esp_wifi_get_bandwidth(interface, &bandwidth);
    if (err == ESP_OK) {
        printf("bandwidth=%s\n", wifi_bandwidth_name(bandwidth));
    } else {
        printf("bandwidth=unavailable (%s)\n", esp_err_to_name(err));
    }

    uint8_t protocols = 0;
    err = esp_wifi_get_protocol(interface, &protocols);
    if (err == ESP_OK) {
        print_protocols(protocols);
    } else {
        printf("protocols=unavailable (%s)\n", esp_err_to_name(err));
    }

    int8_t max_tx_power = 0;
    err = esp_wifi_get_max_tx_power(&max_tx_power);
    if (err == ESP_OK) {
        printf("max_tx_power_config=%d (unit: 0.25 dBm)\n", max_tx_power);
    } else {
        printf("max_tx_power_config=unavailable (%s)\n", esp_err_to_name(err));
    }

    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
        wifi_ap_record_t ap = {0};
        err = esp_wifi_sta_get_ap_info(&ap);
        if (err == ESP_OK) {
            printf("ssid=%.*s bssid=%02x:%02x:%02x:%02x:%02x:%02x rssi=%d dBm\n",
                   (int)sizeof(ap.ssid), (const char *)ap.ssid,
                   ap.bssid[0], ap.bssid[1], ap.bssid[2],
                   ap.bssid[3], ap.bssid[4], ap.bssid[5], ap.rssi);
        } else {
            printf("station_link=not connected (%s)\n", esp_err_to_name(err));
        }
    }

    printf("Note: configured TX power is not a conducted or radiated power measurement.\n");
    return 0;
}

static void register_rf_info(void)
{
    const esp_console_cmd_t command = {
        .command = "rf_info",
        .help = "Show Wi-Fi mode, channel, bandwidth, protocols, RSSI and TX power setting",
        .hint = NULL,
        .func = rf_info_command,
        .argtable = NULL,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&command));
}

static void iperf_stats_hook(iperf_id_t instance_id,
                             iperf_state_data_t *data,
                             void *private_data)
{
    (void)instance_id;
    (void)private_data;

    if (data->state == IPERF_STARTED) {
#if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
        if (data->traffic_type != IPERF_UDP_SERVER) {
            wifi_cmd_clr_tx_statistics(0, NULL);
        }
#endif
#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
        if (data->traffic_type != IPERF_UDP_CLIENT) {
            wifi_cmd_clr_rx_statistics(0, NULL);
        }
#endif
    }

    if (data->state == IPERF_STOPPED) {
#if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
        if (data->traffic_type != IPERF_UDP_SERVER) {
            wifi_cmd_get_tx_statistics(0, NULL);
        }
#endif
#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
        if (data->traffic_type != IPERF_UDP_CLIENT) {
            wifi_cmd_get_rx_statistics(0, NULL);
        }
#endif
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_cmd_initialize_wifi(NULL);
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_MU_STATS
    esp_wifi_enable_rx_statistics(true, true);
#else
    esp_wifi_enable_rx_statistics(true, false);
#endif
#endif
#if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
    esp_wifi_enable_tx_statistics(ESP_WIFI_ACI_BE, true);
#endif

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "wifi-perf>";

    esp_console_dev_usb_serial_jtag_config_t usb_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(
        &usb_config, &repl_config, &repl));

    register_system();
    wifi_cmd_register_all();
    iperf_cmd_register_iperf();
    iperf_cmd_set_iperf_state_handler(iperf_stats_hook, NULL);
    ping_cmd_register_ping();
    register_rf_info();

    printf("\nESP32-S31-Zero Wi-Fi performance console\n");
    printf("Connect a suitable 2.4 GHz antenna to ANT1 before testing.\n");
    printf("Use 'help', then 'sta_connect <ssid> <password>' and 'rf_info'.\n");
    printf("Use iperf 2.x on the peer; this console is not iperf3 compatible.\n");
    printf("This firmware is for OTA throughput pre-checks, not formal TRP/TIS or spurious certification.\n\n");

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
