#!/usr/bin/env python3
"""Build and verify an isolated initramfs; append an opt-in 225 W boot entry.
Installer performs no power changes, GSP commands, driver unloading, or reboot.
"""
from pathlib import Path
import hashlib,json,os,re,shutil,subprocess,tempfile
if not __debug__:raise RuntimeError('Do not disable installer validation')
W=Path(__file__).resolve().parent
M=json.loads((W/'manifest.json').read_text());O=Path(M['output'])
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def b2(p):return hashlib.blake2b(p.read_bytes()).hexdigest()
assert os.geteuid()==0 and os.uname().release==M['kernel']
assert sha(Path('/usr/lib/firmware/nvidia/615.71.09/gsp_ga10x.bin'))==M['firmware_sha256']
assert M['firmware_sha256']=='c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b'
for name,h in M['stock_module_hashes'].items():assert sha(Path(name))==h
for name,h in M['probe_module_hashes'].items():assert sha(Path(M['source'])/name)==h
root=W/'modules-root';destmods=root/'lib/modules'/M['kernel']
assert not root.exists()
destmods.parent.mkdir(parents=True)
subprocess.run(['cp','-a','--reflink=auto','/usr/lib/modules/'+M['kernel'],str(destmods)],check=True)
for p in list(destmods.rglob('nvidia*.ko*')):
 if p.name.split('.ko')[0] in ('nvidia','nvidia-modeset','nvidia-uvm','nvidia-drm','nvidia-peermem'):p.unlink()
(destmods/'extramodules').mkdir(exist_ok=True)
for name in M['probe_module_hashes']:shutil.copy2(Path(M['source'])/name,destmods/'extramodules'/name)
subprocess.run(['depmod','-b',str(root),M['kernel']],check=True)
image=W/'initramfs.img'
with (O/'mkinitcpio.log').open('w') as log:
 subprocess.run(['mkinitcpio','-k',M['kernel'],'-r',str(root),'-c',str(W/'mkinitcpio.conf'),'-g',str(image),'--nopost'],check=True,stdout=log,stderr=subprocess.STDOUT)
log=(O/'mkinitcpio.log').read_text();assert 'Initcpio image generation successful' in log and '==> ERROR:' not in log
with tempfile.TemporaryDirectory(prefix='gsp-read-verify-') as tmp:
 subprocess.run(['lsinitcpio','-x',str(image)],cwd=tmp,check=True,stdout=subprocess.DEVNULL)
 r=Path(tmp);assert (r/'init').exists()
 for name,h in M['probe_module_hashes'].items():assert sha(r/'usr/lib/modules'/M['kernel']/'extramodules'/name)==h
 for p in r.rglob('*'):
  assert 'identity-probe' not in str(p) and 'vbios-identity' not in str(p)
 deps=subprocess.run(['modprobe','-d',str(r),'-S',M['kernel'],'--show-depends','nvidia_drm'],check=True,capture_output=True,text=True)
 (O/'dependency-check.log').write_text(deps.stdout+deps.stderr)
 assert 'ERROR' not in deps.stderr
 assert sha(r/'usr/lib/firmware/nvidia/615.71.09/gsp_ga10x.bin')==M['firmware_sha256']
conf=Path('/boot/limine.conf');original=conf.read_text()
assert '# BEGIN CODEX MANAGED: MAX TGP V6' not in original
match=re.search(r'^  //linux-cachyos\n(.*?)(?=^  //|^/|\Z)',original,re.M|re.S);assert match
block=match.group(1)
kpath=re.search(r'^  path: boot\(\):(/[^#\n]+)#([0-9a-f]+)$',block,re.M);assert kpath
cmd=re.search(r'^  cmdline: (.+)$',block,re.M);assert cmd
assert 'VbiosIdentity' not in cmd.group(1) and 'codex.max_tgp' not in cmd.group(1)
kernel=Path('/boot'+kpath.group(1));assert b2(kernel)==kpath.group(2)
dest=Path('/boot/codex-max-tgp-v6');assert not dest.exists();dest.mkdir(mode=0o700)
(O/'limine-before.conf').write_text(original)
shutil.copy2(kernel,dest/'vmlinuz');shutil.copy2(image,dest/'initramfs.img')
entry="""\n# BEGIN CODEX MANAGED: MAX TGP V6
/CachyOS - NVIDIA Max TGP 225W v6
  comment: Experimental automatic 225W request; UCC yields GPU power control. Recovery: ordinary CachyOS.
  protocol: linux
  path: boot():/codex-max-tgp-v6/vmlinuz#%s
  module_path: boot():/codex-max-tgp-v6/initramfs.img#%s
  cmdline: %s codex.max_tgp=225
# END CODEX MANAGED: MAX TGP V6
"""%(b2(dest/'vmlinuz'),b2(dest/'initramfs.img'),cmd.group(1))
result={'status':'prepared_not_booted','entry':'CachyOS - NVIDIA Max TGP 225W v6','kernel':M['kernel'],'actual_225w_draw_verified':False,'initramfs_sha256':sha(dest/'initramfs.img'),'config_before_sha256':hashlib.sha256(original.encode()).hexdigest(),'stock_modules_unchanged':True,'module_srcversion':subprocess.check_output(['modinfo','-F','srcversion',str(Path(M['source'])/'nvidia.ko')],text=True).strip()}
(O/'installed.json').write_text(json.dumps(result,indent=2)+'\n')
helpers=Path('/usr/local/lib/mechrevo-max-tgp');assert not helpers.exists();helpers.mkdir(mode=0o755)
for name in ['run_boot.py','query_power.so','heap_evidence.py','transaction_signals.py','installed.json']:
 shutil.copy2(O/name,helpers/name);os.chmod(helpers/name,0o644)
unit=Path('/etc/systemd/system/mechrevo-max-tgp.service');assert not unit.exists()
shutil.copy2(O/'mechrevo-max-tgp.service',unit);os.chmod(unit,0o644)
subprocess.run(['systemctl','daemon-reload'],check=True)
subprocess.run(['systemctl','enable','mechrevo-max-tgp.service'],check=True)
assert conf.read_text()==original
pending=conf.with_name('limine.conf.max-tgp-new');assert not pending.exists()
pending.write_text(original+entry);os.chmod(pending,conf.stat().st_mode&0o777);os.replace(pending,conf)
assert conf.read_text()==original+entry
for name,h in M['stock_module_hashes'].items():assert sha(Path(name))==h
result['config_after_sha256']=sha(conf)
(O/'installed.json').write_text(json.dumps(result,indent=2)+'\n')
shutil.copy2(O/'installed.json',helpers/'installed.json');os.chmod(helpers/'installed.json',0o644)
print(json.dumps(result,indent=2))
