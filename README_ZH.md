<div align="center">
  <h1>ESP32-S31-Zero</h1>
  <p><strong>小型 ESP32-S31 无线开发板</strong></p>
  <p><a href="README.md">English</a> | <strong>简体中文</strong></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-S31-Zero/actions/workflows/build.yml"><img alt="Build" src="https://github.com/waveshareteam/ESP32-S31-Zero/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-S31-Zero/releases/latest"><img alt="Latest Release" src="https://img.shields.io/github/v/release/waveshareteam/ESP32-S31-Zero"></a>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-S31-Zero"></a>
  </p>
  <p>
    <a href="https://docs.waveshare.net/ESP32-S31-Zero/">🌐 产品</a> &middot;
    <a href="https://github.com/waveshareteam/ESP32-S31-Zero/releases/latest">📦 固件</a> &middot;
    <a href="examples/esp-idf/">🧩 ESP-IDF</a> &middot;
    <a href="hardware/">📚 资料</a>
  </p>
  <p><img src="docs/images/ESP32-S31-Zero.webp" alt="ESP32-S31-Zero development board" width="500"></p>
</div>

---

## ✨ 概述

ESP32-S31-Zero 是基于 ESP32-S31 的小型无线开发板，配备 USB Type-C、RGB LED、BOOT/RESET 按键及 GPIO/LCD/触摸/SD 扩展接口。仓库提供 ESP-IDF 示例、出厂固件和硬件资料。

| 器件 | 规格 / 接口 |
| --- | --- |
| 主控 | ESP32-S31，双核 RISC-V，最高 320 MHz |
| 存储 | 16 / 32 MB Flash，16 / 32 MB PSRAM，见下表 |
| 无线 | 2.4 GHz Wi-Fi 6、Bluetooth LE |
| RGB LED | WS2812B-1010，GPIO60 |
| 按键 | BOOT（GPIO61，低有效）、RESET |
| USB | Type-C，USB Serial/JTAG（GPIO33/34） |
| 扩展 | GPIO、RGB LCD、触摸、SD 接口 |

型号存储组合如下，`-M` 为排针版；Flash 和 PSRAM 容量单位均为 MB。

| SKU | 型号 | Flash | PSRAM |
| --- | --- | ---: | ---: |
| 35879 | ESP32-S31-Zero-N16R16 | 16 | 16 |
| 36165 | ESP32-S31-Zero-N16R16-M | 16 | 16 |
| 35880 | ESP32-S31-Zero-N32R16 | 32 | 16 |
| 36166 | ESP32-S31-Zero-N32R16-M | 32 | 16 |
| 35121 | ESP32-S31-Zero-N16R32 | 16 | 32 |
| 36167 | ESP32-S31-Zero-N16R32-M | 16 | 32 |
| 35763 | ESP32-S31-Zero-N32R32 | 32 | 32 |
| 36168 | ESP32-S31-Zero-N32R32-M | 32 | 32 |

## 📦 快速开始

使用现成固件时，先查看 [Releases](https://github.com/waveshareteam/ESP32-S31-Zero/releases) 是否已有所需示例的 `*-combined.zip`；源码运行方法见各示例说明。

1. 下载对应示例和框架版本的 `*-combined.zip`。
2. 解压，在 Linux Bash 或 Windows PowerShell 运行 `python -m pip install esptool`。
3. 用支持数据传输的 USB 线连接开发板。
4. 将 `<PORT>` 替换为开发板端口，在 Windows PowerShell 运行 `.\flash_combined.bat "<PORT>"`，或在 Linux Bash 运行 `./flash_combined.sh "<PORT>"`。
5. 若未自动运行，按 RESET 复位。

恢复出厂演示：下载 [`firmware`](firmware/) 中的 `ESP32-S31-Zero_FactoryOnly_20261020.bin`，按[烧录说明](firmware/README_ZH.md)操作。

## 🧪 示例

### ESP-IDF

| 示例 | 主要功能 |
| --- | --- |
| [01_gpio_output](examples/esp-idf/01_gpio_output/README_ZH.md) | 15 路 GPIO 输出高电平 |
| [02_blink_rgb](examples/esp-idf/02_blink_rgb/README_ZH.md) | 板载 RGB LED 颜色循环 |
| [03_chip_info](examples/esp-idf/03_chip_info/README_ZH.md) | 芯片与 Flash 信息 |
| [04_boot_button](examples/esp-idf/04_boot_button/README_ZH.md) | BOOT 按键输入 |
| [05_uart_loopback](examples/esp-idf/05_uart_loopback/README_ZH.md) | 外接跳线 UART 回环 |
| [06_i2c_scan](examples/esp-idf/06_i2c_scan/README_ZH.md) | 外接 I²C 设备扫描 |
| [07_wifi_scan](examples/esp-idf/07_wifi_scan/README_ZH.md) | 扫描附近 Wi-Fi 网络 |
| [08_wifi_sta](examples/esp-idf/08_wifi_sta/README_ZH.md) | 连接 Wi-Fi 网络 |
| [09_wifi_ap](examples/esp-idf/09_wifi_ap/README_ZH.md) | 创建 Wi-Fi 热点 |
| [10_ble_advertiser](examples/esp-idf/10_ble_advertiser/README_ZH.md) | BLE 广播与连接 |
| [11_board_test](examples/esp-idf/11_board_test/README_ZH.md) | LCD/触摸/SD 扩展测试 |
| [12_wifi_performance](examples/esp-idf/12_wifi_performance/README_ZH.md) | Wi-Fi 吞吐测试控制台 |

## 🛠️ 开发环境

| 框架 | 配置版本 | 示例数 |
| --- | --- | ---: |
| ESP-IDF | `v6.1`（ESP32-S31 预览目标） | 12 |

ESP32-S31 为预览目标，请按各示例使用 `idf.py --preview`。现有示例统一采用 16 MB Flash 镜像与分区设置，可用于 16 MB 和 32 MB Flash 型号。

## 📚 资料

- [原理图](hardware/schematic/)
- [封装文件](hardware/footprint/)
- [产品说明与 Wiki](https://docs.waveshare.net/ESP32-S31-Zero/)

## 🤝 支持

- 产品使用、硬件与订单问题：[微雪技术支持](https://docs.waveshare.net/ESP32-S31-Zero/Technical-Support/)。

## 📄 许可证

Apache License 2.0，见 [LICENSE](LICENSE)。
