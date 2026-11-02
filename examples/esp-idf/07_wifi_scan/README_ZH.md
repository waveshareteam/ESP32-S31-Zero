# Wi-Fi 扫描

[English](README.md) | [简体中文](README_ZH.md)

## 功能

每十秒扫描一次附近的 2.4 GHz Wi-Fi 网络。

## 硬件需求

ESP32-S31-Zero、USB 数据线及附近的 2.4 GHz 热点；天线连接应与板上的天线选择配置匹配。

## 怎么运行

在已配置 ESP-IDF 的 Linux Bash 或 Windows PowerShell 中，进入本示例目录运行：

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## 预期现象

控制台显示最多 20 个网络的 SSID、信道、RSSI 和安全模式。

## 常见问题

扫描不到网络时，请检查天线通路及附近是否有 2.4 GHz 网络。
