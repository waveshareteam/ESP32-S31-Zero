# I²C Scan

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Scan I²C addresses 0x08–0x77 on GPIO52 (SDA) and GPIO53 (SCL) every five seconds.

## Hardware Required

ESP32-S31-Zero and a USB data cable; optionally connect a 3.3 V I²C device to J2 pin 19 (SDA), pin 21 (SCL), and GND with suitable pull-ups.

## How to Run

From this example directory in an ESP-IDF-enabled Ubuntu Bash or Windows PowerShell:

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## Expected Result

The console displays responding addresses, -- for no response, and UU for timeout.

## Troubleshooting

An empty bus can return no addresses; persistent timeouts require checking wiring and pull-ups.
