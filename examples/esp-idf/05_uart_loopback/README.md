# UART Loopback

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Send and verify a test message once per second over UART1 at 115200 baud.

## Hardware Required

ESP32-S31-Zero, a USB data cable, and a jumper joining GPIO58 (TX, H2 pin 2) to GPIO59 (RX, H2 pin 1).

## How to Run

From this example directory in an ESP-IDF-enabled Ubuntu Bash or Windows PowerShell:

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## Expected Result

The USB console prints PASS when the received data matches, or FAIL when it does not.

## Troubleshooting

Disconnect other devices from GPIO58/59; this example maps UART1 onto them even though the schematic labels their default UART0 functions.
