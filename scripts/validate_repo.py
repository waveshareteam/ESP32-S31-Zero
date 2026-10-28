#!/usr/bin/env python3
"""Static repository checks; never configures, builds or flashes firmware."""
import ast
import hashlib
import json
from pathlib import Path
import re
import subprocess
from discover_examples import ROOT, discover, matrix


def main():
    projects = discover()
    assert len(projects) == 12, 'Inventory count changed: update the approved implementation inventory'
    config = json.loads((ROOT/'config/frameworks.json').read_text())
    board = json.loads((ROOT/'config/board.json').read_text())
    assert board['target'] == config['target'] == 'esp32s31'
    for row in projects:
        project = ROOT/row['path']
        assert (project/'README.md').is_file(), f'Missing example README: {row["name"]}'
        cmake = (project/'CMakeLists.txt').read_text()
        assert 'set(IDF_TARGET "esp32s31"' in cmake
        defaults = (project/'sdkconfig.defaults').read_text()
        assert 'CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y' in defaults
        assert 'CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y' in defaults
        for readme in ['README.md','README_ZH.md','docs/examples.md']:
            assert row['name'] in (ROOT/readme).read_text(), f'Example missing from {readme}'
    if config['frameworks']:
        rows = matrix()['include']
        assert config['support_status'] == 'experimental_authorized'
        assert len(rows) == 12 and all(r['experimental'] is True for r in rows)
        approved = {c['commit'] for c in config['candidates'] if c['status'] == 'authorized_experimental_lane'}
        assert {r['commit'] for r in rows} <= approved
        print('Build matrix: 12 required experimental jobs configured; not a build-pass claim')
    else:
        assert config['support_status'] == 'blocked_pending_preview_authorization'
        print('Build matrix: explicitly blocked; no supported/approved lane (not a build pass)')
    for base in ['scripts','releases','tests']:
        for f in (ROOT/base).glob('*.py'):
            ast.parse(f.read_text(), filename=str(f.relative_to(ROOT)))
    ignored = {'build','managed_components','.git','.ci','__pycache__'}
    text_files = [p for p in ROOT.rglob('*') if p.is_file() and not ignored.intersection(p.relative_to(ROOT).parts)
                  and p.suffix in ('.md','.json','.cmake','.yml','.yaml','.c','.h')]
    # Public sources/docs must not expose machine paths, private endpoints or key material.
    forbidden = re.compile(r'/home/[^/\s]+/|/Users/[^/\s]+/|[A-Z]:\\(?:Users|Work)\\|https?://(?:192\.168\.|10\.|127\.)|-----BEGIN (?:RSA |OPENSSH )?PRIVATE KEY-----')
    for p in text_files:
        if p.name == 'sdkconfig':continue
        text = p.read_text()
        assert not forbidden.search(text), f'Private machine data: {p.relative_to(ROOT)}'
        if p.suffix == '.md':
            for target in re.findall(r'(?<!!)\[[^\]]*\]\(([^)]+)\)', text):
                if '://' in target or target.startswith('#'):continue
                target = target.split('#')[0]
                assert (p.parent/target).exists(), f'Broken relative link: {p.relative_to(ROOT)} -> {target}'
    factory = ROOT/'firmware/ESP32-S31-Zero-Test.bin'
    assert hashlib.sha256(factory.read_bytes()).hexdigest() == '1654bcb86a04789e5f0167b31623b2e18c2f5f9acd0226039e229d8151d6b1f4', 'Factory binary changed'
    tracked = subprocess.check_output(['git','ls-files'],cwd=ROOT,text=True).splitlines()
    for name in tracked:
        parts=Path(name).parts
        assert not {'build','managed_components','__pycache__','.ci'}.intersection(parts), f'Generated path tracked: {name}'
        assert Path(name).name not in ('sdkconfig','sdkconfig.old','dependencies.lock'), f'Local configuration tracked: {name}'
    subprocess.run(['git','diff','--check'],cwd=ROOT,check=True)
    print(f'Static repository checks passed: {len(projects)} existing projects; no compilation or board test implied')


if __name__ == '__main__':main()
