"""Verify the captured GPU -> PMGR -> policy chain offline. No hardware calls."""
from pathlib import Path
import json,struct,hashlib
from scan_policy import scan
if not __debug__:raise RuntimeError('Validation requires normal Python mode')
root=Path(__file__).resolve().parent;p=root/'live/20afa6a8-ed42-4d5e-ac15-7a54ca42a9eb'
b=(p/'read-00000000-0001.bin').read_bytes()+(p/'read-00001000-0400.bin').read_bytes()
VA=0x7f2000000;PA=0x3ef024000;gpu=0x196bb0;pm=0x380690;board=0x3bc610
q=lambda off:struct.unpack_from('<Q',b,off)[0]
w=lambda off:struct.unpack_from('<I',b,off)[0]
assert q(gpu+0xa0)==VA+gpu and q(gpu+0x2210)==VA+pm
assert q(pm+0x1b8)==VA+pm and q(pm+0x60)==VA+gpu
assert q(pm+0x1cc0)==VA+0x3823a8 and q(pm+0x1cc8)==VA+0x382ba0
assert q(0x3823a8+2*8)==VA+board
assert b[board]==0 and struct.unpack_from('<H',b,board+2)[0]==2
assert q(board+8)==0x4193460 and q(board+0x2d0)==0x17b2248
assert w(board+0x108)==w(board+0x114)==w(pm+0x3d1c)==175000
assert b[board+0x110]==0xfe and b[board+0x105]==1
assert w(pm+0x3d14)==2 and w(pm+0x3d18)==80000
info=(p/'public-a618.bin').read_bytes();mask=struct.unpack_from('<I',info,4)[0]
assert w(0x382ba0+8)==mask
policies=[]
for i in range(32):
 if not mask&(1<<i):continue
 obj=q(0x3823a8+i*8)-VA;at=0xcc+i*0x104
 assert 0<=obj<len(b)-0x340
 assert struct.unpack_from('<H',b,obj+2)[0]==i
 assert (b[obj],b[obj+0x28],b[obj+0x2a])==tuple(info[at+4:at+7])
 assert w(obj+0x108)==struct.unpack_from('<I',info,at+16)[0]
 policies.append({'index':i,'heap_offset':hex(obj),'type':b[obj],'id':b[obj+0x28],'unit_raw':b[obj+0x2a],'max':w(obj+0x108)})
for fname,base,fields in [
 ('read-00196000-0003.bin',0x196000,[(gpu+0xa0,8),(gpu+0x2210,8)]),
 ('read-00380000-0005.bin',0x380000,[(pm+0x60,8),(pm+0x1b8,8),(pm+0x1cc0,16),(pm+0x3d14,12),(0x3823a8,0xa0)]),
 ('read-003bc000-0001.bin',0x3bc000,[(board,0x30),(board+0x104,0x18),(board+0x2d0,0x18)])]:
 reread=(p/fname).read_bytes()
 for off,n in fields:assert reread[off-base:off-base+n]==b[off:off+n],(fname,hex(off))
emu=json.loads((root/'info-emulation.json').read_text());assert emu['full_response_byte_differences']==0
kernel=(p/'read-00196000-0003-kernel.log').read_text();assert 'NVRM: Xid' not in kernel
report={'status':'live_gpu_pmgr_policy_chain_verified','boot_id':p.name,'gpu_memory_writes':0,'power_policy_writes':0,'maximum_power_w':175,
 'heap_mapping':{'virtual_base':hex(VA),'physical_base':hex(PA),'scope':'Confirmed by object self-pointers, bidirectional GPU/PMGR references and all 15 policy array entries; not a universal boot-independent mapping'},
 'objects':{name:{'heap_offset':hex(off),'va':hex(VA+off),'pa':hex(PA+off)} for name,off in [('gpu',gpu),('pmgr',pm),('board_policy_2',board)]},
 'fields':{name:{'pa':hex(PA+off),'value':w(off)} for name,off in [('board_max_effective',board+0x108),('board_max_FE',board+0x114),('pmgr_upper',pm+0x3d1c)]},
 'policy_count':len(policies),'policies':policies,'reread_verified':True,'original_info_emulation':{'instructions':emu['instructions'],'bytes_compared':len(info),'byte_differences':0},
 'no_kernel_xid':True,'heap_dump_sha256':hashlib.sha256(b).hexdigest(),'candidates':scan(b,0),
 'remaining':'No live write transport tested. Need bounded original-value-checked update/rollback, PMU synchronization and controlled real-power verification. Raising reported MAX alone is not a successful power unlock.'}
(root/'live-result.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['status','heap_mapping','objects','fields','policy_count','original_info_emulation','remaining']},indent=2))
