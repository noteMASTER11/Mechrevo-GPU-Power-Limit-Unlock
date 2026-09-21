#!/usr/bin/env python3
"""Apply the v8 semantic GSP policy once in its dedicated boot entry."""
from pathlib import Path
import datetime,hashlib,json,os,subprocess,time

ROOT=Path(__file__).resolve().parent
TOKEN='codex.semantic_tgp=250'
MARKER='semantic-tgp-v8-20260922'
TARGET=250000
GREEN='\033[1;32m';RESET='\033[0m'

def line(message): print(message,flush=True)
def success(message): line(f'{GREEN}[SUCCESS]{RESET} {message}')
def active(name): return subprocess.run(['systemctl','is-active','--quiet',name]).returncode==0
def wait_driver(timeout=120):
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  if Path('/dev/nvidiactl').exists() and subprocess.run(['nvidia-smi','-L'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode==0:return
  time.sleep(1)
 raise RuntimeError('NVIDIA driver did not become ready')
def ac_online():
 base=Path('/sys/class/power_supply')
 return any((p/'type').exists() and (p/'type').read_text().strip() in ('Mains','USB','USB_PD') and
            (p/'online').exists() and (p/'online').read_text().strip()=='1' for p in base.iterdir())
def ucc_yields_gpu():
 result=subprocess.run(['busctl','--system','--json=short','call','com.uniwill.uccd','/com/uniwill/uccd',
                        'com.uniwill.uccd','GetGPUPowerStatusJSON'],capture_output=True,text=True,check=True)
 envelope=json.loads(result.stdout);status=json.loads(envelope['data'][0])
 return status.get('enabled') is True and status.get('requestedW') is None,status
def records(log):
 rows=[]
 for row in log.splitlines():
  if row.startswith('GSP_SEMANTIC {'): rows.append(json.loads(row[len('GSP_SEMANTIC '):]))
 return rows

def main():
 if os.geteuid()!=0:raise RuntimeError('root required')
 if TOKEN not in Path('/proc/cmdline').read_text().split():raise RuntimeError('dedicated boot token absent')
 if Path('/sys/module/nvidia/parameters/GspReadProbeBuild').read_text().strip()!=MARKER:raise RuntimeError('wrong NVIDIA module')
 if not ac_online():raise RuntimeError('AC power required')
 if active('nvidia-powerd.service'):raise RuntimeError('nvidia-powerd must yield Dynamic Boost ownership')
 line('[....] Waiting for NVIDIA GSP-RM')
 wait_driver();success('NVIDIA GSP-RM is ready')
 if not active('uccd.service'):raise RuntimeError('UCC daemon is not running')
 yielded,status=ucc_yields_gpu()
 if not yielded:raise RuntimeError('Enable UCC Max TGP before using this boot entry')
 success('UCC Max TGP is active; platform profiles and water-cooler control remain available')
 boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip();out=Path('/var/lib/mechrevo-semantic-tgp')/boot
 out.mkdir(parents=True,exist_ok=False,mode=0o700)
 record=out/'activation.json';state={'boot_id':boot,'time':datetime.datetime.now().astimezone().isoformat(),
  'target_mw':TARGET,'status':'started','ucc':status,'actual_draw_verified':False}
 record.write_text(json.dumps(state,indent=2)+'\n');os.chmod(record,0o600)
 line('[....] Resolving live GSP heap topology (no configured addresses or offsets)')
 env=dict(os.environ,LD_PRELOAD=str(ROOT/'query_power.so'),CODEX_GSP_POWER_MODE='activate',CODEX_GSP_TARGET_MW=str(TARGET))
 proc=subprocess.run(['nvidia-smi','-q','-d','POWER'],env=env,capture_output=True,text=True)
 (out/'activation.log').write_text(proc.stderr);(out/'nvidia-smi-power.txt').write_text(proc.stdout)
 rows=records(proc.stderr);state['resolver_records']=rows
 if proc.returncode or 'GSP_SEMANTIC SUCCESS mode=activate target=250000' not in proc.stderr.splitlines():
  raise RuntimeError('semantic transaction did not report success')
 if [r['op'] for r in rows]!=[0,1,2,6]:raise RuntimeError('unexpected transaction sequence')
 if any(r['rc'] or r['status'] or r['result'] or r['stage']!=100 or r['resolver'] or r['poisoned'] for r in rows):
  raise RuntimeError('resolver or transaction readback failed')
 if rows[0]['stock']>=TARGET or rows[2]['current']!=TARGET or rows[3]['current']!=TARGET or not rows[3]['active']:
  raise RuntimeError('250 W semantic readback mismatch')
 success(f'Semantic resolver found one coherent layout at runtime (VA base {rows[0]["va_base"]})')
 success('Three discovered ceiling members were updated with compare/readback verification')
 health=subprocess.run(['nvidia-smi','--query-gpu=name,power.limit,enforced.power.limit,power.max_limit,power.draw,temperature.gpu',
                        '--format=csv,noheader,nounits'],capture_output=True,text=True,check=True).stdout.strip()
 state['nvml_after']=health
 fields=[x.strip() for x in health.split(',')]
 if len(fields)<6 or abs(float(fields[1])-250)>0.5 or abs(float(fields[3])-250)>0.5:
  raise RuntimeError('NVML did not expose the verified 250 W limit')
 state['status']='verified_250w';record.write_text(json.dumps(state,indent=2)+'\n')
 success('250 W base TGP is active; Dynamic Boost source is disabled')
 line('[....] Holding the success screen for 5 seconds')
 time.sleep(5)

if __name__=='__main__':
 try:main()
 except BaseException as exc:
  line(f'\033[1;31m[FAILED]\033[0m {exc}')
  raise
