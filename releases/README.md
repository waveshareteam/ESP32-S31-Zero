# Source firmware packages

These tools package maintained source builds, never the supplied [factory image](../firmware/README.md). [CI contract](../docs/ci.md) explains the full-matrix publication gates and approved experimental matrix.

Naming: `esp-idf-EXAMPLE-6.2.0-dev-08e0d30a74ad-esp32s31-SOURCE_SHA12.zip`.
Each manifest includes the full source/framework commits, experimental status, Component Manager version, target, source timestamp, flash settings, reset modes, baud, segment offsets/sizes/hashes and dependency-lock digest.

Members: `manifest.json`, `SHA256SUMS`, `README.md`, `flash_args.txt`, `flash.sh`, `flash.bat`, `dependencies.lock`, original `bin/NN_*.bin` segments and exactly one compact `bin/combined.bin`. Offsets and options come from the same build's `flasher_args.json`; gaps are erased bytes and the combined image ends at the final required segment. No Flash-capacity padding is appended. The image header target ID is checked against [upstream esptool's ESP32-S31 definition](https://github.com/espressif/esptool/blob/master/esptool/targets/esp32s31.py) (32).

Packaging requires a clean source checkout, matching framework commit and a resolved lock file. It rejects output replacement. ZIP timestamps come from the source commit; archive members and permissions are canonical. `SHA256SUMS` covers every member except itself. Release downloads additionally verify GitHub's archive SHA-256 before extraction. Checksums verify integrity, not independent publisher authenticity.

Ubuntu Bash, **after a real build** using an approved version:

```bash
python3 releases/package_firmware.py --example 02_blink_rgb --version 6.2.0-dev-08e0d30a74ad
python3 releases/validate_release.py releases/dist --commit FULL_SOURCE_COMMIT
```

The second command requires the complete configured set, not a single-example subset.

## Download (standalone Python 3.10+ script)

Set `OWNER/REPOSITORY` to the actual public GitHub release repository. No publication repository or release is assumed to exist. `--tag` selects the product Release; `--version` selects the exact framework version shown by `--list`. Without `--tag`, the latest public stable Release is selected.

Ubuntu Bash:

```bash
python3 scripts/download_firmware.py --repo OWNER/REPOSITORY --list
python3 scripts/download_firmware.py --repo OWNER/REPOSITORY --tag v1.0.0 --example 02_blink_rgb --framework esp-idf --version 6.2.0-dev-08e0d30a74ad --output releases/downloads
```

Windows PowerShell:

```powershell
py -3 scripts/download_firmware.py --repo OWNER/REPOSITORY --list
py -3 scripts/download_firmware.py --repo OWNER/REPOSITORY --tag v1.0.0 --example 02_blink_rgb --framework esp-idf --version 6.2.0-dev-08e0d30a74ad --output releases/downloads
```

The downloader uses HTTPS, timeout/retry, Release-asset SHA-256, archive-member checksums, strict member/path/type/size validation, and exclusive destination creation. It refuses ambiguous choices, existing files, symlinks and traversal. It does not run extracted files or flash. `download_firmware.py` can be copied and run by itself without repository modules or third-party Python libraries.

## Flash deliberately after inspection

Install an esptool release supporting `esp32s31`, inspect the manifest and equipment notes, and enter the extracted package directory. A normal USB serial console uses the board's Type-C port. Check the actual serial device/COM port; package examples are placeholders.

Ubuntu Bash:

```bash
sha256sum -c SHA256SUMS
./flash.sh /dev/ttyACM0
```

Windows PowerShell (the downloader has already checked all checksums):

```powershell
.\flash.bat COM3
```

Helpers select an interpreter that can import esptool, quote the serial port, preserve the tool's exit code and write the combined image at its declared base address 0. Segment offsets inside that image came from the build. No automatic erase-all, Flash voltage change or firmware download is performed. Example 11 takes over GPIO33/34 for its external LCD and can stop the USB console after startup.
