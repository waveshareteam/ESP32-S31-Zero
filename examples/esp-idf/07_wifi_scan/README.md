# Wi-Fi Scan

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Scan nearby 2.4 GHz Wi-Fi networks every ten seconds.

## Hardware Required

ESP32-S31-Zero, a USB data cable, and nearby 2.4 GHz access points; use an antenna connection matching the board antenna selection.

## How to Run

From this example directory in an ESP-IDF-enabled Linux Bash or Windows PowerShell:

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## Expected Result

The console lists up to 20 networks with SSID, channel, RSSI, and security mode.

## Troubleshooting

If no networks appear, check the antenna path and the availability of 2.4 GHz networks.
