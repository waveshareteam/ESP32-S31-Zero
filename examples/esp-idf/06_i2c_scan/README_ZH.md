# I²C 扫描

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 GPIO52（SDA）和 GPIO53（SCL），每五秒扫描一次 0x08–0x77 的 I²C 地址。

## 硬件需求

ESP32-S31-Zero 及 USB 数据线；可外接 3.3 V I²C 设备到 J2 第 19 脚（SDA）、第 21 脚（SCL）和 GND，并配置合适的上拉。

## 怎么运行

在已配置 ESP-IDF 的 Linux Bash 或 Windows PowerShell 中，进入本示例目录运行：

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## 预期现象

控制台显示应答地址，无应答显示 --，超时显示 UU。

## 常见问题

未接设备时扫描不到地址属于正常现象；持续超时应检查接线和上拉。
