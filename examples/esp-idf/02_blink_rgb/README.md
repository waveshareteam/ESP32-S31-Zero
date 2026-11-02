# RGB LED

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Cycle the onboard WS2812B RGB LED on GPIO60 through red, green, and blue.

## Hardware Required

ESP32-S31-Zero and a USB data cable.

## How to Run

From this example directory in an ESP-IDF-enabled Ubuntu Bash or Windows PowerShell:

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## Expected Result

Red, green, and blue each remain on for one second, followed by half a second off.

## Troubleshooting

Keep GPIO60 free of external loads; the LED uses GRB data order.
