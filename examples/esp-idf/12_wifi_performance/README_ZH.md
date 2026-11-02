# Wi-Fi 性能测试

[English](README.md) | [简体中文](README_ZH.md)

## 功能

使用乐鑫托管命令组件提供交互式 Wi-Fi 控制台，进行 TCP/UDP 吞吐、ping 及链路信息测试。

## 硬件需求

ESP32-S31-Zero、USB 数据线、2.4 GHz 路由器，以及运行 iperf 2.x 的计算机；天线连接应与板上的天线选择配置匹配。

## 怎么运行

在已配置 ESP-IDF 的 Linux Bash 或 Windows PowerShell 中，进入本示例目录运行：

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

计算机先运行 `iperf -s -i 1`，再在开发板控制台依次运行（替换尖括号参数）：

```text
sta_connect <SSID> <PASSWORD>
rf_info
iperf -c <PEER_IP> -i 1 -t 60
```

## 预期现象

wifi-perf> 控制台接受 help、sta_connect、rf_info、ping 和 iperf 等命令，并输出链路信息和吞吐数据。

## 常见问题

对端必须使用 iperf 2.x，iperf3 的协议不兼容。这些结果用于功能和吞吐评估，不能替代 TRP/TIS 认证测试。
