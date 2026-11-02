# Wi-Fi Performance

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Provide an interactive Wi-Fi console for TCP/UDP throughput, ping, and link information using Espressif managed command components.

## Hardware Required

ESP32-S31-Zero, a USB data cable, a 2.4 GHz router, and a computer running iperf 2.x; use an antenna connection matching the board antenna selection.

## How to Run

This example uses ESP-IDF 6.2 development APIs and needs a matching development environment.

From this example directory in an ESP-IDF-enabled Linux Bash or Windows PowerShell:

```sh
idf.py --preview set-target esp32s31 build flash monitor
```

Start `iperf -s -i 1` on the computer, then run in the board console (replace the bracketed arguments):

```text
sta_connect <SSID> <PASSWORD>
rf_info
iperf -c <PEER_IP> -i 1 -t 60
```

## Expected Result

The wifi-perf> console accepts help, sta_connect, rf_info, ping, and iperf commands, then reports link data and throughput.

## Troubleshooting

The peer must use iperf 2.x; iperf3 uses an incompatible protocol. These are functional throughput measurements, not TRP/TIS certification measurements.
