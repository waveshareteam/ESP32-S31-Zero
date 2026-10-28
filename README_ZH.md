# Waveshare ESP32-S31-Zero

[English](README.md)

ESP32-S31-Zero 是基于 ESP32-S31 的小型开发板，配备 WS2812B-1010 RGB LED、BOOT/RESET 按键、USB Type-C、外置 SPI Flash、射频连接器，以及 GPIO/LCD/触摸/SD 扩展接口。

- [硬件说明](docs/hardware.md)
- [示例清单与构建入口](docs/examples.md)
- [框架和依赖版本](docs/versions.md)
- [固件下载与烧录](releases/README.md)

<img src="docs/images/ESP32-S31-Zero.webp" alt="Waveshare ESP32-S31-Zero" width="500">


## 产品概览

仓库提供 12 个 ESP-IDF 源码工程、测试二进制及硬件资料。RGB 使用 GPIO60，低有效按键使用 GPIO61；USB Type-C 的 USB Serial/JTAG 使用 GPIO33/34。

## 框架支持

**实验 CI 已覆盖全部 12 个工程：ESP-IDF `6.2.0-dev-08e0d30a74ad`，固定提交 `08e0d30a74ad0bfd5a34933142b80f45619ee410`。** 已明确授权使用该开发版及 ESP32-S31 预览目标。查到的最新 6.x 稳定版为官方 v6.1，但其中 S31 仍为预览目标；v5.5.6 没有该目标，未发现正式 v6.2 标签。版本及实验标记见[集中配置](config/frameworks.json)。2026-10-28 已在本地完整编译通过全部 12 个工程，并验证对应固件包；GitHub 运行和实机测试仍待完成，详见[验证记录](docs/validation.md)。本仓库没有 Arduino 工程。

## 示例

| ESP-IDF 工程 | 功能 |
| --- | --- |
| [01_gpio_output](examples/esp-idf/01_gpio_output/) | GPIO 输出高电平演示 |
| [02_blink_rgb](examples/esp-idf/02_blink_rgb/) | 板载 RGB LED |
| [03_chip_info](examples/esp-idf/03_chip_info/) | 芯片与物理 Flash 信息 |
| [04_boot_button](examples/esp-idf/04_boot_button/) | 低有效 BOOT 按键 |
| [05_uart_loopback](examples/esp-idf/05_uart_loopback/) | 外接跳线 UART 回环 |
| [06_i2c_scan](examples/esp-idf/06_i2c_scan/) | 扩展 I²C 总线扫描 |
| [07_wifi_scan](examples/esp-idf/07_wifi_scan/) | Wi-Fi 扫描 |
| [08_wifi_sta](examples/esp-idf/08_wifi_sta/) | Wi-Fi STA |
| [09_wifi_ap](examples/esp-idf/09_wifi_ap/) | Wi-Fi SoftAP |
| [10_ble_advertiser](examples/esp-idf/10_ble_advertiser/) | BLE 广播 |
| [11_board_test](examples/esp-idf/11_board_test/) | 综合测试 |
| [12_wifi_performance](examples/esp-idf/12_wifi_performance/) | Wi-Fi 吞吐控制台 |

## 使用入口

Ubuntu Bash，无固件编译：

```bash
python3 scripts/discover_examples.py all
python3 scripts/validate_repo.py
python3 -m unittest discover -s tests -v
```

Windows PowerShell：

```powershell
py -3 scripts/discover_examples.py all
py -3 scripts/validate_repo.py
py -3 -m unittest discover -s tests -v
```

使用[构建入口](docs/examples.md)及已批准的固定框架版本。成功构建会生成包含原始分段、紧凑合并镜像、清单、实际依赖锁、校验和及 Linux/Windows 烧录助手的 ZIP。[独立下载脚本](scripts/download_firmware.py) 可列出、选择、校验并安全解压公开 Release 固件，不自动烧录；实际 GitHub 发布仓库由维护者配置。已有[工厂测试镜像](firmware/README.md)单独保留，来源和烧录元数据尚不完整。

## 目录结构

| 路径 | 内容 |
| --- | --- |
| `examples/esp-idf/` | 12 个第一方源码工程 |
| `hardware/` | 原理图及封装文件 |
| `config/` | 硬件记录、框架版本及支持门槛 |
| `docs/` | 硬件、示例、依赖、CI 和验证说明 |
| `scripts/` | 发现器、构建入口、检查和独立下载工具 |
| `tests/` | 主机端工具回归测试 |
| `releases/` | 打包、完整集合校验和发布工具 |
| `firmware/` | 原有测试二进制，不进入源码构建矩阵 |
| `.github/workflows/` | 静态检查及带门槛的固件/发布流水线 |

## 验证与许可

[验证报告](docs/validation.md)分别说明静态测试、依赖解析、编译和实机验证。仅文档修改不触发完整固件构建。正式版本标签必须等待全部构建和包验证成功才可发布；不会覆盖公开版本，详见 [CI 说明](docs/ci.md)。

保留原有 Apache-2.0 [LICENSE](LICENSE)。第三方组件遵守其各自许可；不据此推定硬件设计文件和工厂二进制的独立授权范围。
