# ESP32-S31-Zero Board Test Firmware

This existing firmware requires an external LCD/touch/SD/status-LED fixture.
The base-board schematic establishes connector routing, not the fixture BOM,
panel timing, GT911 address/reset sequence or PSRAM capacity. See
[hardware evidence](../../../docs/hardware.md). It starts automatically after boot;
do not flash it as a fixture-free basic board example.
The onboard WS2812B-1010 on GPIO60 cycles RED -> GREEN -> BLUE, holding each
color for one second at brightness 24/255. The cycle starts after LCD startup
and continues during and after the automatic tests in a background task.
Pending checks on the LCD are shown as WAIT and update to PASS or FAIL as each
test completes. LED colors no longer encode the overall test result. The RGB
check verifies driver operation; confirm all three physical colors visually.
USB Serial/JTAG is used for the early USB check and startup messages. After the
LCD takes GPIO33/34, the remaining results are shown on screen; the interactive
command loop remains available only if LCD startup fails. Normal ESP-IDF
information and warning logs are suppressed; driver errors are still shown.

Build and run:

```powershell
idf.py --preview -C examples/esp-idf/11_board_test build
idf.py --preview -C examples/esp-idf/11_board_test flash monitor
```

Before power-up, connect a suitable 2.4 GHz antenna to ANT1 and place the board
near a working access point. The Wi-Fi check uses an active scan and passes
when at least one 2.4 GHz AP is received at -85 dBm or better.

The automatic sequence checks chip/crystal identity, the fitted Flash device's
physical capacity, one Flash erase/write/read cycle, read-only Flash throughput,
PSRAM write/read throughput and integrity, die temperature, Wi-Fi, USB
Serial/JTAG and a two-second internal RAM/CPU stress test. Flash writes are
strictly limited to the first 4 KiB sector of the dedicated 64 KiB `testdata`
partition and the sector is erased again after verification.

The Flash benchmark reads 512 KiB from the application partition four times
without erasing or writing it. The PSRAM benchmark transfers a 2 MiB external
memory block four times in each direction and verifies every block. Reported
`MiB/s` values are measurements rather than fixed pass thresholds; use the
`speed` command to repeat both benchmarks without running the other tests.

Run `selftest` or `all` to repeat the automatic sequence. Use `help` for
fixture-assisted GPIO, interrupt, UART, SPI, ADC, PWM, sleep and other tests.
The manual `flash_rw` command still requires the literal confirmation token
`ERASE-TESTDATA`.

Do not attach loopback jumpers to GPIO33/34, GPIO36/37, or GPIO60/61 unless
the corresponding command and the bring-up document explicitly require it.

Before LCD startup, firmware records the USB Serial/JTAG state, checks the
active-low status LED on GPIO54, mounts the microSD card over SDMMC Slot 0 in
4-bit mode at 20 MHz, and initializes touch.
It then starts the LCD/UI before GPIO60
starts cycling and runs the remaining automatic tests while updating the UI.
The SD test performs a temporary-file write, sync, readback, compare and delete
test. The card is then unmounted and the SD bus is released.
Card absence, mount failure or I/O failure is reported without formatting the
card and does not prevent the remaining peripherals. In 4-bit mode the card
uses GPIO20/D0, GPIO21/D1, GPIO22/D2 and GPIO23/D3 in addition to GPIO24/CLK
and GPIO25/CMD.

Before each mount attempt, firmware releases CLK/CMD/D0-D3 to high impedance
for 100 ms before initializing the SDMMC host. A failed first mount receives
one bus-release and host-initialization retry. There is no dedicated SD power
control GPIO in this example; the complete SD interface is GPIO20 through
GPIO25. Reliable 4-bit operation also requires suitable CMD/D0-D3 pull-ups and
signal integrity on the attached card carrier.

The host advertises only 4-bit SDR data transfers, and the test rejects a card
that did not negotiate 4-bit mode. Initial card identification still uses the
SD protocol's mandatory 1-bit phase at 400 kHz; this is not a 1-bit fallback.
`[SD]` messages report ACMD6 (bus-width selection), data commands, their actual
clock and their result, even when normal ESP-IDF logs are suppressed.
After mounting, the firmware prints the negotiated width and card information.

