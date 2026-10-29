# Dependencies

## Build environment

Use ESP-IDF `6.2.0-dev-08e0d30a74ad` at commit `08e0d30a74ad0bfd5a34933142b80f45619ee410` with ESP-IDF Component Manager **3.1.2**. ESP32-S31 support in this framework is experimental and requires the `esp32s31` preview target. See the [framework configuration](../config/frameworks.json) and [build instructions](examples.md).

Component Manager downloads the declared registry components and their dependencies during project configuration. Examples 01–10 use ESP-IDF components only; examples 11 and 12 also use the components below.

## 11_board_test

The [component manifest](../examples/esp-idf/11_board_test/main/idf_component.yml) declares:

| Component | Required version | Version used in the verified build |
| --- | --- | --- |
| `idf` | `>=6.2.0` | Development revision listed above |
| `espressif/esp_lcd_touch` | `1.2.1` | 1.2.1 |
| `espressif/esp_lcd_touch_gt911` | `1.2.1` | 1.2.1 |
| `espressif/esp_lvgl_adapter` | `0.6.4` | 0.6.4 |
| `lvgl/lvgl` | `9.5.0` | 9.5.0 |

## 12_wifi_performance

The [component manifest](../examples/esp-idf/12_wifi_performance/main/idf_component.yml) declares:

| Component | Required version | Version used in the verified build |
| --- | --- | --- |
| `cmd_system` | Included with ESP-IDF | Development revision listed above |
| `espressif/iperf-cmd` | `~1.0.2` | 1.0.3 |
| `esp-qa/wifi-cmd` | `~0.2.7` | 0.2.12 |
| `esp-qa/ping-cmd` | `~1.0.0` | 1.0.0 |

`cmd_system` comes from `${IDF_PATH}/examples/system/console/advanced/components/cmd_system` in the selected ESP-IDF checkout.

## Reproducing a build

The reference versions above were used for the builds verified on **2026-10-28**. Version ranges and optional component features can change the resolved dependencies. Each generated firmware ZIP includes its build's `dependencies.lock`, which records the complete resolved component set, including transitive dependencies. Preserve that lock and use the same framework commit when reproducing a packaged build.
