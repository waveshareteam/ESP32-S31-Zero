# Waveshare ESP32-S31-Zero

[中文](README_ZH.md)

ESP32-S31-Zero is a compact development board built around ESP32-S31, featuring a WS2812B-1010 RGB LED, BOOT/RESET buttons, USB Type-C, external SPI Flash, an RF connector, and GPIO/LCD/touch/SD expansion interfaces.

- [Hardware information](docs/hardware.md)
- [Examples and build entry](docs/examples.md)
- [Framework and dependency support](docs/versions.md)
- [Firmware download and flashing](releases/README.md)

<img src="docs/images/ESP32-S31-Zero.webp" alt="Waveshare ESP32-S31-Zero" width="500">

## Overview

The repository provides 12 ESP-IDF source projects, a test binary, and hardware resources. GPIO60 drives the RGB LED and GPIO61 reads the active-low button. USB Type-C uses GPIO33/34 for USB Serial/JTAG.

## Framework support

**The experimental CI matrix covers all 12 projects using ESP-IDF `6.2.0-dev-08e0d30a74ad`, pinned to commit `08e0d30a74ad0bfd5a34933142b80f45619ee410`.** This development framework and its ESP32-S31 preview target are explicitly approved. Official v6.1 is the latest stable 6.x release found in this audit, but ESP32-S31 remains a preview target; v5.5.6 does not include it and no stable v6.2 tag was found. See the [central configuration](config/frameworks.json). All 12 projects compiled locally and their firmware packages passed validation on 2026-10-28. GitHub execution and physical-board testing remain pending; see the [validation record](docs/validation.md). No Arduino project is present.

## Examples

| ESP-IDF project | Purpose |
| --- | --- |
| [01_gpio_output](examples/esp-idf/01_gpio_output/) | GPIO output-high demonstration |
| [02_blink_rgb](examples/esp-idf/02_blink_rgb/) | Onboard RGB LED |
| [03_chip_info](examples/esp-idf/03_chip_info/) | Chip and physical Flash information |
| [04_boot_button](examples/esp-idf/04_boot_button/) | Active-low boot button |
| [05_uart_loopback](examples/esp-idf/05_uart_loopback/) | UART loopback with jumper |
| [06_i2c_scan](examples/esp-idf/06_i2c_scan/) | External I²C bus scan |
| [07_wifi_scan](examples/esp-idf/07_wifi_scan/) | Wi-Fi scan |
| [08_wifi_sta](examples/esp-idf/08_wifi_sta/) | Wi-Fi station |
| [09_wifi_ap](examples/esp-idf/09_wifi_ap/) | Wi-Fi SoftAP |
| [10_ble_advertiser](examples/esp-idf/10_ble_advertiser/) | BLE advertiser |
| [11_board_test](examples/esp-idf/11_board_test/) | Comprehensive test |
| [12_wifi_performance](examples/esp-idf/12_wifi_performance/) | Wi-Fi throughput console |

## Getting started

Run discovery without building (Ubuntu Bash):

```bash
python3 scripts/discover_examples.py all
python3 scripts/validate_repo.py
python3 -m unittest discover -s tests -v
```

Windows PowerShell:

```powershell
py -3 scripts/discover_examples.py all
py -3 scripts/validate_repo.py
py -3 -m unittest discover -s tests -v
```

Use the pinned framework and [build entry](docs/examples.md). Successful source builds produce ZIPs with original binaries, a compact combined image, manifest, resolved dependencies, checksums and Bash/Windows helpers. The [standalone downloader](scripts/download_firmware.py) lists and verifies public Release firmware; it never flashes automatically. A GitHub release destination must be configured by the maintainer. The preserved [factory binary](firmware/README.md) is separate and has incomplete provenance.

## Repository layout

| Path | Contents |
| --- | --- |
| `examples/esp-idf/` | 12 first-party source projects |
| `hardware/` | Supplied schematic and footprints |
| `config/` | Target evidence and framework gate |
| `docs/` | Hardware, examples, versions and validation |
| `scripts/` | Discovery, build entry, static checks and standalone download |
| `tests/` | Host tooling regression tests |
| `releases/` | Packaging, aggregate validation and publication tooling |
| `firmware/` | Preserved test image; excluded from source CI |
| `.github/workflows/` | Static checks and guarded firmware/Release pipeline |

## Validation and license

[Validation report](docs/validation.md) distinguishes static tests, dependency resolution, compilation and physical-board testing. Documentation-only changes do not trigger full firmware builds. A stable tag can publish only after the complete required matrix and package gates succeed; public versions are never replaced. See [CI](docs/ci.md).

The supplied [LICENSE](LICENSE) is Apache-2.0 and is preserved. Third-party components retain their own licenses; separate licensing of supplied hardware design files and factory binaries is not inferred.
