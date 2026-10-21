# ESP32-S31-Zero V1.0 hardware evidence

Evidence date: 2026-10-21. Primary source: [supplied schematic](../hardware/schematic/ESP32-S31-Zero-schematic.pdf), sheet 1. Sheets 2–3 contain layout views. V1.0 is the supplied product revision; the schematic title identifies ESP32-S31-Zero but does not establish a detailed chip ordering code. Code alone is not proof of fitted hardware.

| Item | Schematic evidence | Configuration / limitations |
| --- | --- | --- |
| MCU | U2, ESP32-S31 | `esp32s31`, not ESP32-S3. Exact package/PSRAM ordering suffix and silicon revision pending |
| Flash | U3 GD25Q256EYIGR, SPI CS/CLK/D/Q/WP/HD | 256 Mbit = 32 MiB schematic capacity. Existing examples retain 16 MiB image headers/partitions, DIO 80 MHz; fitted part and operating margin require a board test |
| Flash supply | R15 10 kΩ pulls GPIO41 high; annotation `1:3V3 flash`; U3 on VDD_SPI, R21 marked 10K/NC | Do not drive memory/strap pins; verify fitted options before changing voltage/mode |
| PSRAM | No separate PSRAM IC or explicit capacity in sheet 1 | Existing fixture test requests octal 200 MHz and PSRAM XIP. Retained as an unverified fixture setting, not a board capacity claim |
| Clock | Y1 40 MHz, C16/C30 and L7 | Schematic value; no frequency measurement performed |
| RGB LED | LED1 WS2812B-1010; GPIO60 via R11/Q1 level shifter, 5 V LED supply; R16 10 kΩ pull-up | One addressable RGB LED, serial data (not an active-low discrete LED); existing RMT encoder retained |
| User/boot key | S1, GPIO61, R28 10 kΩ to ESP_3V3, switch to GND | Pressed = low; avoid holding during reset except intentional download mode |
| Reset key | S2 on EN/CHIP_PU; R27 10 kΩ pull-up, C32 1 µF | Reset asserted low. RC network does not by itself prove a software delay requirement |
| USB Type-C | H1 D−/D+ through D2/D3 and L2 to USB_N/USB_P on GPIO33/34 | USB Serial/JTAG console; these GPIOs are also J2 RGB data. Screen startup in example 11 takes over the USB console pins |
| Other USB pair | Dedicated U2 USB_DM/DP via R19/R20 0 Ω to P8/P9 | Test pads; not the H1 Type-C pair. No USB host example added |
| Supply | H1 VBUS → D1 MBR230LSFT1G → 5 V; U1 MP1605GTF-Z → 3V3, 200 kΩ / 44.2 kΩ divider | Drawing estimates 3.314 V / max 2 A for converter; not a measured board load rating. No software PMU sequence established |
| RF | U2 ANT through matching network L3/L4/L6 and ANT1; alternate J6 CA-C03 path marked NC | Attach the appropriate external 2.4 GHz antenna to the populated RF connector. Original code's J1 label corrected to ANT1. Antenna gain/part pending |
| GPIO header | H2 exposes GPIO0–6, GPIO46–51, RXD0/GPIO58, TXD0/GPIO59, 3V3/5V/GND | Example 01 drives these 15 signals high; it is not a loopback test. Disconnect loads/other drivers first |
| UART loopback | J2 pin 8 = GPIO8, pin 10 = GPIO9 | Software selects UART1 TX/RX; requires an external jumper and no screen attached |
| Expansion I²C | J2 pin 19 TP_SDA = GPIO52, pin 21 TP_SCL = GPIO53; R7/R29 marked NC | No on-board I²C slave or fixed address. Use suitable external pull-ups to 3.3 V. Scanner skips reserved addresses |
| Expansion touch | J2 pin 23 TP_INT = GPIO7; pin 25 TP_RESET = GPIO38 | No touch IC shown. Example 11's GT911 0x14/0x5D probe/reset logic belongs to its existing fixture |
| Expansion LCD | PCLK40, RESET37, VDD_EN39, PWM42, H_EN43, H_SYNC44, V_SYNC45; data8–19,33–36 | Connector net labels only. Resolution, controller, timing, power enable level and sequence need external display documentation. Example 11 uses 800×480 RGB565 at 16 MHz; these are not verified board specifications |
| Expansion SD | J2 D0/20, D1/21, D2/22, D3/23, CLK24, CMD25 | No card socket, card-power switch or external carrier pull-up values established. Existing fixture uses four-bit 20 MHz |
| Expansion GPIO54 | J2 pin 40 | No onboard GPIO54 status LED shown; example 11's active-low LED is fixture-specific |

## Preserved product adaptations

The RMT WS2812 encoder, GPIO61 input handling, USB Serial/JTAG defaults, conservative 16 MiB partition layout, GT911 reader filtering, RGB bounce-buffer/PSRAM XIP configuration, SD bus release/retry logic, and dedicated `testdata` Flash test partition are retained. The last five belong to an existing fixture application, not new peripheral claims. No screen, sensor, audio codec, RTC or radio from either reference product was copied.

## Board-definition follow-up

Before publishing a shared Waveshare board profile: confirm chip ordering code, PSRAM capacity/mode/speed, fitted Flash and straps, RF connector/antenna, and external fixture schematic/BOM/power sequencing. The supplied drawing does not justify a generic GT911/LCD BSP for this board. Verify an upstream `waveshare_boards` entry and ESP32-S31 compatibility for the approved framework revision before replacing local adapters; no verified compatible entry has been established in this audit. Keep generated board code out of Git.
