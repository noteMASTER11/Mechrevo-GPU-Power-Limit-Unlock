"""Offline candidate finder for this exact GSP build. Never opens a device.
A candidate is not proof of a live object or permission to modify memory.
Derived types can override these slots and therefore escape this narrow scan.
"""
from pathlib import Path
import struct,json,argparse
BASE=0x3ef024000
SIGNATURE={0x2d0:0x177f3c4,0x2d8:0x177c0f4,0x2e0:0x177c250}
def scan(data,offset):
 hits=[];cursor=0;key=struct.pack('<Q',SIGNATURE[0x2e0])
 while True:
  pos=data.find(key,cursor)
  if pos<0:break
  cursor=pos+1;at=pos-0x2e0
  if at<0 or (at+offset)%8 or at+0x2e8>len(data):continue
  q=lambda k:struct.unpack_from('<Q',data,at+k)[0]
  if q(0x2d8)!=SIGNATURE[0x2d8]:continue
  common=q(0x2d0)==SIGNATURE[0x2d0]
  board=q(0x2d0)==0x17b2248 and q(8)==0x4193460 and data[at]==0
  if not (common or board):continue
  hits.append({'heap_offset':hex(at+offset),'physical_address':hex(BASE+at+offset),
   'signature_kind':'board_subclass' if board else 'common_policy',
   'index_raw_2':struct.unpack_from('<H',data,at+2)[0],'type_raw_0':data[at],'id_raw_28':data[at+0x28],'unit_raw_2a':data[at+0x2a],
   'table_raw_8':hex(struct.unpack_from('<Q',data,at+8)[0]),
   'max_effective_raw':struct.unpack_from('<I',data,at+0x108)[0],
   'current_effective_raw':struct.unpack_from('<I',data,at+0x1f8)[0],
   'evidence':'Method signatures match; owner/PMGR link requires independent verification'})
 return hits
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('dump',type=Path);p.add_argument('--offset',type=lambda x:int(x,0),required=True);a=p.parse_args()
 print(json.dumps({'candidates':scan(a.dump.read_bytes(),a.offset),'limitations':__doc__},indent=2))
