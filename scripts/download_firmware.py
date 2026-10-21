#!/usr/bin/env python3
"""Standalone standard-library GitHub Release downloader. Never flashes."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile

MAX_DOWNLOAD = 128 * 1024 * 1024
MAX_EXPANDED = 256 * 1024 * 1024


def sha(data):
    return hashlib.sha256(data).hexdigest()


def canonical_name(m):
    for key in ('framework', 'name', 'framework_version', 'target'):
        if not re.fullmatch(r'[a-zA-Z0-9._-]+', m[key]):
            raise ValueError(f'Invalid identity: {key}')
    if not re.fullmatch(r'[0-9a-f]{40}', m['git_sha']):
        raise ValueError('Invalid source commit')
    return f"{m['framework']}-{m['name']}-{m['framework_version']}-{m['target']}-{m['git_sha'][:12]}.zip"


def inspect_archive(path):
    with zipfile.ZipFile(path) as z:
        members = z.infolist()
        names = [i.filename for i in members]
        if len(names) != len(set(names)) or len(names) != len({n.casefold() for n in names}):
            raise ValueError('Duplicate archive member')
        if sum(i.file_size for i in members) > MAX_EXPANDED:
            raise ValueError('Archive too large')
        for i in members:
            n = i.filename
            mode = i.external_attr >> 16
            if (not re.fullmatch(r'[A-Za-z0-9_.\-/]+', n) or n.startswith('/')
                    or any(p in ('', '.', '..') for p in n.split('/'))
                    or ':' in n or '\\' in n or i.is_dir() or i.flag_bits & 1
                    or stat.S_IFMT(mode) not in (0, stat.S_IFREG)):
                raise ValueError(f'Unsafe archive member: {n}')
        data = {n: z.read(n) for n in names}
    m = json.loads(data['manifest.json'])
    if m['schema'] != 1 or m['framework'] != 'esp-idf' or m['target'] != 'esp32s31':
        raise ValueError('Unsupported archive identity/schema')
    canonical_name(m)
    if m['image_chip_id'] != 32:
        raise ValueError('Wrong ESP32-S31 image ID')
    if m['project_path'] != 'examples/esp-idf/' + m['name']:
        raise ValueError('Project path mismatch')
    required = {'manifest.json', 'SHA256SUMS', 'README.md', 'flash_args.txt', 'flash.sh', 'flash.bat', 'bin/combined.bin', 'dependencies.lock'}
    segments = m['segments']
    if not segments:
        raise ValueError('No firmware segments')
    segment_names = [s['file'] for s in segments]
    if len(segment_names) != len(set(segment_names)):
        raise ValueError('Duplicate segment')
    if any(not re.fullmatch(r'bin/[0-9]{2}_[A-Za-z0-9_.-]+\.bin', n) for n in segment_names):
        raise ValueError('Invalid segment filename')
    if set(data) != required | set(segment_names):
        raise ValueError('Missing or extra archive members')
    lines = data['SHA256SUMS'].decode().splitlines()
    expected_lines = [f'{sha(data[n])}  {n}' for n in sorted(set(data) - {'SHA256SUMS'})]
    if lines != expected_lines:
        raise ValueError('Checksum mismatch or noncanonical checksum list')
    combined = data['bin/combined.bin']
    end = 0
    for s in segments:
        offset = s['offset']
        b = data[s['file']]
        if type(offset) is not int or offset < end or not b or offset + len(b) > 32 * 1024 * 1024:
            raise ValueError('Invalid/overlapping segment')
        if len(b) != s['size'] or sha(b) != s['sha256']:
            raise ValueError('Segment metadata mismatch')
        if combined[end:offset] != b'\xff' * (offset - end) or combined[offset:offset+len(b)] != b:
            raise ValueError('Combined image mismatch')
        if s['role'] in ('bootloader', 'app'):
            # ESP image header chip ID, confirmed against upstream esptool.
            if len(b) < 24 or b[0] != 0xe9 or int.from_bytes(b[12:14], 'little') != m['image_chip_id']:
                raise ValueError('Firmware image chip mismatch')
        end = offset + len(b)
    if not {'bootloader', 'app', 'partition_table'} <= {s['role'] for s in segments}:
        raise ValueError('Incomplete firmware roles')
    if len(combined) != end or sha(combined) != m['combined_sha256']:
        raise ValueError('Combined padding/size/hash mismatch')
    if sha(data['dependencies.lock']) != m['dependencies_lock_sha256']:
        raise ValueError('Dependency lock mismatch')
    for name, payload in flash_payloads(m).items():
        if data[name] != payload:
            raise ValueError('Noncanonical flash helper/arguments')
    return m, data


def request(url, maximum=MAX_DOWNLOAD):
    parsed = urllib.parse.urlsplit(url)
    if parsed.scheme != 'https' or parsed.username or parsed.password:
        raise ValueError('Only unauthenticated HTTPS URLs are accepted')
    for attempt in range(3):
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'esp32-s31-firmware-downloader', 'Accept': 'application/vnd.github+json'})
            with urllib.request.urlopen(req, timeout=30) as response:
                if urllib.parse.urlsplit(response.url).scheme != 'https':
                    raise ValueError('Insecure redirect')
                content = response.read(maximum + 1)
                if len(content) > maximum:
                    raise ValueError('Response too large')
                return content
        except (urllib.error.URLError, TimeoutError):
            if attempt == 2:
                raise
            time.sleep(2 ** attempt)


def release_assets(repo, tag=None):
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repo):
        raise ValueError('Use --repo OWNER/REPOSITORY')
    endpoint = f'https://api.github.com/repos/{repo}/releases/'
    release = json.loads(request(endpoint + ('tags/' + urllib.parse.quote(tag, safe='') if tag else 'latest'), 8 * 1024 * 1024))
    if release['draft'] or release['prerelease']:
        raise ValueError('Only public stable Releases are accepted')
    assets = []
    page = 1
    while True:
        batch = json.loads(request(endpoint + str(release['id']) + f'/assets?per_page=100&page={page}', 8 * 1024 * 1024))
        assets.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    return release, [a for a in assets if a['name'].endswith('.zip')]


def install_asset(asset, destination, expected):
    name = asset['name']
    if Path(name).name != name or not re.fullmatch(r'[A-Za-z0-9._-]+\.zip', name):
        raise ValueError('Unsafe asset filename')
    digest = asset.get('digest', '')
    if not re.fullmatch(r'sha256:[0-9a-f]{64}', digest) or asset.get('state') != 'uploaded':
        raise ValueError('Release asset has no verified SHA-256/uploaded state')
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / name
    extracted = destination / name[:-4]
    if archive.exists() or extracted.exists():
        raise FileExistsError('Destination already exists; choose a new directory')
    payload = request(asset['browser_download_url'])
    if len(payload) != asset['size'] or sha(payload) != digest[7:]:
        raise ValueError('Downloaded asset size/hash mismatch')
    with tempfile.TemporaryDirectory(dir=destination) as temp:
        temp = Path(temp)
        staged = temp / name
        staged.write_bytes(payload)
        m, files = inspect_archive(staged)
        if canonical_name(m) != name or any(m[k] != v for k, v in expected.items()):
            raise ValueError('Downloaded firmware selection mismatch')
        tree = temp / 'contents'
        tree.mkdir()
        for n, data in files.items():
            p = tree / PurePosixPath(n)
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
        (tree / 'flash.sh').chmod(0o755)
        # Exclusive creates prevent overwriting even if another process races us.
        with archive.open('xb') as out:
            out.write(payload)
        try:
            extracted.mkdir()
        except Exception:
            archive.unlink()
            raise
        try:
            for p in tree.iterdir():
                shutil.move(str(p), extracted / p.name)
        except Exception:
            shutil.rmtree(extracted)
            archive.unlink()
            raise
    return extracted


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo', required=True)
    p.add_argument('--tag')
    p.add_argument('--list', action='store_true')
    p.add_argument('--example')
    p.add_argument('--framework', default='esp-idf')
    p.add_argument('--version', help='Exact framework version from --list')
    p.add_argument('--output', type=Path, default=Path('releases/downloads'))
    a = p.parse_args()
    try:
        release, assets = release_assets(a.repo, a.tag)
        if a.list:
            print(release['tag_name'])
            for asset in assets:
                print(asset['name'])
            return
        if not a.example or not a.version:
            raise ValueError('Download requires --example and --version; use --list first')
        prefix = f'{a.framework}-{a.example}-{a.version}-esp32s31-'
        chosen = [x for x in assets if x['name'].startswith(prefix)]
        if len(chosen) != 1:
            raise ValueError(f'Expected one matching asset, found {len(chosen)}')
        print(install_asset(chosen[0], a.output, {'framework': a.framework, 'name': a.example, 'framework_version': a.version}))
    except (ValueError, OSError, KeyError, zipfile.BadZipFile) as exc:
        p.exit(2, f'{exc}\n')



def flash_payloads(m):
    """Canonical, injection-safe helpers generated only from validated settings."""
    settings = m['flash_settings']
    allowed = {'flash_mode': {'dio', 'qio', 'dout', 'qout', 'keep'},
               'flash_size': {'1MB', '2MB', '4MB', '8MB', '16MB', '32MB', 'keep', 'detect'},
               'flash_freq': {'20m', '26m', '40m', '80m', '120m', 'keep'}}
    for k, values in allowed.items():
        if settings[k] not in values:
            raise ValueError(f'Invalid flash setting: {k}')
    if m['before'] not in ('default-reset', 'no-reset', 'usb-reset') or m['after'] not in ('hard-reset', 'soft-reset', 'no-reset', 'no-reset-stub'):
        raise ValueError('Invalid reset mode')
    if m['baud'] != 460800 or type(m['stub']) is not bool:
        raise ValueError('Invalid flash transport')
    args = ['--chip', 'esp32s31', '--baud', str(m['baud']), '--before', m['before'], '--after', m['after']]
    if not m['stub']:
        args += ['--no-stub']
    args += ['write-flash', '--flash-mode', settings['flash_mode'], '--flash-size', settings['flash_size'], '--flash-freq', settings['flash_freq'], '0x0', 'bin/combined.bin']
    command = ' '.join(args)
    shell = '''#!/bin/sh
set -eu
[ "$#" -eq 1 ] || { echo "Usage: $0 SERIAL_PORT" >&2; exit 2; }
PORT=$1
cd -- "$(dirname -- "$0")"
if command -v python >/dev/null 2>&1 && python -c 'import esptool' >/dev/null 2>&1; then
    PYTHON=python
elif command -v python3 >/dev/null 2>&1 && python3 -c 'import esptool' >/dev/null 2>&1; then
    PYTHON=python3
else
    echo "Install an ESP32-S31-capable esptool: python3 -m pip install esptool" >&2
    exit 1
fi
exec "$PYTHON" -m esptool --port "$PORT" ''' + command + '\n'
    batch = '''@echo off
setlocal
cd /d "%~dp0"
if "%~1"=="" (echo Usage: flash.bat SERIAL_PORT & exit /b 2)
if not "%~2"=="" exit /b 2
set "PORT=%~1"
python -c "import esptool" >nul 2>&1
if not errorlevel 1 goto use_python
py -3 -c "import esptool" >nul 2>&1
if not errorlevel 1 goto use_py
echo Install an ESP32-S31-capable esptool: py -3 -m pip install esptool
exit /b 1
:use_python
python -m esptool --port "%PORT%" ''' + command + '''
exit /b %errorlevel%
:use_py
py -3 -m esptool --port "%PORT%" ''' + command + '''
exit /b %errorlevel%
'''
    return {'flash.sh': shell.encode(), 'flash.bat': batch.replace('\n', '\r\n').encode(), 'flash_args.txt': (command+'\n').encode()}


if __name__ == '__main__':
    main()
