"""Verify the pinned upstream plus explicitly recorded Atlas source changes. AGPL-3.0."""
from pathlib import Path
import hashlib
import json
import subprocess


def verify(root):
    lock = json.loads((root / 'atlas/upstream.json').read_text())
    base = lock['commit']
    subprocess.run(['git', 'cat-file', '-e', base + '^{commit}'], cwd=root, check=True)
    paths = ['src', 'resources', 'deps', 'lib', 'cmake', 'tests', 'CMakeLists.txt',
             'version.inc', 'build_win.bat', 'LICENSE']
    manifest_path = root / 'atlas/source-changes.json'
    approved = json.loads(manifest_path.read_text())['sha256_lf'] if manifest_path.exists() else {}
    changed = subprocess.check_output(
        ['git', 'diff', '--name-only', base, '--', *paths], cwd=root, text=True).splitlines()
    untracked = subprocess.check_output(
        ['git', 'ls-files', '--others', '--exclude-standard', '--', *paths],
        cwd=root, text=True).splitlines()
    unexpected = (set(changed) | set(untracked)) - set(approved)
    if unexpected:
        raise SystemExit('Unexpected upstream changes: ' + ', '.join(sorted(unexpected)))
    for relative, expected in approved.items():
        source = root / relative
        if not source.is_file():
            raise SystemExit('Missing approved source: ' + relative)
        # Windows checkout may use CRLF. Compare canonical LF text on every host.
        actual = hashlib.sha256(source.read_text(encoding='utf-8').encode('utf-8')).hexdigest()
        if actual != expected:
            raise SystemExit('Source differs from recorded Atlas change: ' + relative)
    print('PASS: pinned upstream ' + base + '; ' + str(len(approved)) +
          ' recorded Atlas files verified; other protected sources unchanged')


if __name__ == '__main__':
    verify(Path(__file__).resolve().parents[2])
