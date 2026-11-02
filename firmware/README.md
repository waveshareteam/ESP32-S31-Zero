# Firmware

[English](README.md) | [简体中文](README_ZH.md)

Merged firmware tested on the ESP32-S31-Zero board, flashed at `0x0`. Older versions are kept; the newest is listed first.

| File | Purpose |
| --- | --- |
| [ESP32-S31-Zero_FactoryOnly_20261020.bin](ESP32-S31-Zero_FactoryOnly_20261020.bin) | Factory firmware V1, 2026-10-20, restores the out-of-box demo |

Run from this directory and replace `<PORT>` with the board port. Erasing removes all data in Flash.

Linux Bash / Windows PowerShell:

```sh
python -m pip install esptool
python -m esptool --chip esp32s31 --port "<PORT>" erase-flash
python -m esptool --chip esp32s31 --port "<PORT>" write-flash 0x0 ESP32-S31-Zero_FactoryOnly_20261020.bin
```

If download mode is needed, hold BOOT, press RESET, then release BOOT; press RESET after flashing to run.
