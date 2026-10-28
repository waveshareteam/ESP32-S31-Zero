# Local validation record

Audit date: 2026-10-28. All 12 projects passed local CMake configuration, C/C++ compilation, linking and firmware packaging with the approved experimental framework. No firmware was flashed. GitHub Actions execution and physical-board acceptance remain separate checks.

| Surface | Result |
| --- | --- |
| Original source projects / newly added | 12 / 0; 12 retained and normalized |
| Schematic | Sheet 1 visually inspected, including enlarged J2 connector; hardware evidence table recorded |
| Framework support | v5.5.6 excluded; v6.1 preview target; v6.2 stable unavailable; pinned 6.2 development preview lane explicitly authorized |
| Firmware build matrix | **12/12 local builds passed** at `08e0d30a74ad0bfd5a34933142b80f45619ee410`, target `esp32s31`, using `--preview` |
| Host tests | **35 passed**. Includes archive/download/release gates and CLI validation of selected, full and stale artifact sets |
| Static repository checks | Passed. Names, CMake project names, target defaults, docs links, public text and factory digest checked |
| YAML / shell / whitespace | actionlint **1.7.12** passed for both workflows; downloaded binary SHA-256 verified; POSIX helper exercised; `git diff --check` passed |
| Dependency resolution | All 12 projects passed real CMake/Kconfig-aware resolution and download with Component Manager **3.1.2**; see [resolved dependencies](dependencies.md) |
| ESP-IDF configure / full build | **12/12 passed** in an isolated source snapshot with fresh generated configurations. Tools: Python 3.12.3, CMake 4.0.3, Ninja 1.12.1, RISC-V GCC 16.1.0 (`esp-16.1.0_20260609`), esptool 5.4.0. Original working-copy configurations were preserved |
| Real firmware packages | **12/12 ZIPs passed**, including image chip ID, flash offsets, compact combined image, original segments, helpers, checksums, dependency locks and aggregate source/framework identity. The normal packaging CLI was exercised from a clean local snapshot; these are local verification artifacts |
| Physical board / Flash / Wi-Fi / BLE / fixture | Not tested; no firmware flashed |
| Windows helper runtime | Not executed on Windows; batch structure checked, POSIX fallback/port quoting exercised |
| GitHub Actions / Releases | Configured locally, not run or deployed |
| Public download | Tested with controlled synthetic HTTP payloads; no product Release exists that was verified here |
| Review and publication | Source changes prepared for GitLab merge-request review; no merge, tag or firmware Release is part of this change |

## Initial repository state

The initial checkout had one tracked README commit. Existing examples, hardware, factory image, LICENSE, `.gitignore` and an empty Chinese README were untracked. These supplied files were treated as user work. Renamed source directories preserve their contents; targeted changes correct project names, I²C connector references, RF connector labels and physical Flash reporting. Original local SDK configurations remain ignored rather than removed. At the end of the initial normalization, all 93 original files were accounted for after path mapping: 68 were byte-identical and 25 had targeted edits. The product owner confirmed that `hardware/schematic/PcbLib2.PcbLib` is unnecessary and intentionally excluded; it is not a missing deliverable and does not need restoration. The schematic PDF, main footprint library, LICENSE, original generated SDK configurations and factory binary remain byte-identical to the original audit. MR preparation normalized source line endings and extra end-of-file blank lines, and added binary Git attributes so firmware and hardware assets are not treated as text diffs.

There was one worktree and no nested repository boundary warning. No submodule or Git LFS declaration was present; Git LFS tooling was not installed, so a separate LFS runtime status is unavailable. The inspected primary remote's main ref matched the local initial commit. GitHub hosting, mirror direction and protection rules remain unverified. The initial local-only audit did not change remote state; the source changes are now prepared for GitLab merge-request review.

## Remaining acceptance work

1. Revisit stable target support when Espressif releases it. The authorized pinned 6.2 lane is experimental; existing example 11 requests IDF >=6.2.0, so v6.1 alone cannot currently satisfy every project.
2. Run the GitHub workflow for the reviewed commit, including the new artifact-set gate on ordinary pull requests. Local compilation and packaging passed; hosted runner installation, network downloads and Actions artifact transfer still require that real run.
3. Retain each build's final resolved lock in its archive. Registry ranges can resolve newer transitive versions, so preserve the verified package rather than assuming the earlier audit table is immutable.
4. Confirm fitted chip/Flash/PSRAM, fixture schematic/BOM, timing/reset/power sequencing and upstream board-definition availability.
5. Perform real-board tests and Windows helper testing, then independently verify a published product Release after a separately authorized deployment.

The referenced product-note URL returned a JavaScript redirect page without product content in the read-only fetch. No specifications were inferred from it. Reference repositories were used for layout and workflow behavior only; their peripheral inventory, GPIOs and version pins were not copied.

## Reference CI findings

The [ESP32-S3-LR1121-ePaper-2.9 failed run](https://github.com/waveshareteam/ESP32-S3-LR1121-ePaper-2.9/actions/runs/40460058751) was inspected through its actual stable and experimental ESP-IDF job logs. Both stopped because the downloaded `waveshare/waveshare_boards` package did not contain the requested board definition. Its [later successful run](https://github.com/waveshareteam/ESP32-S3-LR1121-ePaper-2.9/actions/runs/40662444351) followed the published board-component update and exclusion of credential-dependent LoRaWAN firmware jobs. A Dependabot PR title alone does not establish an Actions-version failure.

S31-Zero does not use that board-generation pipeline or LoRaWAN provisioning. All its declared dependencies were downloaded and built directly. The existing framework pin and preview flag were retained, every Actions SHA was checked against its upstream major-version ref, and workflow syntax was checked with actionlint. The added artifact-set gate now runs before tagging, with failure diagnostics retained separately from distributable firmware.
