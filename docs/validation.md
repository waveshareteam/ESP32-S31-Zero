# Testing and compatibility

## Build environment

The results below were recorded on **2026-10-28** using ESP-IDF `6.2.0-dev-08e0d30a74ad`, commit `08e0d30a74ad0bfd5a34933142b80f45619ee410`, and Component Manager **3.1.2**. The target is `esp32s31` with `--preview`; framework support is experimental. See [component versions](dependencies.md) and [build instructions](examples.md).

## Verified results

| Check | Result |
| --- | --- |
| Dependency resolution, configuration, compilation and linking | **12/12 examples passed locally** |
| Firmware packaging | **12/12 ZIPs passed** individual package and aggregate validation, including image target, flash offsets, checksums and dependency locks |
| Python host tests | **35 passed**, covering packaging, download validation and release gates |
| Repository checks | Passed structure, configuration, documentation link and factory binary checksum checks |
| Workflow syntax | Both workflows passed actionlint **1.7.12** |
| Bash flash helper | Local helper checks passed |

## Test coverage

No firmware was flashed to a physical board. The results above establish local build and package compatibility; they do not establish on-board peripheral, Wi-Fi or Bluetooth operation.

The Windows flash helper has not been executed on Windows. GitHub Actions execution and published Release downloads have not been verified for this product.
