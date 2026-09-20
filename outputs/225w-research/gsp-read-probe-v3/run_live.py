"""Explicit, root-only v3 probe. No GPU writes, power changes, or auto-run."""
from pathlib import Path
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys
import zipfile
from heap_evidence import extract

root = Path(__file__).resolve().parent
assert os.geteuid() == 0
manifest = json.loads((root / 'installed.json').read_text())
assert Path('/sys/module/nvidia/parameters/GspReadProbeBuild').read_text().strip() == 'heap-read-v3-20260920'
assert Path('/sys/module/nvidia/srcversion').read_text().strip() == manifest['module_srcversion']
fw = Path('/usr/lib/firmware/nvidia/615.71.09/gsp_ga10x.bin')
assert hashlib.sha256(fw.read_bytes()).hexdigest() == 'c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b'
mode = sys.argv[1]
assert mode in ('metadata', 'read')
boot_id = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
out = root / 'live' / boot_id
out.mkdir(parents=True, exist_ok=True)
assert not (out / (mode + '-run.json')).exists(), 'Already attempted; inspect saved results'
if mode == 'read':
    old = json.loads((out / 'metadata-run.json').read_text())
    assert old['boot_id'] == boot_id and old['process_exit'] == 0
    s = (out / 'metadata-probe.log').read_text()
    assert 'status=0x0 operation=0 stage=1' in s
    assert 'wpr=3ef020000..3f9c60000' in s

# Verify the *current boot's* startup table immediately before the probe.
nvzip = out / (mode + '-before-nvlog.zip')
subprocess.run(['nvidia-debugdump', '--nvlogonly', '--ioctl', '--file', str(nvzip)], check=True)
with zipfile.ZipFile(nvzip) as z:
    data = z.read('nvlog.log')
evidence = extract(data)  # Missing, conflicting, or changed ranges abort here.
(out / (mode + '-heap-evidence.json')).write_text(json.dumps(evidence, indent=2) + '\n')
env = dict(os.environ, LD_PRELOAD=str(root / 'query_probe.so'))
env.pop('CODEX_GSP_READ_HEAP', None)
if mode == 'read':
    env['CODEX_GSP_READ_HEAP'] = '1'
result = {'boot_id': boot_id, 'mode': mode, 'time': datetime.datetime.now().astimezone().isoformat(),
          'status': 'started', 'target': '0x3ef024000', 'gpu_memory_writes': 0}
runfile = out / (mode + '-run.json')
runfile.write_text(json.dumps(result, indent=2) + '\n')
with (out / (mode + '-power.txt')).open('w') as f, (out / (mode + '-probe.log')).open('w') as e:
    p = subprocess.run(['nvidia-smi', '-q', '-d', 'POWER'], env=env, stdout=f, stderr=e)
result['process_exit'] = p.returncode
log = (out / (mode + '-probe.log')).read_text()
result['status'] = 'completed'
runfile.write_text(json.dumps(result, indent=2) + '\n')
print(log)
if mode == 'read':
    with (out / 'kernel-after.log').open('w') as f:
        subprocess.run(['journalctl', '-b', '-k', '--no-pager'], stdout=f, check=True)
    subprocess.run(['nvidia-debugdump', '--nvlogonly', '--ioctl', '--file', str(out / 'read-after-nvlog.zip')], check=True)
    if 'stage=3 rpc_status=0x0 bytes=256 retained=0' in log:
        match = re.search(r' data=([0-9a-f]{512})\n', log)
        assert match
        (out / 'heap-first256.bin').write_bytes(bytes.fromhex(match.group(1)))
    else:
        raise RuntimeError('Read did not succeed; inspect logs. No retry or module unload.')
else:
    assert p.returncode == 0 and 'status=0x0 operation=0 stage=1' in log
