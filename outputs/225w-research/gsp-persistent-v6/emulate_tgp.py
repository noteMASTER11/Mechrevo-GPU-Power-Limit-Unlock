"""Offline execution of original cTGP handlers on captured memory, no device access."""
from pathlib import Path
import struct,json
from unicorn import *
from unicorn.riscv_const import *
r=Path(__file__).resolve().parents[3];p=r/'outputs/225w-research/gsp-read-probe-v4/live/20afa6a8-ed42-4d5e-ac15-7a54ca42a9eb'
b=(p/'read-00000000-0001.bin').read_bytes()+(p/'read-00001000-0400.bin').read_bytes();e=(r/'work/linux-probe/gsp-rm.elf').read_bytes()
u=Uc(UC_ARCH_RISCV,UC_MODE_RISCV64)
for i in range(struct.unpack_from('<H',e,56)[0]):
 t,f,o,v,pa,sz,ms,al=struct.unpack_from('<IIQQQQQQ',e,struct.unpack_from('<Q',e,32)[0]+56*i)
 if t==1:u.mem_map(v&~4095,(ms+(v&4095)+4095)&~4095);u.mem_write(v,e[o:o+sz])
BASE=0x7f2000000;GPU=BASE+0x196bb0;PMGR=BASE+0x380690;BOARD=BASE+0x3bc610
u.mem_map(BASE,len(b));u.mem_write(BASE,b)
u.mem_map(0x6000000,0x10000);u.mem_map(0x7000000,0x1000);u.mem_map(0x8000000,0x6000)
def put(addr,val,n=4):u.mem_write(addr,val.to_bytes(n,'little'))
def get(addr,n=4):return int.from_bytes(u.mem_read(addr,n),'little')
# Synthetic legacy wrappers contain only the two links dereferenced by handlers.
put(0x8000000+0x160,0x8001000,8);put(0x8001000+0xe8,GPU,8)
for a in [BOARD+0x108,BOARD+0x114,PMGR+0x3d1c]:put(a,225000)
trace=[];writes=[];prepared=[]
def code(uc,addr,size,data):
 trace.append(addr)
 if addr==0x17c53a8:
  at=uc.reg_read(UC_RISCV_REG_A2);count=uc.reg_read(UC_RISCV_REG_A3)
  prepared.extend({'source':get(at+i*8,1),'selector':get(at+i*8+1,1),'value':get(at+i*8+4)} for i in range(count))
  uc.emu_stop()
u.hook_add(UC_HOOK_CODE,code)
def write(uc,access,addr,size,val,data):
 if BASE<=addr<BASE+len(b):writes.append({'pc':hex(uc.reg_read(UC_RISCV_REG_PC)),'address':hex(addr),'size':size,'value':val})
u.hook_add(UC_HOOK_MEM_WRITE,write)
def call(pc,value,n):
 put(0x8002000,value,n)
 for reg,x in [(UC_RISCV_REG_A0,0x8000000),(UC_RISCV_REG_A1,0x8002000),(UC_RISCV_REG_SP,0x600f000),(UC_RISCV_REG_RA,0x7000000)]:u.reg_write(reg,x)
 try:u.emu_start(pc,0x7000000,count=1000000)
 except UcError as ex:return {'error':str(ex),'pc':hex(u.reg_read(UC_RISCV_REG_PC)),'tail':[hex(x) for x in trace[-20:]]}
 return {'stopped_before_policy_submission':u.reg_read(UC_RISCV_REG_PC)==0x17c53a8,'returned':u.reg_read(UC_RISCV_REG_PC)==0x7000000,'status':u.reg_read(UC_RISCV_REG_A0) if u.reg_read(UC_RISCV_REG_PC)==0x7000000 else None}
rows=[]
for name,pc,value,n in [('offset',0x1768618,145000,4),('enable',0x1771864,1,1)]:
 result=call(pc,value,n);rows.append({'name':name,**result,'current_mw':get(BOARD+0x1f8),'pmgr':u.mem_read(PMGR+0x3d08,24).hex(),'writes':writes.copy()});writes.clear()
 if result.get('error'):break
assert rows[0]['returned'] and rows[0]['status']==0
assert rows[1]['stopped_before_policy_submission']
assert {'selector':3,'source':247,'value':225000} in prepared
print(json.dumps({'scope':__doc__,'results':rows,'prepared_policy_sources':prepared,'pmu_execution_verified':False},indent=2))
