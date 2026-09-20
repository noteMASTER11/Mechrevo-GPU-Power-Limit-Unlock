#!/usr/bin/env python3
"""Build pinned NVIDIA modules and client locally; never install or load them."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
from package_common import ROOT, NVIDIA_BASE, KERNEL, MODULES, sha

def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)

def prepare_source(source):
    patch = ROOT / 'patches/nvidia-gsp-persistent-v6.patch'
    if not source.exists():
        source.parent.mkdir(parents=True, exist_ok=True)
        run('git', 'clone', 'https://github.com/NVIDIA/open-gpu-kernel-modules.git', str(source))
        run('git', 'checkout', '--detach', NVIDIA_BASE, cwd=source)
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip()
    if head != NVIDIA_BASE:
        raise RuntimeError(f'Source HEAD must be {NVIDIA_BASE}; found {head}')
    if subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], cwd=source).strip():
        raise RuntimeError('Untracked source files present; use a clean checkout')
    diff = subprocess.check_output(['git', 'diff', 'HEAD', '--binary'], cwd=source)
    if not diff:
        run('git', 'apply', '--check', str(patch), cwd=source)
        run('git', 'apply', '--index', str(patch), cwd=source)
    # Build the expected tree separately: patch text order is not canonical.
    with tempfile.TemporaryDirectory(prefix='max-tgp-index-') as tmp:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(tmp) / 'index'))
        subprocess.run(['git', 'read-tree', NVIDIA_BASE], cwd=source, env=env, check=True)
        subprocess.run(['git', 'apply', '--cached', str(patch)], cwd=source, env=env, check=True)
        tree = subprocess.check_output(['git', 'write-tree'], cwd=source, env=env, text=True).strip()
    if subprocess.run(['git', 'diff', '--quiet', tree, '--'], cwd=source).returncode:
        raise RuntimeError('Existing checkout differs from the exact v6 patch; use a clean checkout')

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, default=ROOT / 'build/nvidia')
    p.add_argument('--jobs', type=int, default=min(os.cpu_count() or 2, 12))
    p.add_argument('--prepare-only', action='store_true', help='clone/check/apply patch, without compiling')
    a = p.parse_args()
    if a.jobs < 1: p.error('--jobs must be positive')
    source = a.source.resolve()
    prepare_source(source)
    if a.prepare_only: return
    if not Path('/usr/lib/modules', KERNEL, 'build').is_dir():
        raise RuntimeError(f'Install matching {KERNEL} kernel headers before building')
    out = ROOT / 'build/artifacts'
    out.mkdir(parents=True, exist_ok=True)
    run('make', f'-j{a.jobs}', 'CC=clang', 'LD=ld.lld', f'KERNEL_UNAME={KERNEL}', 'modules', cwd=source)
    run('cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra',
        '-I' + str(source / 'src/common/sdk/nvidia/inc'),
        '-I' + str(source / 'src/nvidia/arch/nvalloc/common/inc'),
        '-I' + str(source / 'src/nvidia/interface'),
        str(ROOT / 'src/runtime/query_power.c'), '-ldl', '-o', str(out / 'query_power.so'))
    manifest = {'nvidia_base': NVIDIA_BASE, 'kernel': KERNEL, 'source': str(source / 'kernel-open'),
                'probe_module_hashes': {n + '.ko': sha(source / 'kernel-open' / (n + '.ko')) for n in MODULES},
                'client_sha256': sha(out / 'query_power.so'),
                'patch_sha256': sha(ROOT / 'patches/nvidia-gsp-persistent-v6.patch')}
    (out / 'build.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Built artifacts and manifest: {out}; nothing installed or loaded.')

if __name__ == '__main__': main()
