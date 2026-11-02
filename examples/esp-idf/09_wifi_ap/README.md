# Wi-Fi SoftAP

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Create a 2.4 GHz Wi-Fi access point with configurable name, password, channel, and client limit.

## Hardware Required

ESP32-S31-Zero, a USB data cable, and a Wi-Fi client; use an antenna connection matching the board antenna selection.

## How to Run

From this example directory in an ESP-IDF-enabled Ubuntu Bash or Windows PowerShell:

In `Example Configuration`, set the AP name, password, channel, and client limit.

```sh
idf.py --preview set-target esp32s31
idf.py menuconfig
idf.py build flash monitor
```

## Expected Result

The configured hotspot appears on the client, and the USB console logs client connections and disconnections.

## Troubleshooting

Set your own access-point password before deployment; the example defaults are intended for demonstrations.
