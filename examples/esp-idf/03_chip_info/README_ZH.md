# 芯片信息

[English](README.md) | [简体中文](README_ZH.md)

## 功能

输出芯片型号、核心数、修订号、基础 MAC，以及物理 Flash 容量和配置容量。

## 硬件需求

ESP32-S31-Zero 及 USB 数据线。

## 怎么运行

在已配置 ESP-IDF 的 Linux Bash 或 Windows PowerShell 中，进入本示例目录运行：

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## 预期现象

启动后，USB 控制台显示芯片及 Flash 信息。

## 常见问题

物理 Flash 容量可能与镜像配置容量不同；现有镜像配置为 16 MB。
