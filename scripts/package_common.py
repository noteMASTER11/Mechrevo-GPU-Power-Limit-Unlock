"""Pinned build/deployment metadata. Host checks are read-only."""
from pathlib import Path
import hashlib
import platform
import subprocess

ROOT = Path(__file__).resolve().parents[1]
NVIDIA_BASE = '61dcc93722ecb418bb5f2e00923f05b4b8051dd1'
KERNEL = '7.2.6-1-cachyos'
FIRMWARE = Path('/usr/lib/firmware/nvidia/615.71.09/gsp_ga10x.bin')
FIRMWARE_SHA256 = 'c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b'
MODULES = ('nvidia', 'nvidia-modeset', 'nvidia-uvm', 'nvidia-drm')
RUNTIME = ('run_boot.py', 'heap_evidence.py', 'transaction_signals.py', 'query_power.so', 'mechrevo-max-tgp.service')

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def validate_host():
    if platform.release() != KERNEL:
        raise RuntimeError(f'Only tested kernel {KERNEL} is supported')
    if 'ID=cachyos' not in Path('/etc/os-release').read_text().splitlines():
        raise RuntimeError('Only the tested CachyOS installation is supported')
    if sha(FIRMWARE) != FIRMWARE_SHA256:
        raise RuntimeError('Firmware fingerprint mismatch')
    versions = subprocess.check_output(['nvidia-smi', '--query-gpu=driver_version', '--format=csv,noheader'], text=True).splitlines()
    if versions != ['615.71.09']:
        raise RuntimeError(f'Expected NVIDIA userspace/driver 615.71.09; found {versions}')
    gpus = []
    for device in Path('/sys/bus/pci/devices').iterdir():
        if (device / 'vendor').read_text().strip() == '0x10de' and (device / 'class').read_text().strip().startswith('0x03'):
            gpus.append(tuple((device / name).read_text().strip() for name in ('vendor', 'device', 'subsystem_vendor', 'subsystem_device')))
    if gpus != [('0x10de', '0x2c19', '0x1d05', '0x6041')]:
        raise RuntimeError(f'Expected exactly the tested NVIDIA GPU/subsystem; found {gpus}')

def validate_manifest(manifest, deployment=False):
    """Reject incomplete or path-traversing local manifests before privileged work."""
    import re
    if manifest.get('nvidia_base') != NVIDIA_BASE or manifest.get('kernel') != KERNEL:
        raise RuntimeError('Build metadata mismatch')
    if manifest.get('patch_sha256') != sha(ROOT / 'patches/nvidia-gsp-persistent-v6.patch'):
        raise RuntimeError('Build patch mismatch')
    def hashes(field, names):
        value = manifest.get(field)
        if not isinstance(value, dict) or set(value) != set(names):
            raise RuntimeError(f'Unexpected or missing {field} entries')
        if any(not isinstance(h, str) or not re.fullmatch('[0-9a-f]{64}', h) for h in value.values()):
            raise RuntimeError(f'Invalid {field} digest')
    hashes('probe_module_hashes', [name + '.ko' for name in MODULES])
    if not isinstance(manifest.get('client_sha256'), str) or not re.fullmatch('[0-9a-f]{64}', manifest['client_sha256']):
        raise RuntimeError('Invalid client digest')
    if not isinstance(manifest.get('source'), str) or not Path(manifest['source']).is_absolute():
        raise RuntimeError('Source must be an absolute path')
    if not deployment: return
    if manifest.get('firmware_sha256') != FIRMWARE_SHA256:
        raise RuntimeError('Firmware fingerprint mismatch')
    hashes('runtime_hashes', RUNTIME)
    if manifest['runtime_hashes']['query_power.so'] != manifest['client_sha256']:
        raise RuntimeError('Client/runtime digest mismatch')
    if not isinstance(manifest.get('output'), str) or not Path(manifest['output']).is_absolute():
        raise RuntimeError('Runtime output must be an absolute path')
    stock = manifest.get('stock_module_hashes')
    if not isinstance(stock, dict) or len(stock) != len(MODULES):
        raise RuntimeError('Unexpected or missing stock_module_hashes entries')
    names = []
    for name in stock:
        path = Path(name)
        if not path.is_absolute() or '..' in path.parts or not (path.is_relative_to(Path('/usr/lib/modules') / KERNEL) or path.is_relative_to(Path('/lib/modules') / KERNEL)):
            raise RuntimeError('Stock module outside tested kernel directory')
        match = re.fullmatch(r'(nvidia(?:-modeset|-uvm|-drm)?)\.ko(?:\.(?:zst|xz|gz))?', path.name)
        if not match: raise RuntimeError('Unexpected stock module name')
        names.append(match.group(1))
    if set(names) != set(MODULES):
        raise RuntimeError('Unexpected or missing stock module names')
    hashes('stock_module_hashes', stock)
