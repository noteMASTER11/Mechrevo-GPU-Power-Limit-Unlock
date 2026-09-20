#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "nvtypes.h"
#include "ctrl/ctrl2080/ctrl2080internal.h"
#include "deprecated/gsp_read_probe.h"
int main(void) {
 NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS p;
 /* Wrong PCI identity or a malformed physical interval must never produce a DMA request. */
 assert(gspReadProbeBuild(&p,0x2c1910de,0x60411d05,0x100000,0x400000,0x300000,0x10000,0x900000)==1);
 assert(p.src.baseAddr==0x300000 && p.src.size==64 && p.src.offset==0 && p.src.aperture==2);
 assert(p.dst.baseAddr==0x900000 && p.dst.size==4096 && p.dst.offset==0 && p.dst.aperture==1);
 assert(p.transferSize==64 && p.memop==0);
 assert(!gspReadProbeBuild(&p,0x2c1810de,0x60411d05,0x100000,0x400000,0x300000,0x10000,0x900000));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0x60401d05,0x100000,0x400000,0x300000,0x10000,0x900000));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0x60411d05,0x100000,0x400000,0x400000-32,64,0x900000));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0x60411d05,0x100000,0x400000,0xfffffffffffffff0ULL,64,0x900000));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0x60411d05,0x100000,0x400000,0x300000,63,0x900000));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0x60411d05,0x100000,0x400000,0x300000,64,0x900001));
 assert(!gspReadProbeBuild(&p,0x2c1910de,0x60411d05,0x100000,0x400000,0x300000,64,0));
 assert(sizeof(p)==96);
 puts("PASS: identity, range/overflow guards and read-only DMA direction");
}
