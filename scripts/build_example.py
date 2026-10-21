#!/usr/bin/env python3
"""Explicit build entry point. Does not flash; --configure-only never compiles."""
import argparse
from pathlib import Path
import os
import subprocess
from discover_examples import ROOT, matrix


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('selector')
    p.add_argument('--version', required=True)
    p.add_argument('--configure-only', action='store_true')
    a = p.parse_args()
    try:
        rows = [r for r in matrix(selector=a.selector)['include'] if r['version'] == a.version]
        if not rows:
            raise ValueError('Version is not in the approved matrix')
        idf = Path(os.environ['IDF_PATH']).resolve()
        revision = subprocess.check_output(['git', '-C', str(idf), 'rev-parse', 'HEAD'], text=True).strip()
        for row in rows:
            if revision != row['commit']:
                raise ValueError('Active ESP-IDF commit does not match config/frameworks.json')
            project = ROOT / row['path']
            # Existing user sdkconfig is never overwritten. CI uses a clean checkout.
            if (project / 'sdkconfig').exists() and not (project / 'build').exists():
                raise ValueError('Existing sdkconfig: preserve it outside this project before a fresh configured build')
            command = ['idf.py'] + (['--preview'] if row['experimental'] else [])
            subprocess.run(command + ['-C', str(project), '-D', f"IDF_TARGET={row['target']}",
                                      'reconfigure' if a.configure_only else 'build'], check=True)
    except (ValueError, KeyError, OSError) as exc:
        p.exit(2, f'{exc}\n')


if __name__ == '__main__':
    main()
