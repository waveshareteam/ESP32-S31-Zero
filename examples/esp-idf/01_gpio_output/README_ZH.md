# GPIO 输出

[English](README.md) | [简体中文](README_ZH.md)

## 功能

将 GPIO0–6、GPIO46–51、GPIO58、GPIO59 设为高电平输出。

## 硬件需求

ESP32-S31-Zero、USB 数据线及万用表或逻辑分析仪。

## 怎么运行

在已配置 ESP-IDF 的 Linux Bash 或 Windows PowerShell 中，进入本示例目录运行：

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## 预期现象

初始化后，所选 15 路 GPIO 保持高电平。

## 常见问题

运行前断开这些引脚上的其他驱动设备；本例将 GPIO58/59 作为输出，不能同时连接 UART 设备。
