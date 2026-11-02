# Chip Information

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Print chip identity, core count, revision, base MAC, and physical and configured Flash sizes.

## Hardware Required

ESP32-S31-Zero and a USB data cable.

## How to Run

From this example directory in an ESP-IDF-enabled Linux Bash or Windows PowerShell:

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## Expected Result

The USB console displays chip and Flash information after boot.

## Troubleshooting

The physical Flash capacity can differ from the configured image capacity; the existing image configuration is 16 MB.
