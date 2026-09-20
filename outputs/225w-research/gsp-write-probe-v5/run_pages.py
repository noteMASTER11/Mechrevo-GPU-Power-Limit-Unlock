"""Explicit root-only v4 launcher: metadata, then one page, then bounded ranges."""
from pathlib import Path
import argparse,datetime,hashlib,json,os,subprocess,zipfile
from heap_evidence import extract
from scan_policy import scan
if not __debug__:
 raise RuntimeError('Run without Python optimization; validation must remain enabled')
root=Path(__file__).resolve().parent
owner=(root.stat().st_uid,root.stat().st_gid)
def own(path):
 os.chown(path,*owner);os.chmod(path,0o700 if path.is_dir() else 0o600)
p=argparse.ArgumentParser();p.add_argument('mode',choices=['metadata','read']);p.add_argument('--offset',type=lambda x:int(x,0),default=0);p.add_argument('--pages',type=int,default=1);a=p.parse_args()
assert os.geteuid()==0
assert Path('/sys/module/nvidia/parameters/GspReadProbeBuild').read_text().strip()=='power-words-v5-20260920'
installed=json.loads((root/'installed.json').read_text())
assert Path('/sys/module/nvidia/srcversion').read_text().strip()==installed['module_srcversion']
assert hashlib.sha256(Path('/usr/lib/firmware/nvidia/615.71.09/gsp_ga10x.bin').read_bytes()).hexdigest()=='c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b'
assert a.offset>=0 and a.offset%4096==0 and 1<=a.pages<=8192 and a.offset+a.pages*4096<=0x73dc000
boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip();out=root/'live'/boot;out.mkdir(parents=True,exist_ok=True);own(out.parent);own(out)
def save(path,data):
 path.write_text(json.dumps(data,indent=2)+'\n');own(path)
def nvlog(path):
 try:
  subprocess.run(['nvidia-debugdump','--nvlogonly','--ioctl','--file',str(path)],check=True)
 finally:
  if path.exists():own(path)
 with zipfile.ZipFile(path) as z:return z.read('nvlog.log')
metadata=out/'metadata.json'
if a.mode=='metadata':
 assert a.offset==0 and not metadata.exists()
 evidence=extract(nvlog(out/'startup-nvlog.zip'))
 save(out/'startup-heap-evidence.json',evidence)
 tag='metadata';count=0
else:
 old=json.loads(metadata.read_text());assert old['boot_id']==boot and old['status']=='success' and old['module_srcversion']==installed['module_srcversion']
 first=out/'read-00000000-0001.json'
 if not first.exists():assert a.offset==0 and a.pages==1,'First hardware read must be one page only'
 else:assert json.loads(first.read_text())['status']=='success','First page failed; stop'
 # Reuse exact startup map only within the same boot/module; do not depend on
 # old startup records remaining in a circular log after thousands of reads.
 extract_data=json.loads((out/'startup-heap-evidence.json').read_text())
 assert extract_data['fixed_probe_target']=='0x3ef024000'
 tag=f'read-{a.offset:08x}-{a.pages:04x}';count=a.pages
record=out/(tag+'.json');assert not record.exists()
result={'boot_id':boot,'time':datetime.datetime.now().astimezone().isoformat(),'module_srcversion':installed['module_srcversion'],'mode':a.mode,'offset':a.offset,'pages':count,'status':'started','gpu_memory_writes':0,'power_policy_writes':0}
save(record,result)
dump=out/(tag+'.bin');logfile=out/(tag+'.log')
env=dict(os.environ,LD_PRELOAD=str(root/'query_pages.so'),CODEX_GSP_OFFSET=str(a.offset),CODEX_GSP_COUNT=str(count),CODEX_GSP_OUTPUT=str(dump))
try:
 with (out/(tag+'-power.txt')).open('w') as f,logfile.open('w') as e:
  run=subprocess.run(['nvidia-smi','-q','-d','POWER'],env=env,stdout=f,stderr=e)
 log=logfile.read_text();print(log)
 expected=f'GSP_PAGES SUCCESS offset={a.offset:x} pages={count} bytes={count*4096}'
 assert run.returncode==0 and expected in log.splitlines() and 'ERROR' not in log
 if count:
  data=dump.read_bytes();assert len(data)==count*4096
  result['sha256']=hashlib.sha256(data).hexdigest();result['candidates']=scan(data,a.offset)
 else:assert 'wpr=3ef020000..3f9c60000' in log
 result['status']='success'
except BaseException as error:
 result['status']='failed';result['error']=str(error)
 raise
finally:
 if dump.exists():own(dump)
 save(record,result)
 try:
  if count:
   with (out/(tag+'-kernel.log')).open('w') as f:subprocess.run(['journalctl','-b','-k','--no-pager'],stdout=f,check=True)
   nvlog(out/(tag+'-after-nvlog.zip'))
 finally:
  for artifact in out.iterdir():
   if artifact.is_file():own(artifact)
print(json.dumps(result,indent=2))
