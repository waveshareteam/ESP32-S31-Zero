#!/usr/bin/env python3
"""Reject incomplete, duplicate, stale or unexpected release sets."""
import argparse
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from discover_examples import matrix
from download_firmware import canonical_name, inspect_archive


def validate_set(directory, rows, commit):
    expected = {(r['name'], r['version']): r for r in rows}
    if not expected or len(expected) != len(rows):
        raise ValueError('Empty/duplicate required matrix')
    found = set()
    members = [p for p in directory.rglob('*') if p.is_file() or p.is_symlink()]
    if any(p.is_symlink() or p.suffix != '.zip' for p in members):
        raise ValueError('Unexpected release file or symlink')
    archives = sorted(members)
    if len(archives) != len(expected):
        raise ValueError(f'Expected {len(expected)} archives, found {len(archives)}')
    for archive in archives:
        m, _ = inspect_archive(archive)
        key = (m['name'], m['framework_version'])
        if key not in expected or key in found:
            raise ValueError('Unexpected/duplicate archive')
        r = expected[key]
        if (m['git_sha'] != commit or m['target'] != r['target'] or m['framework_commit'] != r['commit']
                or m['experimental'] != r['experimental'] or m['component_manager_version'] != r['component_manager']
                or archive.name != canonical_name(m)):
            raise ValueError('Source/target/framework/name mismatch')
        found.add(key)
    if found != set(expected):
        raise ValueError('Incomplete release matrix')
    return archives


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('--commit', required=True)
    a = p.parse_args()
    try:
        files = validate_set(a.directory, matrix()['include'], a.commit)
        print(f'Validated {len(files)} firmware packages')
    except (ValueError, OSError, KeyError) as exc:
        p.exit(2, f'{exc}\n')
