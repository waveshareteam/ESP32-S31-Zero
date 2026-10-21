#!/usr/bin/env python3
"""Package one coherent ESP-IDF build using its own flash metadata."""
import argparse
import datetime
import importlib.metadata
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from discover_examples import ROOT, matrix
from download_firmware import canonical_name, flash_payloads, inspect_archive, sha


def git(*args, cwd=ROOT):
    return subprocess.check_output(['git', '-C', str(cwd), *args], text=True).strip()


def package(build, row, source_sha, epoch, lock, output):
    build = build.resolve()
    f = json.loads((build / 'flasher_args.json').read_text())
    description = json.loads((build / 'project_description.json').read_text())
    extra = f['extra_esptool_args']
    if extra['chip'] != row['target'] or description['target'] != row['target'] or description['project_name'] != row['name']:
        raise ValueError('Build target/project mismatch')
    if git('rev-parse', 'HEAD', cwd=Path(description['idf_path'])) != row['commit']:
        raise ValueError('Build framework commit mismatch')
    if not lock.is_file():
        raise ValueError('Missing resolved dependencies.lock; reconfigure with Component Manager enabled')
    m = dict(schema=1, name=row['name'], project_path=row['path'], framework='esp-idf',
             framework_version=row['version'], framework_commit=row['commit'], experimental=row['experimental'],
             component_manager_version=row['component_manager'],
             target=row['target'], git_sha=source_sha, image_chip_id=32, baud=460800,
             timestamp_utc=datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc).isoformat(),
             before=extra['before'].replace('_', '-'), after=extra['after'].replace('_', '-'),
             stub=extra['stub'], flash_settings=f['flash_settings'], segments=[])
    data = {'dependencies.lock': lock.read_bytes()}
    m['dependencies_lock_sha256'] = sha(data['dependencies.lock'])
    combined = bytearray()
    roles = {str(f[k]['file']): role for k, role in
             (('bootloader', 'bootloader'), ('app', 'app'), ('partition-table', 'partition_table'))}
    for i, (offset, filename) in enumerate(sorted(f['flash_files'].items(), key=lambda item: int(item[0], 0))):
        offset = int(offset, 0)
        file = (build / filename).resolve()
        if not file.is_relative_to(build) or not file.is_file() or file.suffix != '.bin':
            raise ValueError('Flash file outside build or missing')
        b = file.read_bytes()
        if offset < len(combined) or offset + len(b) > 32 * 1024 * 1024 or not b:
            raise ValueError('Overlapping/oversized/empty firmware segment')
        name = f'bin/{i:02}_{file.name}'
        data[name] = b
        combined.extend(b'\xff' * (offset - len(combined)))
        combined.extend(b)
        m['segments'].append(dict(file=name, offset=offset, size=len(b), sha256=sha(b), role=roles.get(filename, 'data')))
    data['bin/combined.bin'] = bytes(combined)
    m['combined_sha256'] = sha(combined)
    data.update(flash_payloads(m))
    data['README.md'] = (f"# {row['name']}\n\nTarget: esp32s31. Source: {source_sha}.\n"
                         f"Framework: {row['version']} ({row['commit']}); experimental={row['experimental']}.\n\n"
                         "Install an esptool version supporting ESP32-S31. Verify SHA256SUMS before use.\n"
                         "Ubuntu Bash: `./flash.sh /dev/ttyACM0`\n\nWindows PowerShell: `.\\flash.bat COM3`\n\n"
                         "Flashing overwrites the addressed flash regions. No erase-all is performed.\n"
                         "Board fixture requirements: see the source example README.\n").encode()
    data['manifest.json'] = (json.dumps(m, indent=2, sort_keys=True)+'\n').encode()
    data['SHA256SUMS'] = ''.join(f'{sha(data[n])}  {n}\n' for n in sorted(data)).encode()
    output.mkdir(parents=True, exist_ok=True)
    archive = output / canonical_name(m)
    # Stable timestamp and permissions; exclusive creation prevents replacement.
    stamp = datetime.datetime.fromtimestamp(max(epoch, 315532800), datetime.timezone.utc).timetuple()[:6]
    try:
        with archive.open('xb') as stream, zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for name in sorted(data):
                info = zipfile.ZipInfo(name, stamp)
                info.create_system = 3
                info.external_attr = (0o100755 if name == 'flash.sh' else 0o100644) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, data[name])
        inspect_archive(archive)
    except FileExistsError:
        raise
    except Exception:
        archive.unlink(missing_ok=True)
        raise
    return archive


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--example', required=True)
    p.add_argument('--version', required=True)
    p.add_argument('--output', type=Path, default=ROOT / 'releases/dist')
    a = p.parse_args()
    try:
        rows = [r for r in matrix(selector=a.example)['include'] if r['version'] == a.version]
        if len(rows) != 1:
            raise ValueError('Expected exactly one configured framework/example')
        if git('status', '--porcelain', '--untracked-files=all'):
            raise ValueError('Refuse to label a dirty source tree as a commit build')
        row = rows[0]
        if importlib.metadata.version('idf-component-manager') != row['component_manager']:
            raise ValueError('Component Manager version does not match the configured lane')
        project = ROOT / row['path']
        print(package(project / 'build', row, git('rev-parse', 'HEAD'), int(git('show', '-s', '--format=%ct', 'HEAD')), project / 'dependencies.lock', a.output))
    except (ValueError, OSError, KeyError, importlib.metadata.PackageNotFoundError) as exc:
        p.exit(2, f'{exc}\n')
