# Board Test

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Run automatic chip, Flash, PSRAM, temperature, Wi-Fi, USB, CPU/RAM, and fixture peripheral tests with LCD status tiles.

## Hardware Required

ESP32-S31-Zero, USB data cable, and an external 800 × 480 RGB LCD / GT911 touch / SD / status-LED fixture matching [the pin definitions](main/board_peripherals.h). The LCD uses PSRAM; the SD interface uses GPIO20–25 and external pull-ups, touch uses GPIO52/53 with INT GPIO7 and RESET GPIO38, and the fixture LED uses GPIO54.

## How to Run

The dependency manifest requires ESP-IDF ≥6.2.0, which excludes the configured v6.1; use a matching 6.2 development environment.

From this example directory in an ESP-IDF-enabled Linux Bash or Windows PowerShell:

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

## Expected Result

The LCD shows PASS/FAIL results while the onboard RGB LED cycles red, green, and blue once per second. The SD check writes, verifies, and deletes a temporary file without formatting the card; continuous touch tracking is disabled by default.

## Troubleshooting

LCD startup reassigns GPIO33/34, ending USB Serial/JTAG output; read subsequent results on the screen. Flash writes use the dedicated testdata partition, and continuous touch tracking can be enabled under Board test in menuconfig.
