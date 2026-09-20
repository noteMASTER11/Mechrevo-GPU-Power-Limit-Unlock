"""Offline original GSP policy INFO backend on the captured heap; no device access."""
from pathlib import Path
import struct,json
from unicorn import Uc,UC_ARCH_RISCV,UC_MODE_RISCV64,UC_HOOK_CODE
from unicorn.riscv_const import *
r=Path(__file__).resolve().parents[3];p=Path(__file__).parent/'live/20afa6a8-ed42-4d5e-ac15-7a54ca42a9eb'
b=(p/'read-00000000-0001.bin').read_bytes()+(p/'read-00001000-0400.bin').read_bytes();e=(r/'work/linux-probe/gsp-rm.elf').read_bytes()
u=Uc(UC_ARCH_RISCV,UC_MODE_RISCV64)
for i in range(struct.unpack_from('<H',e,56)[0]):
 t,f,o,v,pa,sz,ms,al=struct.unpack_from('<IIQQQQQQ',e,struct.unpack_from('<Q',e,32)[0]+56*i)
 if t==1:u.mem_map(v&~4095,(ms+(v&4095)+4095)&~4095);u.mem_write(v,e[o:o+sz])
u.mem_map(0x7f2000000,len(b));u.mem_write(0x7f2000000,b)
u.mem_map(0x6000000,0x10000);u.mem_map(0x7000000,0x1000);u.mem_map(0x8000000,0x6000)
for reg,x in [(UC_RISCV_REG_A0,0x7f2196bb0),(UC_RISCV_REG_A1,0x7f2380690),(UC_RISCV_REG_A2,0x8000000),(UC_RISCV_REG_SP,0x600f000),(UC_RISCV_REG_RA,0x7000000)]:u.reg_write(reg,x)
trace=[]
u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,data:trace.append(addr))
u.emu_start(0x1787ef0,0x7000000,count=1000000)
assert u.reg_read(UC_RISCV_REG_PC)==0x7000000
assert u.reg_read(UC_RISCV_REG_A0)==0
info=bytes(u.mem_read(0x8000000,0x5020));actual=(p/'public-a618.bin').read_bytes()
rows=[]
for i in range(32):
 if not struct.unpack_from('<I',actual,4)[0]&(1<<i):continue
 at=0xcc+i*0x104
 # Public record type/id/unit and MIN/DEFAULT/MAX from original getters.
 assert info[at:at+20]==actual[at:at+20],i
 rows.append({'index':i,'type':info[at+4],'id':info[at+5],'unit_raw':info[at+6],
 'min':struct.unpack_from('<I',info,at+8)[0],'default':struct.unpack_from('<I',info,at+12)[0],'max':struct.unpack_from('<I',info,at+16)[0]})
print(json.dumps({'scope':__doc__,'status':0,'instructions':len(trace),'public_records_matched':rows,'full_response_byte_differences':sum(a!=b for a,b in zip(info,actual))},indent=2))
