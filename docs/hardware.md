# ESP32-S31-Zero Hardware

ESP32-S31-Zero is a compact development board with an ESP32-S31 processor, external SPI Flash, an RGB LED, BOOT/RESET buttons, USB Type-C, an antenna connector, and GPIO and peripheral expansion interfaces.

## Hardware overview

| Item | Description |
| --- | --- |
| Processor | ESP32-S31 |
| Flash | GD25Q256EYIGR, 256 Mbit (32 MiB), SPI interface |
| Crystal | 40 MHz |
| RGB LED | One WS2812B-1010 addressable RGB LED, controlled by GPIO60 |
| BOOT button | GPIO61, active low |
| RESET button | Connected to EN/CHIP_PU, active low |
| USB Type-C | Power input and USB Serial/JTAG; D− on GPIO33, D+ on GPIO34 |
| Power supply | USB 5 V input with an MP1605GTF-Z regulator supplying the 3.3 V rail |
| Antenna connector | ANT1, connected to the processor's RF matching network |
| Expansion interfaces | GPIO, UART, I²C, RGB LCD, touch and SD signals |

## GPIO and UART header

H2 exposes GPIO0–6, GPIO46–51, GPIO58 and GPIO59, together with 3.3 V, 5 V and GND.

| H2 pin | Signal |
| --- | --- |
| 1 | GPIO59 / TXD0 |
| 2 | GPIO58 / RXD0 |

## Peripheral expansion connector

J2 provides the following peripheral signals:

| Interface | Signals and GPIOs |
| --- | --- |
| I²C | SDA: GPIO52 (J2 pin 19); SCL: GPIO53 (J2 pin 21) |
| Touch control | INT: GPIO7 (J2 pin 23); RESET: GPIO38 (J2 pin 25) |
| RGB LCD data | GPIO8–19 and GPIO33–36 |
| RGB LCD timing | PCLK: GPIO40; H_EN: GPIO43; H_SYNC: GPIO44; V_SYNC: GPIO45 |
| LCD control | RESET: GPIO37; VDD_EN: GPIO39; PWM: GPIO42 |
| SD data | D0: GPIO20; D1: GPIO21; D2: GPIO22; D3: GPIO23 |
| SD clock and command | CLK: GPIO24; CMD: GPIO25 |
| General-purpose GPIO | GPIO54 (J2 pin 40) |

GPIO33 and GPIO34 are shared between USB Serial/JTAG and the LCD data interface.

## Hardware files

- [Schematic](../hardware/schematic/ESP32-S31-Zero-schematic.pdf)
- [PCB footprint library](../hardware/footprint/)
