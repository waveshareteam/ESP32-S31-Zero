# 固件

[English](README.md) | [简体中文](README_ZH.md)

在 ESP32-S31-Zero 开发板上实测过、从 `0x0` 烧录的合并固件。旧版本保留，新版本排在前面。

| 文件 | 用途 |
| --- | --- |
| [ESP32-S31-Zero_FactoryOnly_20261020.bin](ESP32-S31-Zero_FactoryOnly_20261020.bin) | 出厂固件 V1，2026-10-20，恢复开箱演示 |

在此目录运行；将 `<PORT>` 替换为开发板端口。擦除会清空 Flash 中的所有数据。

Ubuntu Bash / Windows PowerShell：

```sh
python -m pip install esptool
python -m esptool --chip esp32s31 --port <PORT> erase-flash
python -m esptool --chip esp32s31 --port <PORT> write-flash 0x0 ESP32-S31-Zero_FactoryOnly_20261020.bin
```

无法进入下载模式时，按住 BOOT，按一下 RESET，再松开 BOOT；烧录后按 RESET 运行。
