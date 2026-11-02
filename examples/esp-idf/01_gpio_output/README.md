# GPIO Output

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Drive GPIO0–6, GPIO46–51, GPIO58, and GPIO59 high; this is an output demonstration.

## Hardware Required

ESP32-S31-Zero, a USB data cable, and a multimeter or logic analyzer.

## How to Run

From this example directory in an ESP-IDF-enabled Linux Bash or Windows PowerShell:

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## Expected Result

The 15 selected GPIOs remain high after initialization.

## Troubleshooting

Disconnect other drivers from these pins before running; GPIO58/59 are outputs here and cannot simultaneously serve a UART device.
