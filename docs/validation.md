# Local validation record

Audit date: 2026-10-21. Validation covers local source changes, static/host tests and dependency checks. Full firmware compilation and flashing were not performed.

| Surface | Result |
| --- | --- |
| Original source projects / newly added | 12 / 0; 12 retained and normalized |
| Schematic | Sheet 1 visually inspected, including enlarged J2 connector; hardware evidence table recorded |
| Framework support | v5.5.6 excluded; v6.1 preview target; v6.2 stable unavailable; pinned 6.2 development preview lane explicitly authorized |
| Firmware build matrix | 12 required experimental jobs configured at `08e0d30a74ad0bfd5a34933142b80f45619ee410`; no build-pass claim |
| Host tests | **34 passed**. Discovery, archive construction/validation, aggregate release gate and download checks tested with synthetic firmware fixtures |
| Static repository checks | Passed. Names, CMake project names, target defaults, docs links, public text and factory digest checked |
| YAML / shell / whitespace | Workflow YAML parsed; POSIX helper exercised; `git diff --check` passed |
| Dependency resolution | All 12 projects passed independent first-pass resolution/download with Component Manager 3.1.2, target esp32s31, pinned SDK and interface 5; see [resolved dependencies](dependencies.md) |
| ESP-IDF configure / full build | Full CMake configure not executed: the installed 6.2 host environment could not activate and CMake/Ninja were unavailable. Dependency resolution used an isolated Python environment. Full firmware compilation was not executed; no compile-pass claim |
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
2. Run full CMake/Kconfig configuration, including the post-Kconfig dependency pass, after installing the pinned framework host tools. The independent first-pass dependency audit passed for all 12 projects; retain CI's final actual lock files in packages and confirm API compatibility through builds.
3. Run complete build CI, validate real build artifacts and the full required archive set. Synthetic tests do not establish real ESP32 image compatibility.
4. Confirm fitted chip/Flash/PSRAM, fixture schematic/BOM, timing/reset/power sequencing and upstream board-definition availability.
5. Perform real-board tests and Windows helper testing, then independently verify a published product Release after a separately authorized deployment.

The referenced product-note URL returned a JavaScript redirect page without product content in the read-only fetch. No specifications were inferred from it. Reference repositories were used for layout and workflow behavior only; their peripheral inventory, GPIOs and version pins were not copied.
