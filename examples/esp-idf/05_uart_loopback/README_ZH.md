# UART 回环

[English](README.md) | [简体中文](README_ZH.md)

## 功能

使用 UART1 以 115200 波特率每秒发送并校验一次测试消息。

## 硬件需求

ESP32-S31-Zero、USB 数据线，以及连接 GPIO58（TX，H2 第 2 脚）和 GPIO59（RX，H2 第 1 脚）的跳线。

## 怎么运行

在已配置 ESP-IDF 的 Ubuntu Bash 或 Windows PowerShell 中，进入本示例目录运行：

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## 预期现象

回读数据一致时 USB 控制台输出 PASS，否则输出 FAIL。

## 常见问题

断开 GPIO58/59 上的其他设备；本例将 UART1 映射到这两个引脚，原理图上的 UART0 标注为其默认功能。
