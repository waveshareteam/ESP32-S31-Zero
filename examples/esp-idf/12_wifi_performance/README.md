# ESP32-S31-Zero Wi-Fi Performance Test

This example is based on the ESP-IDF `examples/wifi/iperf` project and uses
Espressif's maintained `wifi-cmd`, `iperf-cmd`, and `ping-cmd` components.
It provides an interactive USB Serial/JTAG console for repeatable Wi-Fi
throughput and link-quality measurements.

The ESP32-S31-Zero schematic routes RF to ANT1 and does not show a PCB antenna.
Connect a suitable 2.4 GHz antenna to ANT1 before enabling Wi-Fi. Never run a
transmit test into an open RF connector.

## Scope

This firmware is suitable for:

- TCP and UDP throughput in station or SoftAP mode
- ESP transmit and receive data paths
- RSSI, channel, bandwidth, protocol, and configured TX-power logging
- ping latency and packet-loss checks
- OTA chamber throughput pre-checks with the production antenna and enclosure

It is not a substitute for formal TRP, TIS, receiver PER/sensitivity, or
radiated-spurious testing. Those tests require calibrated chamber/tester
equipment and the dedicated Espressif ESP32-S31 RF test firmware.

## Build and flash

Use only an approved pinned ESP-IDF revision from [versions](../../../docs/versions.md).
The existing code was prepared for the 6.2 development line; 6.2 is not an established stable release. The first build needs access to the ESP Component Registry to
resolve the dependencies declared in `main/idf_component.yml`.

```powershell
idf.py --preview -C examples/esp-idf/12_wifi_performance build
idf.py --preview -C examples/esp-idf/12_wifi_performance flash monitor
```

The project is fixed to `esp32s31`, 16 MB Flash, DIO at 80 MHz, and USB
Serial/JTAG console output. Wi-Fi power saving is disabled while this firmware
is running.

The console prompt is `wifi-perf>`. Run `help` for the complete command
list. The peer must use iperf 2.x; iperf3 uses a different protocol and is not
compatible.

## Connect in station mode

Set the test AP to the required channel and bandwidth, then connect:

```text
sta_connect <ssid> <password>
rf_info
ping <gateway-ip>
```

`rf_info` prints the active mode, primary/secondary channel, bandwidth,
protocol bitmap, RSSI, BSSID, and the configured maximum TX-power value. The
TX-power field uses 0.25 dBm units and is only a software configuration value;
it is not a conducted or radiated power measurement.

## SoftAP mode

```text
wifi_mode ap
ap_set <ssid> <password>
rf_info
```

Use `help` before changing any radio option because the exact options exposed
by `wifi-cmd` follow the managed component version.

## Throughput tests

Replace `<PC_IP>` and `<DUT_IP>` with the actual addresses. Use a wired
Ethernet connection between the PC and test AP whenever possible so that the
PC's Wi-Fi link does not become the bottleneck.

| Direction | PC iperf 2.x command | ESP console command |
| --- | --- | --- |
| TCP, ESP TX | `iperf -s -i 1` | `iperf -c <PC_IP> -i 1 -t 60` |
| TCP, ESP RX | `iperf -c <DUT_IP> -i 1 -t 60` | `iperf -s -i 1` |
| UDP, ESP TX | `iperf -s -u -i 1` | `iperf -c <PC_IP> -u -b 20M -i 1 -t 60` |
| UDP, ESP RX | `iperf -c <DUT_IP> -u -b 20M -i 1 -t 60` | `iperf -s -u -i 1` |

Start UDP at a conservative bandwidth, then increase `-b` until loss or
jitter exceeds the test requirement. The firmware prints Wi-Fi TX/RX
statistics when an iperf run ends.

## OTA test method

Use the same hardware revision, production antenna/cable, enclosure, power
supply, firmware, and AP/tester settings for every comparison.

| Variable | Values |
| --- | --- |
| Channel | 1, 6, 11 |
| Bandwidth | 20 MHz, then 40 MHz if required |
| Direction | TCP TX, TCP RX, UDP TX, UDP RX |
| Orientation | At least three orthogonal axes; full turntable sweep for chamber work |
| Duration | 60 seconds per run, at least three runs |

For each run record the hardware revision, antenna/cable, enclosure state,
channel, bandwidth, AP mode, regulatory setup, distance or path loss,
orientation, `rf_info` output, throughput, UDP loss/jitter, RSSI, and
disconnect count. Use the median of repeated runs for engineering comparisons.

## TRP, TIS, and radiated spurious

The original notes referenced EspRFTestTool v5.73 and this dedicated image; neither
the tool, image nor its test manual is present in this repository:

```text
ESP32-S31_RFTest_V100_736358f_20260608.bin
```

Obtain a verified ESP32-S31 image and its matching
`RF_test_introduction.pdf` and the laboratory's approved procedure. Do not
flash an ESP32-C5, C6, C61, S3, or similarly named RF image onto ESP32-S31.

- **TRP:** generate the lab-specified packet TX waveform, rate, channel,
  bandwidth, and power setting; integrate measured EIRP over the required
  sphere.
- **TIS:** use the RF image/tester receiver mode and the lab's PER criterion
  while sweeping orientation and input level.
- **Radiated spurious:** use the prescribed continuous packet or other
  approved transmitter mode and measure with a spectrum analyzer/EMI receiver.

The normal iperf firmware may be used as a functional traffic source or OTA
throughput pre-check, but rate adaptation and packet scheduling do not provide
the deterministic waveform required for certification. Restore the normal
application firmware after RF testing.

Formal limits, channels, rates, detector settings, RBW/VBW, distance, and
pass/fail criteria depend on the target market and laboratory test plan; they
are intentionally not hard-coded here.

## Verification status

The source and configuration were added from the ESP-IDF 6.2 official iperf
example and written against ESP32-S31 APIs. It has not been
compiled, flashed, or validated in a chamber.
