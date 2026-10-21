# Example inventory and build entry

All paths below are repository-relative. There were **12 existing source projects**, **0 newly added source projects**, and **12 maintained projects after normalization**. Existing generated `sdkconfig` files remain on disk and are ignored. No third-party example tree or backup project is included. The factory binary is excluded.

| Original name | Maintained name | Purpose / equipment |
| --- | --- | --- |
| 01_gpio_loopback | [01_gpio_output](../examples/esp-idf/01_gpio_output/) | 15 exposed GPIOs driven high; not loopback |
| 02_blinkrgb | [02_blink_rgb](../examples/esp-idf/02_blink_rgb/) | GPIO60 WS2812B-1010 RGB cycle |
| 03_chipinfo | [03_chip_info](../examples/esp-idf/03_chip_info/) | Chip identity and physical Flash capacity |
| 04_bootbutton | [04_boot_button](../examples/esp-idf/04_boot_button/) | GPIO61 active-low button |
| 05_uart_loopback | [05_uart_loopback](../examples/esp-idf/05_uart_loopback/) | Jumper J2 pin 8 to pin 10 |
| 06_i2c_scan | [06_i2c_scan](../examples/esp-idf/06_i2c_scan/) | External bus, SDA J2/19, SCL J2/21; zero devices is valid |
| 07_wifi_scan | [07_wifi_scan](../examples/esp-idf/07_wifi_scan/) | External antenna; AP scanning |
| 08_wifi_sta | [08_wifi_sta](../examples/esp-idf/08_wifi_sta/) | External antenna; configure local SSID/password |
| 09_wifi_ap | [09_wifi_ap](../examples/esp-idf/09_wifi_ap/) | External antenna; demonstration SoftAP |
| 10_ble_advertiser | [10_ble_advertiser](../examples/esp-idf/10_ble_advertiser/) | External antenna; BLE central for observation |
| 11_board_test | [11_board_test](../examples/esp-idf/11_board_test/) | Existing external LCD/touch/SD/LED fixture and PSRAM assumptions; see hardware limitations |
| 12_wifi_performance | [12_wifi_performance](../examples/esp-idf/12_wifi_performance/) | Existing interactive iperf 2.x test, antenna and peer required |

Directory names and CMake `project()` names match. Discovery is deliberately limited to direct projects under `examples/esp-idf`; nested library examples, downloaded dependencies and factory images do not enter the matrix. Duplicate identifiers or numbers, unknown selectors, and an empty project/matrix set fail explicitly.

## Discovery (no build)

Ubuntu Bash:

```bash
python3 scripts/discover_examples.py all
python3 scripts/discover_examples.py 02_blink_rgb
python3 scripts/discover_examples.py examples/esp-idf/02_blink_rgb
python3 scripts/discover_examples.py all --matrix
```

Windows PowerShell:

```powershell
py -3 scripts/discover_examples.py all
py -3 scripts/discover_examples.py examples/esp-idf/02_blink_rgb
```

## Build with the approved experimental framework

Read [versions](versions.md) and activate the exact approved ESP-IDF checkout/environment. CI also installs the centrally pinned Component Manager; for local use, run `python -m pip install "idf-component-manager==3.1.2"` in that environment (Ubuntu Bash or Windows PowerShell). The approved version is `6.2.0-dev-08e0d30a74ad` at commit `08e0d30a74ad0bfd5a34933142b80f45619ee410`. The build entry checks this exact commit and enables the required `--preview` flag. The matrix is experimental; no moving development branch is substituted.

Ubuntu Bash:

```bash
python3 scripts/build_example.py 02_blink_rgb --version 6.2.0-dev-08e0d30a74ad --configure-only
python3 scripts/build_example.py 02_blink_rgb --version 6.2.0-dev-08e0d30a74ad
```

Windows PowerShell (ESP-IDF PowerShell environment):

```powershell
python scripts/build_example.py 02_blink_rgb --version 6.2.0-dev-08e0d30a74ad --configure-only
python scripts/build_example.py 02_blink_rgb --version 6.2.0-dev-08e0d30a74ad
```

`--configure-only` resolves/configures dependencies; it is not a compilation. A normal invocation builds, never flashes. Preserve an existing `sdkconfig`, lock file and build directory before switching frameworks; use a fresh source copy or explicitly relocate local generated files. CI starts from a clean checkout. Do not publish credentials set through `menuconfig`.
