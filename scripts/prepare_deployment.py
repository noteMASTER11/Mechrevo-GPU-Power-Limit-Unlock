#!/usr/bin/env python3
"""Generate a local deployment manifest and stage runtime files; no system writes."""
import json
import shutil
import subprocess
from pathlib import Path
from package_common import ROOT, KERNEL, FIRMWARE_SHA256, MODULES, RUNTIME, NVIDIA_BASE, sha, validate_host, validate_manifest

def main():
    validate_host()
    artifacts = ROOT / 'build/artifacts'
    built = json.loads((artifacts / 'build.json').read_text())
    validate_manifest(built)
    source = Path(built['source'])
    for name, expected in built['probe_module_hashes'].items():
        if sha(source / name) != expected: raise RuntimeError(f'Module changed: {name}')
    if sha(artifacts / 'query_power.so') != built['client_sha256']:
        raise RuntimeError('Client changed')
    stock = {}
    for name in MODULES:
        path = Path(subprocess.check_output(['modinfo', '-k', KERNEL, '-n', name], text=True).strip()).resolve()
        stock[str(path)] = sha(path)
    deployment = ROOT / 'build/deployment'
    deployment.mkdir()  # Refuse to overwrite a previous preparation.
    runtime = deployment / 'runtime'
    runtime.mkdir()
    for name in RUNTIME:
        shutil.copy2(artifacts / name if name == 'query_power.so' else ROOT / 'src/runtime' / name, runtime / name)
    manifest = {**built, 'firmware_sha256': FIRMWARE_SHA256, 'stock_module_hashes': stock,
                'output': str(runtime), 'runtime_hashes': {name: sha(runtime / name) for name in RUNTIME}}
    validate_manifest(manifest, deployment=True)
    (deployment / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (deployment / 'mkinitcpio.conf').write_text('source /etc/mkinitcpio.conf\nMODULES+=(aead nvidia nvidia_modeset nvidia_uvm nvidia_drm)\n')
    print(f'Prepared {deployment}/manifest.json; no system files changed.')

if __name__ == '__main__': main()
