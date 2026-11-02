# BOOT 按键

[English](README.md) | [简体中文](README_ZH.md)

## 功能

读取 GPIO61 上的低有效 BOOT 按键，并进行 20 ms 消抖。

## 硬件需求

ESP32-S31-Zero 及 USB 数据线。

## 怎么运行

在已配置 ESP-IDF 的 Ubuntu Bash 或 Windows PowerShell 中，进入本示例目录运行：

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## 预期现象

按键状态变化时，USB 控制台输出 PRESSED 和 RELEASED。

## 常见问题

请在应用启动后操作 BOOT；复位时按住 BOOT 会进入下载模式。
