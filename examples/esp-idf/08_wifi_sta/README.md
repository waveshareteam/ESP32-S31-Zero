# Wi-Fi Station

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Connect the board to a Wi-Fi network using the credentials in Example Configuration.

## Hardware Required

ESP32-S31-Zero, a USB data cable, and a 2.4 GHz access point; use an antenna connection matching the board antenna selection.

## How to Run

From this example directory in an ESP-IDF-enabled Linux Bash or Windows PowerShell:

In `Example Configuration`, set the network name and password.

```sh
idf.py --preview set-target esp32s31
idf.py menuconfig
idf.py build flash monitor
```

## Expected Result

The console prints the assigned IPv4 address after a successful connection.

## Troubleshooting

The application stops retrying after ten failed reconnect attempts; check the configured network name and password.
