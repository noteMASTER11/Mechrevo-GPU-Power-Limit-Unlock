"""Explicit bounded v5 experiment. No load, CURRENT, rail or voltage changes.
inspect is read-only; exercise writes identity, raises three ceilings, reads public
INFO, restores them, and verifies INFO again. No automatic retries on any error.
"""
from pathlib import Path
import argparse,datetime,hashlib,json,os,subprocess,zipfile
from heap_evidence import extract
from transaction_signals import blocked_signals
if not __debug__:raise RuntimeError('Python optimization disables validation; refused')
root=Path(__file__).resolve().parent
p=argparse.ArgumentParser(description=__doc__);p.add_argument('mode',choices=['inspect','exercise','restore'])
for name,default in [('gpu',0x196bb0),('pmgr',0x380690),('board',0x3bc610)]:p.add_argument('--'+name+'-offset',type=lambda x:int(x,0),default=default)
a=p.parse_args()
if os.geteuid()!=0:raise RuntimeError('Run as root')
assert Path('/sys/module/nvidia/parameters/GspReadProbeBuild').read_text().strip()=='power-words-v5-20260920','v5 is not loaded'
installed=json.loads((root/'installed.json').read_text());src=Path('/sys/module/nvidia/srcversion').read_text().strip();assert src==installed['module_srcversion']
assert hashlib.sha256(Path('/usr/lib/firmware/nvidia/615.71.09/gsp_ga10x.bin').read_bytes()).hexdigest()=='c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b'
offsets={k:getattr(a,k+'_offset') for k in ('gpu','pmgr','board')}
assert all(0<=v<0x73dc000 and v%8==0 for v in offsets.values())
boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip();out=root/'live'/boot
owner=(root.stat().st_uid,root.stat().st_gid)
def own(p):os.chown(p,*owner);os.chmod(p,0o700 if p.is_dir() else 0o600)
out.mkdir(parents=True,exist_ok=True);own(out.parent);own(out)
def save(p,data):p.write_text(json.dumps(data,indent=2)+'\n');own(p)
def nvlog(path):
 try:subprocess.run(['nvidia-debugdump','--nvlogonly','--ioctl','--file',str(path)],check=True)
 finally:
  if path.exists():own(path)
 with zipfile.ZipFile(path) as z:return z.read('nvlog.log')
evidence=out/'heap-evidence.json'
if not evidence.exists():save(evidence,{'boot_id':boot,'srcversion':src,'map':extract(nvlog(out/'startup-nvlog.zip'))})
e=json.loads(evidence.read_text());assert e['boot_id']==boot and e['srcversion']==src and e['map']['fixed_probe_target']=='0x3ef024000'
if a.mode!='inspect':
 prev=json.loads((out/'inspect.json').read_text());assert prev['status']=='success' and prev['offsets']==offsets and prev['boot_id']==boot and prev['srcversion']==src
record=out/(a.mode+'.json');assert not record.exists(),'This operation was already attempted; inspect its evidence, do not retry'
result={'boot_id':boot,'srcversion':src,'offsets':offsets,'mode':a.mode,'status':'started','time':datetime.datetime.now().astimezone().isoformat(),'scope':'Three MAX/UPPER fields only; actual power unlock not established'}
# Exclusive journal entry must exist before issuing even the first probe command.
with record.open('x') as f:json.dump(result,f,indent=2)
own(record)
env=dict(os.environ,LD_PRELOAD=str(root/'query_power.so'),CODEX_GSP_POWER_MODE=a.mode,**{'CODEX_GSP_'+k.upper():str(v) for k,v in offsets.items()})
with blocked_signals():
 try:
  with (out/(a.mode+'-power.txt')).open('w') as f,(out/(a.mode+'.log')).open('w') as err:
   proc=subprocess.run(['nvidia-smi','-q','-d','POWER'],env=env,stdout=f,stderr=err)
  log=(out/(a.mode+'.log')).read_text();print(log)
  rows=[json.loads(line[len('GSP_POWER '):]) for line in log.splitlines() if line.startswith('GSP_POWER {')]
  result['responses']=rows
  assert proc.returncode==0 and 'GSP_POWER SUCCESS mode='+a.mode in log.splitlines() and 'ERROR' not in log
  assert rows and all(r['result']==r['status']==r['rc']==r['poisoned']==0 for r in rows)
  assert rows[-1]['active']==0 and rows[-1]['max']==rows[-1]['fe']==rows[-1]['upper']==175000
  result['status']='success'
 except BaseException as error:
  result['status']='failed';result['error']=str(error);raise
 finally:
  save(record,result)
  try:
   with (out/(a.mode+'-kernel.log')).open('w') as f:subprocess.run(['journalctl','-b','-k','--no-pager'],stdout=f,check=True)
   nvlog(out/(a.mode+'-after-nvlog.zip'))
  finally:
   for f in out.iterdir():
    if f.is_file():own(f)
print(json.dumps(result,indent=2))
