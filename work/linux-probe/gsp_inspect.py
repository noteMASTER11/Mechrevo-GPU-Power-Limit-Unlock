#!/usr/bin/env python3
"""Offline examination of the installed GSP image; never opens GPU devices."""
from pathlib import Path
import argparse, struct
p=argparse.ArgumentParser();p.add_argument('operation',choices=['xref','asm','bytes']);p.add_argument('start',type=lambda x:int(x,0));p.add_argument('end',nargs='?',type=lambda x:int(x,0));a=p.parse_args()
root=Path(__file__).parent;b=(root/'gsp-rm.elf').read_bytes()
segments=[]
for n in range(struct.unpack_from('<H',b,56)[0]):
 t,f,o,v,pa,sz,ms,al=struct.unpack_from('<IIQQQQQQ',b,struct.unpack_from('<Q',b,32)[0]+56*n)
 if t==1:segments.append((v,o,sz))
def offset(v):
 for base,o,sz in segments:
  if base<=v<base+sz:return o+v-base
 raise ValueError(hex(v))
def sign(v,bits):return v-(1<<bits) if v&(1<<(bits-1)) else v
if a.operation=='bytes':
 o=offset(a.start);e=a.end or a.start+64
 for v in range(a.start,e,16):print(hex(v), b[offset(v):offset(v)+min(16,e-v)].hex(' '))
elif a.operation=='asm':
 for line in (root/'gsp-code.asm').open():
  try:v=int(line.split(':',1)[0].strip(),16)
  except ValueError:continue
  if a.start<=v<(a.end or a.start+256):print(line,end='')
  elif v>=(a.end or a.start+256):break
else:
 for base,o,sz in segments:
  if base!=0x1000000:continue
  for i in range(o,o+sz-8,2):
   w,z=struct.unpack_from('<II',b,i)
   if w&127!=0x17 or z&0x707f not in (0x13,0x67) or (w>>7)&31!=(z>>15)&31:continue
   dest=base+i-o+sign(w&0xfffff000,32)+sign(z>>20,12)
   if dest==a.start:print(hex(base+i-o),'call' if z&127==0x67 else 'address')
