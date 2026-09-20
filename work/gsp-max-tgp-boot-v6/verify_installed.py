from pathlib import Path
import hashlib,json,subprocess
root=Path(__file__).resolve().parents[2];out=root/'outputs/225w-research/gsp-persistent-v6';m=json.loads((out/'manifest.json').read_text());result=json.loads((out/'installed.json').read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for name,h in m['stock_module_hashes'].items():assert sha(Path(name))==h
assert sha(Path('/boot/codex-max-tgp-v6/initramfs.img'))==result['initramfs_sha256']
conf=Path('/boot/limine.conf').read_text();before=(out/'limine-before.conf').read_text();assert conf.startswith(before)
assert conf.count('/CachyOS - NVIDIA Max TGP 225W v6')==1
assert conf.count('codex.max_tgp=225')==1
for name in ['run_boot.py','query_power.so','heap_evidence.py','transaction_signals.py']:
 p=Path('/usr/local/lib/mechrevo-max-tgp')/name
 assert sha(p)==sha(out/name) and p.stat().st_uid==0 and p.stat().st_mode&0o022==0
subprocess.run(['systemctl','restart','uccd.service'],check=True)
response=subprocess.run(['busctl','--system','--json=short','call','com.uniwill.uccd','/com/uniwill/uccd','com.uniwill.uccd','GetGPUPowerStatusJSON'],capture_output=True,text=True,check=True)
payload=json.loads(response.stdout);assert payload['type']=='s';status=json.loads(payload['data'][0]);assert status['forced'] is False
# Prove ordinary boot does not activate the enabled oneshot.
subprocess.run(['systemctl','start','mechrevo-max-tgp.service'],check=True)
condition=subprocess.check_output(['systemctl','show','mechrevo-max-tgp.service','-p','ConditionResult','--value'],text=True).strip();assert condition=='no'
health=subprocess.check_output(['nvidia-smi','--query-gpu=name,power.limit,enforced.power.limit,power.max_limit,power.draw,temperature.gpu','--format=csv'],text=True)
record={'installed_verified':True,'new_entry_booted':False,'ucc_package':subprocess.check_output(['pacman','-Q','uniwill-control-center-mechrevo'],text=True).strip(),'ucc_power_status':status,'ordinary_boot_service_condition':condition,'health':health}
(out/'final-check.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2))
