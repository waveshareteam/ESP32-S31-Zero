#!/usr/bin/env python3
"""CI-only draft/upload/verify/publish gate; refuses existing Releases."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from validate_release import validate_set
from discover_examples import matrix
from download_firmware import inspect_archive, request


def api(route, method='GET', payload=None, binary=None):
    base = 'https://uploads.github.com' if binary is not None else 'https://api.github.com'
    data = binary if binary is not None else (json.dumps(payload).encode() if payload is not None else None)
    headers = {'Authorization': 'Bearer ' + os.environ['GH_TOKEN'], 'Accept': 'application/vnd.github+json',
               'User-Agent': 's31-release-gate', 'X-GitHub-Api-Version': '2022-11-28',
               'Content-Type': 'application/zip' if binary is not None else 'application/json'}
    with urllib.request.urlopen(urllib.request.Request(base + route, data=data, headers=headers, method=method), timeout=60) as r:
        return json.load(r)


def git(*args):
    return subprocess.check_output(['git', *args], text=True).strip()


def main():
    tag = os.environ['GITHUB_REF_NAME']
    repo = os.environ['GITHUB_REPOSITORY']
    if os.environ.get('GITHUB_REF_TYPE') != 'tag' or not re.fullmatch(r'v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', tag):
        raise ValueError('Release requires a stable vMAJOR.MINOR.PATCH tag')
    commit = git('rev-parse', 'HEAD')
    if git('rev-parse', f'{tag}^{{commit}}') != commit or git('rev-parse', os.environ['GITHUB_SHA']+'^{commit}') != commit:
        raise ValueError('Tag/event/checkout mismatch')
    metadata = api(f'/repos/{repo}')
    default = metadata['default_branch']
    subprocess.run(['git', 'fetch', 'origin', f'refs/heads/{default}:refs/remotes/origin/{default}'], check=True)
    subprocess.run(['git', 'merge-base', '--is-ancestor', commit, f'refs/remotes/origin/{default}'], check=True)
    # Re-read remote tag immediately before publication; do not trust stale local refs.
    remote = git('ls-remote', 'origin', f'refs/tags/{tag}', f'refs/tags/{tag}^{{}}').splitlines()
    refs = dict(line.split()[::-1] for line in remote)
    if refs.get(f'refs/tags/{tag}^{{}}', refs.get(f'refs/tags/{tag}')) != commit:
        raise ValueError('Remote tag moved')
    files = validate_set(Path('release-artifacts'), matrix()['include'], commit)
    try:
        api(f'/repos/{repo}/releases/tags/{tag}')
    except urllib.error.HTTPError as exc:
        if exc.code != 404:
            raise
    else:
        raise ValueError('Release already exists; public versions are never overwritten. Review an existing draft manually.')
    release = api(f'/repos/{repo}/releases', 'POST', {
        'tag_name': tag, 'target_commitish': commit, 'name': tag, 'draft': True, 'prerelease': False,
        'body': f'Source `{commit}`. All {len(files)} configured packages passed build and archive gates.\n\n'
                'ESP32-S31 support may be experimental; see each manifest and config/frameworks.json. '
                'Build success does not establish physical-board functionality. No hardware test is implied.'})
    rid = release['id']
    expected = {}
    for file in files:
        data = file.read_bytes()
        expected[file.name] = (len(data), 'sha256:' + hashlib.sha256(data).hexdigest())
        api(f'/repos/{repo}/releases/{rid}/assets?name=' + urllib.parse.quote(file.name), 'POST', binary=data)
    assets = []
    page = 1
    while True:
        batch = api(f'/repos/{repo}/releases/{rid}/assets?per_page=100&page={page}')
        assets.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    if len(assets) != len(expected) or {a['name'] for a in assets} != set(expected):
        raise ValueError('Remote draft asset set mismatch; draft left unpublished')
    for a in assets:
        if a['state'] != 'uploaded' or (a['size'], a.get('digest')) != expected[a['name']]:
            raise ValueError('Remote asset digest/size/state mismatch; draft left unpublished')
    current = api(f'/repos/{repo}/releases/{rid}')
    if not current['draft']:
        raise ValueError('Draft unexpectedly became public')
    published = api(f'/repos/{repo}/releases/{rid}', 'PATCH', {'draft': False})
    if published['draft'] or published['prerelease'] or published['tag_name'] != tag:
        raise ValueError('Public Release metadata mismatch')
    # Independent unauthenticated read and fresh representative download.
    public = json.loads(request(f'https://api.github.com/repos/{repo}/releases/tags/{tag}'))
    if public['draft'] or public['prerelease']:
        raise ValueError('Public Release verification failed')
    public_assets = []
    page = 1
    while True:
        batch = json.loads(request(f'https://api.github.com/repos/{repo}/releases/{rid}/assets?per_page=100&page={page}'))
        public_assets.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    if (len(public_assets) != len(expected)
            or {a['name'] for a in public_assets} != set(expected)
            or any(a['state'] != 'uploaded' or (a['size'], a.get('digest')) != expected[a['name']] for a in public_assets)):
        raise ValueError('Public asset set differs from validated draft')
    representative = sorted(public_assets, key=lambda x: x['name'])[0]
    data = request(representative['browser_download_url'])
    if 'sha256:' + hashlib.sha256(data).hexdigest() != expected[representative['name']][1]:
        raise ValueError('Public download digest mismatch')
    with tempfile.TemporaryDirectory() as temp:
        file = Path(temp) / representative['name']
        file.write_bytes(data)
        inspect_archive(file)
    print(f'Published and independently verified {tag}: {len(files)} packages')


if __name__ == '__main__':
    main()
