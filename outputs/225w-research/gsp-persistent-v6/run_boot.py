#!/usr/bin/env python3
"""Boot-scoped one-attempt cTGP experiment. Never infer success from a ceiling."""
from pathlib import Path
import datetime,hashlib,json,os,subprocess,sys,zipfile
from heap_evidence import extract
from transaction_signals import blocked_signals
if not __debug__:raise RuntimeError('Optimized Python is unsupported')
ROOT=Path(__file__).resolve().parent
MARKER='power-ctgp-v6-20260920'
FW_SHA='c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b'
def boot_enabled(cmdline):return 'codex.max_tgp=225' in cmdline.split()
def ac_online(base=Path('/sys/class/power_supply')):
 return any((p/'type').read_text().strip() in ('Mains','USB','USB_PD') and (p/'online').exists() and (p/'online').read_text().strip()=='1' for p in base.iterdir() if (p/'type').exists())
def check_log(log,mode,returncode):
 rows=[json.loads(x[len('GSP_POWER '):]) for x in log.splitlines() if x.startswith('GSP_POWER {')]
 expected=[0,1,2,4] if mode=='activate' else [5,3]
 if returncode or 'GSP_POWER SUCCESS mode='+mode not in log.splitlines() or 'ERROR' in log:raise RuntimeError('Helper did not confirm transaction; inspect log, do not retry')
 if [r['op'] for r in rows]!=expected or any(any(r[k] for k in ['rc','status','result','poisoned']) or r['stage']!=100 for r in rows):raise RuntimeError('Invalid transaction readback')
 last=rows[-1];want=225000 if mode=='activate' else 175000
 if any(last[k]!=want for k in ['max','fe','upper']) or last['active']!=(mode=='activate'):raise RuntimeError('Ceilings/ownership readback mismatch')
 if mode=='activate' and last['current']!=225000:raise RuntimeError('Effective CURRENT is not 225 W')
 if mode=='release' and last['current']>175000:raise RuntimeError('CURRENT remains above stock ceiling')
 return rows

def ucc_owns_external(raw):
 obj=json.loads(raw)
 return isinstance(obj,dict) and obj.get('enabled') is True and obj.get('forced') is True

def main():
 mode=sys.argv[1] if len(sys.argv)==2 else 'activate'
 if mode not in ['activate','release']:raise RuntimeError('Use activate or release')
 if os.geteuid()!=0:raise RuntimeError('Root required')
 if not boot_enabled(Path('/proc/cmdline').read_text()):raise RuntimeError('Dedicated boot token absent')
 if Path('/sys/module/nvidia/parameters/GspReadProbeBuild').read_text().strip()!=MARKER:raise RuntimeError('Wrong loaded module')
 installed=json.loads((ROOT/'installed.json').read_text())
 src=Path('/sys/module/nvidia/srcversion').read_text().strip()
 if src!=installed['module_srcversion']:raise RuntimeError('Module identity mismatch')
 if hashlib.sha256(Path('/usr/lib/firmware/nvidia/615.71.09/gsp_ga10x.bin').read_bytes()).hexdigest()!=FW_SHA:raise RuntimeError('Firmware mismatch')
 if mode=='activate' and not ac_online():raise RuntimeError('AC power required')
 if mode=='activate':
  # Check the running daemon, not a file or the presence of a GUI checkbox.
  response=subprocess.run(['busctl','--system','--json=short','call','com.uniwill.uccd','/com/uniwill/uccd','com.uniwill.uccd','GetGPUPowerStatusJSON'],check=True,capture_output=True,text=True)
  payload=json.loads(response.stdout)
  if payload.get('type')!='s' or not ucc_owns_external(payload['data'][0]):raise RuntimeError('UCC did not confirm forced external GPU power ownership')
 boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip()
 out=Path('/var/lib/mechrevo-max-tgp')/boot;out.mkdir(parents=True,exist_ok=True,mode=0o700);os.chmod(out,0o700)
 record=out/(mode+'.json');result={'boot_id':boot,'srcversion':src,'mode':mode,'status':'started','time':datetime.datetime.now().astimezone().isoformat(),'actual_draw_verified':False}
 # Persist attempt BEFORE any GSP call. A restart never retries this boot.
 with record.open('x') as f:json.dump(result,f)
 os.chmod(record,0o600)
 with blocked_signals():
  try:
   if mode=='activate':
    nvlog=out/'startup-nvlog.zip'
    subprocess.run(['nvidia-debugdump','--nvlogonly','--ioctl','--file',str(nvlog)],check=True)
    os.chmod(nvlog,0o600)
    with zipfile.ZipFile(nvlog) as z:evidence=extract(z.read('nvlog.log'))
    if evidence['fixed_probe_target']!='0x3ef024000':raise RuntimeError('WPR heap layout mismatch')
    result['heap_evidence']=evidence
   env=dict(os.environ,LD_PRELOAD=str(ROOT/'query_power.so'),CODEX_GSP_POWER_MODE=mode,CODEX_GSP_GPU=str(0x196bb0),CODEX_GSP_PMGR=str(0x380690),CODEX_GSP_BOARD=str(0x3bc610))
   with (out/(mode+'.log')).open('w') as err,(out/(mode+'-power.txt')).open('w') as f:
    proc=subprocess.run(['nvidia-smi','-q','-d','POWER'],env=env,stdout=f,stderr=err)
   log=(out/(mode+'.log')).read_text();print(log,flush=True)
   result['responses']=check_log(log,mode,proc.returncode)
   result['status']='policy_225_verified' if mode=='activate' else 'released'
   result['nvml_after']=subprocess.run(['nvidia-smi','--query-gpu=name,power.limit,enforced.power.limit,power.max_limit,power.draw,temperature.gpu','--format=csv'],capture_output=True,text=True).stdout
  except BaseException as e:
   result['status']='failed';result['error']=str(e);raise
  finally:
   record.write_text(json.dumps(result,indent=2)+'\n')
   for file in out.iterdir():os.chmod(file,0o600)
   status=Path('/run/mechrevo-max-tgp');status.mkdir(exist_ok=True,mode=0o755);os.chmod(status,0o755)
   # Expose truthful status only, never raw heap/log/identifiers.
   public={k:result[k] for k in ['mode','status','actual_draw_verified']}
   if result.get('responses'):public['current_mw']=result['responses'][-1]['current']
   tmp=status/'status.json.tmp';tmp.write_text(json.dumps(public)+'\n');os.chmod(tmp,0o644);tmp.replace(status/'status.json')
 print(json.dumps(result,indent=2))
if __name__=='__main__':main()
