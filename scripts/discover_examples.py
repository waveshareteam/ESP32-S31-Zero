#!/usr/bin/env python3
"""Discover only direct first-party ESP-IDF projects; fail closed."""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXCLUDED_WORDS = {'backup', 'backups', 'archive', 'archived', 'vendor', 'thirdparty', 'downloads', 'download', 'build', 'managed', 'factory'}
NAME = re.compile(r'[0-9]{2}_[a-z0-9]+(?:_[a-z0-9]+)*')


def discover(root=ROOT, selector='all'):
    base = root / 'examples/esp-idf'
    projects = []
    names = set()
    for path in sorted(base.iterdir()):
        if EXCLUDED_WORDS.intersection(path.name.lower().split('_')):
            continue
        if not path.is_dir() or path.is_symlink() or not (path / 'CMakeLists.txt').is_file():
            continue
        if not NAME.fullmatch(path.name):
            raise ValueError(f'Invalid first-party name: {path.name}')
        if path.name in names:
            raise ValueError(f'Duplicate name: {path.name}')
        names.add(path.name)
        cmake = (path / 'CMakeLists.txt').read_text()
        if re.findall(r'project\(([^)]+)\)', cmake) != [path.name]:
            raise ValueError(f'project() mismatch: {path.name}')
        if not (path / 'main/CMakeLists.txt').is_file():
            raise ValueError(f'Missing main component: {path.name}')
        projects.append({'name': path.name, 'path': path.relative_to(root).as_posix()})
    if len({p['name'].split('_')[0] for p in projects}) != len(projects):
        raise ValueError('Duplicate example number')
    if not projects:
        raise ValueError('Empty first-party project set')
    selected = projects if selector == 'all' else [p for p in projects if selector in (p['name'], p['path'])]
    if not selected:
        raise ValueError(f'Invalid selector: {selector!r}')
    return selected


def matrix(root=ROOT, selector='all'):
    projects = discover(root, selector)
    config = json.loads((root / 'config/frameworks.json').read_text())
    lanes = config['frameworks']
    if not lanes:
        raise ValueError('Build blocked: no authorized, target-supported framework matrix')
    rows = []
    for lane in lanes:
        if not re.fullmatch(r'[0-9a-f]{40}', lane['commit']):
            raise ValueError('Framework must be pinned to a full commit')
        if not re.fullmatch(r'[a-zA-Z0-9._-]+', lane['version']):
            raise ValueError('Invalid framework version')
        for project in projects:
            rows.append(dict(project, **lane, target=config['target'], framework='esp-idf'))
    if len({(r['name'], r['version']) for r in rows}) != len(rows):
        raise ValueError('Duplicate matrix entry')
    return {'include': rows}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('selector', nargs='?', default='all')
    parser.add_argument('--matrix', action='store_true')
    args = parser.parse_args()
    try:
        print(json.dumps(matrix(selector=args.selector) if args.matrix else discover(selector=args.selector)))
    except (ValueError, KeyError, OSError) as exc:
        parser.exit(2, f'{exc}\n')
