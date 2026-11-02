# 板级测试

[English](README.md) | [简体中文](README_ZH.md)

## 功能

自动检查芯片、Flash、PSRAM、温度、Wi-Fi、USB、CPU/RAM 及扩展外设，并在 LCD 显示各项状态。

## 硬件需求

ESP32-S31-Zero、USB 数据线及与[引脚定义](main/board_peripherals.c)一致的外接 800 × 480 RGB LCD / GT911 触摸 / SD / 状态灯夹具。LCD 使用 PSRAM，SD 使用 GPIO20–25 并需外接上拉，触摸使用 GPIO52/53、INT GPIO7 和 RESET GPIO38，夹具状态灯使用 GPIO54。

## 怎么运行

在已配置 ESP-IDF 的 Linux Bash 或 Windows PowerShell 中，进入本示例目录运行：

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## 预期现象

LCD 显示 PASS/FAIL 结果，板载 RGB LED 按红、绿、蓝每秒切换。SD 测试写入、校验并删除临时文件，不格式化卡；默认不启用持续触摸跟踪。

## 常见问题

LCD 启动后 GPIO33/34 转作屏幕信号，USB Serial/JTAG 输出随之结束，后续结果请看屏幕。Flash 写入使用专用 testdata 分区；持续触摸跟踪可在 menuconfig 的 Board test 中启用。
