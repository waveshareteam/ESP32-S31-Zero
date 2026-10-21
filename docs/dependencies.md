# Resolved dependency audit

Framework: `6.2.0-dev-08e0d30a74ad`, commit `08e0d30a74ad0bfd5a34933142b80f45619ee410`.
Target: `esp32s31`. Resolver: **ESP-IDF Component Manager 3.1.2**. Audit date: 2026-10-21.

All **12 projects passed independent first-pass dependency resolution and download** in fresh temporary source copies. The resolver used the pinned SDK's Component Manager interface version 5 and its declared IDF version 6.2.0. That internal version is not a claim that 6.2 is a stable release. The original local SDK configurations were excluded from the copies and preserved unchanged.

This validates component manifests, version solving and retrieval for the selected target. It does **not** validate the complete CMake component graph, the post-Kconfig dependency pass, C/C++ compilation, linking or board behavior. CI must still perform full configuration and build, and package its own final resolved lock. Audit locks and downloaded components remain outside the repository; the table is an observation, not a hand-written lock or an additional version constraint.

## Examples 01–10

Each resolved only the framework dependency `idf: 6.2.0`; no managed third-party component was required by its main manifest. Framework CMake dependencies are checked later by real configuration/build.

## 11_board_test

| Dependency | Resolved version |
| --- | --- |
| `espressif/button` | 4.2.2 |
| `espressif/cmake_utilities` | 0.5.3 |
| `espressif/esp_lcd_touch` | 1.2.1 |
| `espressif/esp_lcd_touch_gt911` | 1.2.1 |
| `espressif/esp_lv_decoder` | 0.4.3 |
| `espressif/esp_lv_fs` | 1.0.1 |
| `espressif/esp_lvgl_adapter` | 0.6.4 |
| `espressif/esp_mmap_assets` | 2.0.1 |
| `espressif/esp_new_jpeg` | 1.0.2 |
| `espressif/freetype` | 2.14.3~1 |
| `espressif/knob` | 1.1.0 |
| `espressif/libpng` | 1.6.58~2 |
| `espressif/zlib` | 1.3.2~1 |
| `idf` | 6.2.0 |
| `lvgl/lvgl` | 9.5.0 |

## 12_wifi_performance

| Dependency | Resolved version |
| --- | --- |
| `cmd_system` | Local component from the pinned IDF checkout (lock version `*`) |
| `esp-qa/ping-cmd` | 1.0.0 |
| `esp-qa/wifi-cmd` | 0.2.12 |
| `espressif/iperf` | 1.0.3 |
| `espressif/iperf-cmd` | 1.0.3 |
| `idf` | 6.2.0 |

`cmd_system` is sourced from `${IDF_PATH}/examples/system/console/advanced/components/cmd_system`; the framework commit pins that local component. Optional adapter features may change the dependency graph during the complete Kconfig-aware configure pass. Preserve the resulting release archive's actual `dependencies.lock` for reproduction.
