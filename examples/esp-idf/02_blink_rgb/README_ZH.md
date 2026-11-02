# RGB LED

[English](README.md) | [简体中文](README_ZH.md)

## 功能

驱动 GPIO60 上的板载 WS2812B RGB LED，循环显示红、绿、蓝。

## 硬件需求

ESP32-S31-Zero 及 USB 数据线。

## 怎么运行

在已配置 ESP-IDF 的 Linux Bash 或 Windows PowerShell 中，进入本示例目录运行：

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## 预期现象

红、绿、蓝各亮一秒，随后熄灭半秒，循环运行。

## 常见问题

GPIO60 不应连接其他负载；LED 数据顺序为 GRB。
