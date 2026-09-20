"""Offline original-code emulation over a captured heap. NO device access.
It verifies CPU-side writes of the current BoardSet path, not firmware/PMU
synchronization, electrical behavior, or permission to apply these writes live.
"""
from pathlib import Path
import struct,json,hashlib
from unicorn import Uc,UC_ARCH_RISCV,UC_MODE_RISCV64,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.riscv_const import *
root=Path(__file__).resolve().parents[3]
live=Path(__file__).parent/'live/20afa6a8-ed42-4d5e-ac15-7a54ca42a9eb'
elf=(root/'work/linux-probe/gsp-rm.elf').read_bytes()
heap=(live/'read-00000000-0001.bin').read_bytes()+(live/'read-00001000-0400.bin').read_bytes()
BASE=0x7f2000000;GPU=BASE+0x196bb0;PMGR=BASE+0x380690;BOARD=BASE+0x3bc610
assert struct.unpack_from('<Q',heap,0x196bb0+0xa0)[0]==GPU
assert struct.unpack_from('<Q',heap,0x196bb0+0x2210)[0]==PMGR
assert struct.unpack_from('<Q',heap,0x380690+0x1b8)[0]==PMGR
assert struct.unpack_from('<Q',heap,0x3823a8+16)[0]==BOARD
assert struct.unpack_from('<Q',heap,0x3bc610+0x2d0)[0]==0x17b2248
results=[]
for source,value in [(0xfe,175000),(0xfd,225000),(0xfe,225000)]:
 u=Uc(UC_ARCH_RISCV,UC_MODE_RISCV64)
 for i in range(struct.unpack_from('<H',elf,56)[0]):
  typ,flags,off,va,_,filesz,memsz,_=struct.unpack_from('<IIQQQQQQ',elf,struct.unpack_from('<Q',elf,32)[0]+i*56)
  if typ==1:
   u.mem_map(va&~4095,(memsz+(va&4095)+4095)&~4095);u.mem_write(va,elf[off:off+filesz])
 u.mem_map(BASE,len(heap));u.mem_write(BASE,heap)
 u.mem_map(0x6000000,0x10000);u.mem_map(0x7000000,0x1000)
 trace=[];writes=[]
 def code(uc,pc,size,user):
  trace.append(pc)
 def write(uc,access,addr,size,val,user):
  if not 0x6000000<=addr<0x6010000:
   writes.append({'pc':hex(uc.reg_read(UC_RISCV_REG_PC)),'address':hex(addr),'board_offset':hex(addr-BOARD),'size':size,'before':uc.mem_read(addr,size).hex(),'after':int(val & ((1<<(size*8))-1)).to_bytes(size,'little').hex()})
 u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_WRITE,write)
 def call(src,val):
  for reg,x in [(UC_RISCV_REG_A0,GPU),(UC_RISCV_REG_A1,PMGR),(UC_RISCV_REG_A2,BOARD),(UC_RISCV_REG_A3,2),(UC_RISCV_REG_A4,src),(UC_RISCV_REG_A5,val),(UC_RISCV_REG_SP,0x600f000),(UC_RISCV_REG_RA,0x7000000)]:u.reg_write(reg,x)
  u.emu_start(0x17b2248,0x7000000,count=100000)
  assert u.reg_read(UC_RISCV_REG_PC)==0x7000000
  assert u.reg_read(UC_RISCV_REG_A0)==0
 call(source,value)
 effective=struct.unpack('<I',u.mem_read(BOARD+0x108,4))[0]
 assert effective==(225000 if source==0xfe and value==225000 else 175000)
 row={'source':hex(source),'value':value,'effective_max':effective,'instructions':len(trace),'writes':writes.copy()}
 if source==0xfe and value==225000:
  writes.clear();call(0xfe,175000)
  assert struct.unpack('<I',u.mem_read(BOARD+0x108,4))[0]==175000
  row['restore_writes']=writes.copy()
  now=bytes(u.mem_read(BASE,len(heap)))
  delta=[i for i,(a,b) in enumerate(zip(heap,now)) if a!=b]
  assert delta==[0x3bc610+0x44],delta
  row['restore_remaining_difference']='policy dirty flag at +0x44, set by original setter'
 results.append(row)
print(json.dumps({'scope':__doc__,'elf_sha256':hashlib.sha256(elf).hexdigest(),'heap_sha256':hashlib.sha256(heap).hexdigest(),'results':results},indent=2))
