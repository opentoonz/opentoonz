import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile


def run(*args, env=None):
    result = subprocess.run(args, capture_output=True, env=env)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors='replace')[-4000:])
    return result.stdout


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def verify_patch(git, base, target_tree, patch):
    with tempfile.TemporaryDirectory() as temp:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(temp) / 'index'))
        git('read-tree', base, env=env)
        git('apply', '--cached', '--check', str(patch), env=env)
        git('apply', '--cached', '--whitespace=nowarn', str(patch), env=env)
        assert git('write-tree', env=env).decode().strip() == target_tree
        git('apply', '--cached', '--reverse', '--whitespace=nowarn', str(patch), env=env)
        assert git('write-tree', env=env) == git('rev-parse', base + '^{tree}')


def prepare(args):
    data = json.loads(args.inventory.read_text())
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)

    def git(*items, **kwargs):
        return run('git', '-C', str(args.repo), *items, **kwargs)

    main = data['source_main']
    upstream = data['upstream_master']
    base = data['snapshot_upstream_base']
    refs = {'refs/heads/preserved/main': main,
            'refs/heads/preserved/upstream-master': upstream}
    for pr in data['prs']:
        assert isinstance(pr['number'], int) and pr['number'] > 0
        refs['refs/heads/preserved/pr-' + str(pr['number'])] = pr['head_sha']
    assert len(refs) == len(data['prs']) + 2
    assert all(re.fullmatch('[0-9a-f]{40}', sha) for sha in refs.values())
    if args.fetch:
        git('fetch', '--no-tags', 'origin',
            *['+' + sha + ':' + ref for ref, sha in refs.items()])
    assert git('rev-parse', '--is-shallow-repository').strip() == b'false'
    for ref, sha in refs.items():
        assert git('rev-parse', ref).decode().strip() == sha, ref

    main_tree = git('rev-parse', main + '^{tree}').decode().strip()
    assert main_tree == data['source_main_tree']
    merge = git('merge-tree', '--write-tree', '--merge-base=' + base, main, upstream)
    edition_tree = merge.decode().splitlines()[0]
    assert edition_tree == data['candidate_edition_tree']

    bundle = output / 'OTDev-Preservation-2026-10-18.bundle'
    git('bundle', 'create', str(bundle), *refs)
    with tempfile.TemporaryDirectory() as temp:
        restored = Path(temp) / 'restored.git'
        run('git', 'init', '--bare', '--quiet', str(restored))
        run('git', '-C', str(restored), 'bundle', 'verify', str(bundle))
        run('git', '-C', str(restored), 'fetch', '--quiet', str(bundle),
            *['+' + ref + ':' + ref for ref in refs])
        run('git', '-C', str(restored), 'fsck', '--full', '--no-dangling')
        for ref, sha in refs.items():
            assert run('git', '-C', str(restored), 'rev-parse', ref).decode().strip() == sha

    with tempfile.TemporaryDirectory() as temp:
        package = Path(temp)
        patches = []
        for name, source, target in [
                ('otdev-preserved-main.patch', base, main_tree),
                ('otdev-edition-on-current-upstream.patch', upstream, edition_tree)]:
            patch = package / name
            patch.write_bytes(git('diff', '--binary', '--full-index', '--no-renames',
                                  '--no-ext-diff', '--no-textconv', source, target))
            verify_patch(git, source, target, patch)
            patches.append({'file': name, 'base_commit': source,
                            'target_tree': target, 'sha256': digest(patch)})
        manifest = {'source_main': main, 'source_main_tree': main_tree,
                    'snapshot_upstream_base': base, 'upstream_master': upstream,
                    'candidate_edition_tree': edition_tree, 'patches': patches,
                    'verified': {'forward_and_reverse_tree_equality': True,
                                 'standalone_bundle_restore': True,
                                 'bundle_full_fsck': True,
                                 'preserved_pr_heads': len(data['prs'])},
                    'candidate_edition_compiled': False}
        (package / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        (package / 'pr-preservation-inventory.json').write_text(
            json.dumps(data, indent=2) + '\n')
        (package / 'README.txt').write_text(
            'OT-Dev preservation, 2026-10-18\n\n'
            'Preserved main: ' + main + '\n'
            'Snapshot upstream base: ' + base + '\n'
            'Current upstream base: ' + upstream + '\n\n'
            'Apply each patch only to its pinned base in a clean worktree:\n'
            'git apply --check PATH-TO-PATCH\n'
            'git apply --index --whitespace=nowarn PATH-TO-PATCH\n'
            'git write-tree\n'
            'Compare the tree to manifest.json. Both patches were verified\n'
            'forward and reverse. The current-upstream Edition is uncompiled.\n\n'
            'The separate Git bundle preserves full Git histories of main,\n'
            'the upstream base, and all 83 observed open PR heads. Restore with:\n'
            'git clone -b preserved/main OTDev-Preservation-2026-10-18.bundle recovered\n'
            'Each PR is retained as preserved/pr-NUMBER. The PRs are independent\n'
            'branches; their code is not merged into the preserved Edition.\n'
            'Git LFS payloads remain in the original repository LFS storage.\n\n'
            'The Windows ZIP is the existing successful main CI artifact,\n'
            'verified against the recorded SHA-256; it is not a new build.\n'
            'See SHA256SUMS.txt for release asset checksums.\n\n'
            'Development: use upstream/master as the exact clean mirror;\n'
            'keep existing PR branches and bases; send focused fixes upstream;\n'
            'inherit accepted changes through mirror synchronization.\n'
            'https://github.com/OpenAnimationLibrary/opentoonz-dev/blob/edition/CONTRIBUTING.md\n')
        archive = output / 'OTDev-Edition-Patches-2026-10-18.zip'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
            for path in sorted(package.iterdir()):
                zipped.write(path, arcname=path.name)
        with zipfile.ZipFile(archive) as zipped:
            assert zipped.testzip() is None

    assert args.windows.stat().st_size == data['windows_artifact']['bytes']
    assert digest(args.windows) == data['windows_artifact']['sha256']
    with zipfile.ZipFile(args.windows) as zipped:
        assert zipped.testzip() is None
    windows = output / 'OpenToonz-OTDev-Preserved-2026-10-18-Windows.zip'
    if args.windows.resolve() != windows:
        shutil.copyfile(args.windows, windows)
    assets = [archive, bundle, windows]
    (output / 'SHA256SUMS.txt').write_text(
        ''.join(digest(path) + '  ' + path.name + '\n' for path in sorted(assets)))
    (output / 'release-notes.md').write_text(
        'Preserved OT-Dev Edition before the transition to upstream-first development.\n\n'
        '- Source: `' + main + '`.\n'
        '- Windows package: successful [main CI build](https://github.com/OpenAnimationLibrary/opentoonz-dev/actions/runs/39110540543), verified by SHA-256.\n'
        '- Source patches: exact main snapshot and a candidate on upstream `' + upstream + '`.\n'
        '- Git bundle: full histories of main and all 83 observed open PR heads; restored and checked with `git fsck`.\n'
        '- Existing PR heads and base branches are retained. Unmerged PR code stays on separate branches.\n\n'
        'The candidate Edition patch on current upstream was not compiled. '
        'This is an experimental preservation release, not an official OpenToonz release.\n\n'
        'New focused work targets `upstream/master`; accepted changes return from OT-Master. '
        'See [workflow guidance](https://github.com/OpenAnimationLibrary/opentoonz-dev/blob/edition/CONTRIBUTING.md).\n')
    print(json.dumps({'assets': [str(p) for p in assets],
                      'preserved_prs': len(data['prs']), 'verification': 'passed'}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, default=Path('.'))
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--windows', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fetch', action='store_true')
    prepare(parser.parse_args())
