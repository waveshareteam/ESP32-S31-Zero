# Wi-Fi STA

[English](README.md) | [简体中文](README_ZH.md)

## 功能

使用 Example Configuration 中设置的网络信息，将开发板连接到 Wi-Fi 网络。

## 硬件需求

ESP32-S31-Zero、USB 数据线及 2.4 GHz 热点；天线连接应与板上的天线选择配置匹配。

## 怎么运行

在已配置 ESP-IDF 的 Linux Bash 或 Windows PowerShell 中，进入本示例目录运行：

在 `Example Configuration` 中设置要连接的网络名称和密码。

```sh
idf.py --preview set-target esp32s31
idf.py menuconfig
idf.py build flash monitor
```

## 预期现象

连接成功后，控制台输出分配到的 IPv4 地址。

## 常见问题

重连失败十次后应用停止重试；请检查配置的网络名称和密码。
