from pathlib import Path
import os,sys,subprocess,json,datetime
root=Path(__file__).resolve().parent
assert os.geteuid()==0
assert Path('/sys/module/nvidia/parameters/GspReadProbeBuild').read_text().strip()=='wpr-read-v2-20260920'
assert Path('/sys/module/nvidia/srcversion').read_text().strip()=='C8854C703EC237279993EED'
mode=sys.argv[1];assert mode in ('metadata','read')
out=root/'live-20260920'
if mode=='read':
 s=(out/'metadata-probe.log').read_text()
 assert 'status=0x0 operation=0 stage=1' in s
 assert 'wpr=3ef020000..3f9c60000' in s
 assert not (out/'read-probe.log').exists(), 'Read already attempted; inspect saved result'
env=dict(os.environ,LD_PRELOAD=str(root/'query_probe.so'))
env.pop('CODEX_GSP_READ_WPR',None)
if mode=='read':env['CODEX_GSP_READ_WPR']='1'
with (out/(mode+'-power.txt')).open('w') as f, (out/(mode+'-probe.log')).open('w') as e:
 p=subprocess.run(['nvidia-smi','-q','-d','POWER'],env=env,stdout=f,stderr=e)
print('process_exit',p.returncode)
print((out/(mode+'-probe.log')).read_text())
(out/(mode+'-run.json')).write_text(json.dumps({'time':datetime.datetime.now().astimezone().isoformat(),'boot_id':Path('/proc/sys/kernel/random/boot_id').read_text().strip(),'mode':mode,'process_exit':p.returncode},indent=2)+'\n')
