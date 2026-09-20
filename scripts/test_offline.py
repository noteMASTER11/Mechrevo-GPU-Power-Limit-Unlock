#!/usr/bin/env python3
"""Run pure runtime and packaging tests; optionally compile C tests against a patched source."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from package_common import ROOT

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, help='optional pinned, patched NVIDIA checkout')
    a = p.parse_args()
    env = dict(os.environ, PYTHONPATH=os.pathsep.join([str(ROOT/'src/runtime'), str(ROOT/'scripts')]))
    subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', str(ROOT/'tests'), '-p', 'test_*.py', '-v'], env=env, check=True)
    if a.source:
        source = a.source.resolve()
        includes = [ROOT/'src/runtime', source/'src/common/sdk/nvidia/inc', source/'src/nvidia/interface', source/'src/nvidia/interface/deprecated']
        with tempfile.TemporaryDirectory(prefix='max-tgp-offline-') as tmp:
            for name in ['test_ctgp', 'test_client']:
                exe = str(Path(tmp)/name)
                subprocess.run(['cc', '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', *['-I'+str(x) for x in includes], str(ROOT/'tests'/(name+'.c')), '-ldl', '-o', exe], check=True)
                subprocess.run([exe], check=True)

if __name__ == '__main__': main()
