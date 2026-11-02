# BLE Advertiser

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Advertise the BLE device name ESP32-S31-Zero and accept a central connection.

## Hardware Required

ESP32-S31-Zero, a USB data cable, and a phone or other BLE central; use an antenna connection matching the board antenna selection.

## How to Run

From this example directory in an ESP-IDF-enabled Linux Bash or Windows PowerShell:

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## Expected Result

The central discovers ESP32-S31-Zero, and the USB console logs connection and disconnection events.

## Troubleshooting

Use a BLE scanner rather than a classic Bluetooth device search, and check the antenna path if discovery fails.