If `sdmmc_init_sd_ssr` / ACMD13 times out after ACMD51 (SCR) and ACMD6 succeeded,
the failure is at the first SD-status data transfer after switching to 4-bit.
In the current IDF initialization sequence this is still at 400 kHz, before
the host switches to 20 MHz. Check D1/D2/D3 continuity and ordering, CMD/D0-D3
external pull-ups (typically 10 kOhm to the card's 3.3 V supply), card power,
and the target-specific driver. A successful D0/SCR read does not validate
D1/D2/D3. Lowering the final transfer speed or formatting the card does not
address this initialization stage. Releasing GPIOs is not a card power cycle.

GPIO54 uses output mode and stays low to keep the active-low fixture LED on.
It does not toggle for a high/low readback test. GPIO54 is reserved from manual
GPIO, interrupt, PWM, UART and ADC tests; GPIO60 is reserved from UART tests
while the RGB driver is active. Successful driver initialization does not replace visual
confirmation of the physical LED.

The GT911 bus is GPIO52/SDA and GPIO53/SCL, with GPIO38/RST and GPIO7/INT.
Firmware probes both 0x14 and 0x5D before creating the GT911 driver and uses
only an address that acknowledged. If neither address responds, it prints
`*�K0�x`, releases the I2C bus and continues without touch.

The 800x480 RGB565 LCD uses a 16 MHz falling-edge pixel clock and 4/8/8
horizontal and vertical sync timing. Its framebuffer remains in PSRAM, while
two 20-line RGB bounce buffers, the 10-line LVGL draw buffer, and the LVGL task
stack use internal RAM. This reduces PSRAM contention during drawing and gives
the RGB copy interrupt more buffering margin. PSRAM XIP and the IRAM-safe RGB
interrupt are enabled because the automatic Flash R/W test runs with the LCD
active. These settings consume additional internal RAM and PSRAM; check LCD,
Wi-Fi and memory-test results after flashing. On ESP32-S31 the IDF RGB restart
API is unsupported; the ESP32-S3-only VSYNC restart option is not applicable.
Its UI reports the original nine
automatic tests, the GPIO60 RGB indicator, LCD, Touch, SDMMC and GPIO54 status.
Failed peripherals retain their real failure state and SDMMC shows the returned
ESP-IDF error name. Continuous touch sampling and the orange marker are disabled
by default (`CONFIG_BOARD_TOUCH_TRACKING=n`) because the hardware still reports
touch communication failures. GT911 detection/initialization runs once at startup,
then its driver and I2C bus are released. The Touch tile reports only that startup
result, not runtime tracking reliability. No sampling task, LVGL input or marker
timer is created. The footer shows only UI uptime and LVGL memory statistics,
without a tracking status message or inactive Reads/Errors/Last counters.

For further diagnosis, `idf.py menuconfig` > Board test > Enable continuous touch
sampling and touch marker restores the optional runtime behavior described below.
When this option is enabled and touch is available, a translucent orange marker follows
the primary touch point. On release it stays at the last position for two seconds
and then disappears. Touching again immediately resumes tracking and cancels the
previous release timeout. The marker is not clickable and uses the coordinates
published by the touch sampling task. It is attached directly as the LVGL input cursor;
there is no separate movement-polling timer. The touch read timer is 10 ms and
the redraw timer is 20 ms. The 16 MHz panel timings allow approximately 39
physical frames per second; these timer periods are not measured frame rates.
The CPU/RAM test yields one scheduler tick between verified blocks to leave
time for background tasks.

CPU/RAM reserves four separate 16 KiB blocks with INTERNAL | 8BIT capabilities,
holding all 64 KiB simultaneously. This avoids requiring a contiguous 64 KiB
region after LCD/task allocations (the reported largest free block was 54 KiB).
Each round writes all four blocks before verifying all four, using one continuous
pseudorandom sequence across them. Free PSRAM and the LVGL pool are not substitutes.
The tile shows `ALLOC <n>K/64K` if allocation fails: <n> is the total successfully
reserved before the failure, and all partial allocations are then released.
`R/W mismatch @<hex>` identifies a byte offset in the logical four-block sequence, and
`No completed round` means no full write/read pass completed. A passing tile
shows `4x16K` and the number of fully verified rounds. No minimum speed
or temperature threshold is used. The test does not reduce its total reserved memory or fall
back to PSRAM to turn a failure into a pass. Detailed console output includes
remaining free/largest internal heap at allocation failure (before cleanup),
or the expected/actual mismatched values.

With tracking enabled, touch sampling runs in a dedicated 4 KiB task, outside the LVGL lock. It polls
GT911 at 100 kHz with a 50 ms timeout per transaction and waits 10 ms between
polls (20 ms after a failure). The 50 kHz comparison did not resolve the reported
failures, so the normal 100 kHz setting is restored. I2C IRAM-safe interrupts are
enabled so the Flash self-test does not mask transaction-completion interrupts.
The GT911 component still handles initialization and reset; project code reads
runtime status/coordinate registers without modifying managed components.

The reader only acknowledges a ready frame after its coordinate read succeeds.
Not-ready status does not clear the register or replay the driver cache as a new
sample. Completed frames must contain valid point counts, distinct track IDs and
coordinates within 800x480. The pointer follows the same track ID when multiple
fingers change order. Losing that ID emits a release before another is selected.
An isolated jump larger than 120 pixels on either axis is withheld until another
frame arrives within 48 pixels of the candidate. This can add one frame of delay
to a large genuine movement; normal small movements are not averaged or delayed.

LVGL reads a short snapshot under a spinlock and never waits for I2C. A valid
zero-point frame releases immediately. During brief errors or not-ready periods,
the last accepted point is retained for at most 150 ms. LVGL also checks that
expiry independently, even if the sampling task is blocked. The orange marker
then follows the same two-second release timeout. Range and jump rejection
reduce bad input but do not repair electrical faults or all possible corrupted
in-range coordinates. Persistent bus errors still need hardware/driver diagnosis.

The bottom of the screen updates once per second without USB logging:
`UI` is the LVGL tick time in seconds. `Reads` counts polling attempts, including
not-ready status; it is not comparable directly to the previous IRQ-only count.
`Frames` counts complete acknowledged frames (including zero-point releases),
`Errors` counts failed polls, and `Drop` counts rejected malformed frames or
unconfirmed jumps. `Last` retains the last nonzero error code (`0x107` means
ESP_ERR_TIMEOUT; `0x108` means ESP_ERR_INVALID_RESPONSE). This IDF can return
0x108 for both NACK and some timeout paths; validation also uses it for malformed
frames. `Read`/`Max` show the latest/maximum full polling duration, and `Error`
retains the failed poll duration plus `IO` for transfer failures or `DATA` for
frame validation failures. Timings include scheduling and logging overhead.
The third line reports the LVGL pool's free bytes, largest block and peak usage,
not total system RAM. All counters come from the sampling task; the UI does not
count repeated reads of its cached snapshot as new hardware reads. Diagnostic
text reuses a static buffer.

The original documentation described additional touch-reader regression tests, but
that test harness is not present in the supplied source set. Those tests have not
been run in this audit; no touch reliability claim follows from static checks.

LCD startup assigns GPIO33 and GPIO34 to RGB data, so USB Serial/JTAG
and the interactive USB test console are no longer available after the screen
starts. If LCD startup fails before that handoff completes, the command loop
remains available.

Managed dependency requests are pinned in `main/idf_component.yml`: esp_lvgl_adapter 0.6.4, LVGL 9.5.0, esp_lcd_touch 1.2.1
and esp_lcd_touch_gt911 1.2.1, for ESP32-S31. ESP-IDF 6.2 is a development line, not a verified stable release;
use only an explicitly approved pinned revision from [versions](../../../docs/versions.md).
A resolved lock file must be generated and included in each firmware archive.

The project keeps a 16 MiB image header and partition table so the same firmware
remains flashable on boards fitted with either 16 MiB or 32 MiB devices. The
Flash test uses `esp_flash_get_physical_size()` and reports the full detected
physical capacity; capacity beyond the 16 MiB partition boundary is reported
but is not allocated by this firmware.
