"""Offline evidence extraction; never accesses GPU devices."""
from pathlib import Path
import hashlib,json,struct
p=Path(__file__).resolve().parent
b=(p/'nvlog.bin').read_bytes()
log=(p/'read-probe.log').read_text()
assert 'operation=1 stage=2 rpc_status=0x1f bytes=0 retained=1' in log
records=[]
for name,marker,arg,pc in [
 ('fb_address_not_in_mapped_ranges',0x03030000003d2b48,0x3ef020000,0x1ba6462),
 ('surface_mapping_returned_null',0x03020000003ba9c8,None,0x1b04638),
 ('source_map_failed',0x03030000002cf4e0,0x1f,0x1365ae8),
]:
 key=struct.pack('<Q',marker);hits=[];at=0
 while True:
  at=b.find(key,at)
  if at<0:break
  before=struct.unpack_from('<Q',b,at-8)[0]
  if arg is None or before==arg:hits.append({'offset':hex(at),'preceding_word':hex(before)})
  at+=1
 assert hits,name
 records.append({'event':name,'firmware_pc':hex(pc),'nvlog_marker':hex(marker),'records':hits})
r={'result':'gsp_handler_reached_source_mapping_rejected','loaded_marker':'wpr-read-v2-20260920','loaded_srcversion':'C8854C703EC237279993EED','wpr_start':'0x3ef020000','wpr_end_exclusive':'0x3f9c60000','rpc_status':'0x1f NV_ERR_INVALID_ARGUMENT','bytes_read':0,'gpu_memory_writes':0,'power_policy_writes':0,'retained_sysmem_bytes':4096,'maximum_power_w':175,'nvlog_sha256':hashlib.sha256(b).hexdigest(),'evidence':records,'limits':'This proves reachability of the GSP transfer handler and rejection of this source region, not general inability to read other GSP regions. Duplicate records in separate NVLOG buffers are not interpreted as multiple probe requests.'}
(p/'result.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({'result':r['result'],'events':[a['event'] for a in records],'bytes_read':0},indent=2))
