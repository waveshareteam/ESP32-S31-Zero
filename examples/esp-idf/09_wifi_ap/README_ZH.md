# Wi-Fi SoftAP

[English](README.md) | [简体中文](README_ZH.md)

## 功能

创建 2.4 GHz Wi-Fi 热点，支持设置名称、密码、信道和客户端数量上限。

## 硬件需求

ESP32-S31-Zero、USB 数据线及 Wi-Fi 客户端；天线连接应与板上的天线选择配置匹配。

## 怎么运行

在已配置 ESP-IDF 的 Ubuntu Bash 或 Windows PowerShell 中，进入本示例目录运行：

在 `Example Configuration` 中设置热点名称、密码、信道和客户端上限。

```sh
idf.py --preview set-target esp32s31
idf.py menuconfig
idf.py build flash monitor
```

## 预期现象

客户端可搜索到配置的热点，USB 控制台输出客户端连接和断开日志。

## 常见问题

实际使用前请设置自己的热点密码；示例默认配置仅用于演示。
