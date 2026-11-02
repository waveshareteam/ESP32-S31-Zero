# BOOT Button

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Read the active-low BOOT button on GPIO61 with 20 ms debounce.

## Hardware Required

ESP32-S31-Zero and a USB data cable.

## How to Run

From this example directory in an ESP-IDF-enabled Ubuntu Bash or Windows PowerShell:

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## Expected Result

The USB console prints PRESSED and RELEASED when the button changes state.

## Troubleshooting

Press BOOT after the application starts; holding it during reset enters download mode.
