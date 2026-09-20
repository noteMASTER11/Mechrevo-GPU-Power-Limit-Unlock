#include <assert.h>
#include <stdio.h>
#include "nvtypes.h"
#include "ctrl/ctrl2080/ctrl2080internal.h"
#include "deprecated/gsp_read_probe.h"
int main(void){
 NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS p;
 NvU64 offsets[]={0,4096,GSP_READ_PROBE_HEAP_SIZE-4096};
 for(unsigned i=0;i<3;i++){
  assert(gspReadProbeBuild(&p,0x2c1910de,0x60411d05,GSP_READ_PROBE_WPR_START,GSP_READ_PROBE_WPR_END,offsets[i],0x900000));
  assert(p.src.baseAddr==GSP_READ_PROBE_SOURCE+offsets[i]&&p.src.size==4096&&p.src.offset==0&&p.src.aperture==2&&p.src.cpuCacheAttrib==0);
  assert(p.dst.baseAddr==0x900000&&p.dst.size==4096&&p.dst.offset==0&&p.dst.aperture==1&&p.dst.cpuCacheAttrib==0&&p.transferSize==4096&&p.memop==0);
 }
 NvU64 bad[]={1,4095,GSP_READ_PROBE_HEAP_SIZE,GSP_READ_PROBE_HEAP_SIZE-4095,~(NvU64)0,~(NvU64)4095};
 for(unsigned i=0;i<6;i++)assert(!gspReadProbeBuild(&p,0x2c1910de,0x60411d05,GSP_READ_PROBE_WPR_START,GSP_READ_PROBE_WPR_END,bad[i],0x900000));
 assert(!gspReadProbeBuild(&p,0,0x60411d05,GSP_READ_PROBE_WPR_START,GSP_READ_PROBE_WPR_END,0,0x900000));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0,GSP_READ_PROBE_WPR_START,GSP_READ_PROBE_WPR_END,0,0x900000));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0x60411d05,0,GSP_READ_PROBE_WPR_END,0,0x900000));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0x60411d05,GSP_READ_PROBE_WPR_START,0,0,0x900000));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0x60411d05,GSP_READ_PROBE_WPR_START,GSP_READ_PROBE_WPR_END,0,0));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0x60411d05,GSP_READ_PROBE_WPR_START,GSP_READ_PROBE_WPR_END,0,0x900001));
 assert(sizeof(p)==96&&sizeof(GSP_READ_PROBE_PARAMS)==4200);
 puts("PASS: first/last page, bounds, alignment, wraparound, identity, direction and ABI");
}
