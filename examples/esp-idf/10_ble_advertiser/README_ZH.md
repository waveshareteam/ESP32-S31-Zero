# BLE 广播

[English](README.md) | [简体中文](README_ZH.md)

## 功能

以 ESP32-S31-Zero 为 BLE 设备名广播，并接受中心设备连接。

## 硬件需求

ESP32-S31-Zero、USB 数据线及手机等 BLE 中心设备；天线连接应与板上的天线选择配置匹配。

## 怎么运行

在已配置 ESP-IDF 的 Linux Bash 或 Windows PowerShell 中，进入本示例目录运行：

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## 预期现象

中心设备可发现 ESP32-S31-Zero，USB 控制台输出连接和断开事件。

## 常见问题

请使用 BLE 扫描工具；发现失败时检查天线通路。
