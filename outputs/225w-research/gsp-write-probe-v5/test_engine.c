/* Real engine on captured bytes. Catches wrong destinations, missing guards,
 * unexpected extra writes, repeat-apply and continuation after uncertain writes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvtypes.h"
#include "nvstatus.h"
#include "deprecated/gsp_power_probe.h"
static unsigned char heap[0x401000],original[0x401000];
static int writes,reads,failWrite,corruptWrite;
static NV_STATUS rd(void *c,NvU64 o,void *b,NvU32 n){(void)c;assert(o+n<=sizeof(heap));memcpy(b,heap+o,n);reads++;return NV_OK;}
static NV_STATUS wr(void *c,NvU64 o,NvU32 v){(void)c;assert(o==0x3bc724||o==0x3bc718||o==0x3843ac);assert(v==175000||v==225000);writes++;if(writes==failWrite)return NV_ERR_TIMEOUT;memcpy(heap+o,&v,4);if(writes==corruptWrite)heap[o]^=1;return NV_OK;}
static GSP_POWER_PROBE_PARAMS req(unsigned op){return (GSP_POWER_PROBE_PARAMS){.version=1,.operation=op,.gpuOffset=0x196bb0,.pmgrOffset=0x380690,.boardOffset=0x3bc610};}
static unsigned word(unsigned o){unsigned v;memcpy(&v,heap+o,4);return v;}
int main(int argc,char **argv){assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(original,1,sizeof(original),f)==sizeof(original));fclose(f);
 GSP_POWER_IO io={NULL,rd,wr};GSP_POWER_STATE s={0};GSP_POWER_PROBE_PARAMS p;NV_STATUS rc;
 memcpy(heap,original,sizeof(heap));p=req(0);rc=gspPowerEngine(&s,&io,&p);assert(rc==NV_OK);assert(!writes&&p.maxValue==175000&&p.currentValue==145000);
 p=req(2);assert(gspPowerEngine(&s,&io,&p)!=NV_OK&&!writes);
 p=req(1);assert(gspPowerEngine(&s,&io,&p)==NV_OK&&s.armed&&writes==1&&!memcmp(heap,original,sizeof(heap)));
 p=req(2);assert(gspPowerEngine(&s,&io,&p)==NV_OK&&s.active&&writes==4);
 assert(word(0x3bc724)==225000&&word(0x3bc718)==225000&&word(0x3843ac)==225000);
 p=req(2);assert(gspPowerEngine(&s,&io,&p)!=NV_OK&&writes==4);
 p=req(3);assert(gspPowerEngine(&s,&io,&p)==NV_OK&&!s.active&&writes==7&&!memcmp(heap,original,sizeof(heap)));
 p=req(2);assert(gspPowerEngine(&s,&io,&p)!=NV_OK&&writes==7);
 /* Every ownership/signature/baseline corruption must prevent identity writes. */
 unsigned bad[]={0x196c50,0x198dc0,0x380848,0x3806f0,0x382350,0x382358,0x3823b8,0x3bc610,0x3bc612,0x3bc618,0x3bc638,0x3bc63a,0x3bc8e0,0x3bc8e8,0x3bc8f0,0x3bc715,0x3bc718,0x3bc71c,0x3bc720,0x3bc724,0x3843a4,0x3843ac};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){memcpy(heap,original,sizeof(heap));heap[bad[i]]^=1;s=(GSP_POWER_STATE){0};writes=reads=0;p=req(1);assert(gspPowerEngine(&s,&io,&p)!=NV_OK&&!writes);}
 /* Bounds/overlap/ABI cannot trigger any hardware access. */
 for(unsigned i=0;i<6;i++){memcpy(heap,original,sizeof(heap));s=(GSP_POWER_STATE){0};writes=reads=0;p=req(1);if(i==0)p.gpuOffset=~0ULL;if(i==1)p.pmgrOffset=0x73dbfff;if(i==2)p.boardOffset=p.pmgrOffset;if(i==3)p.version=0;if(i==4)p.reserved[1]=1;if(i==5)p.operation=9;assert(gspPowerEngine(&s,&io,&p)!=NV_OK&&!writes&&!reads);}
 /* A write failure or mismatched readback poisons; no automatic second write. */
 for(unsigned fault=1;fault<=7;fault++){for(unsigned corrupt=0;corrupt<2;corrupt++){
  memcpy(heap,original,sizeof(heap));s=(GSP_POWER_STATE){0};writes=reads=0;failWrite=corrupt?0:fault;corruptWrite=corrupt?fault:0;
  p=req(1);rc=gspPowerEngine(&s,&io,&p);if(fault>1){assert(!rc);p=req(2);rc=gspPowerEngine(&s,&io,&p);}if(fault>4){assert(!rc);p=req(3);rc=gspPowerEngine(&s,&io,&p);}assert(rc!=NV_OK&&s.poisoned&&writes==(int)fault);
  p=req(3);assert(gspPowerEngine(&s,&io,&p)!=NV_OK&&writes==(int)fault);
 }}
 failWrite=corruptWrite=0;
 /* Restoration must refuse if a consumer raised CURRENT above the old ceiling. */
 memcpy(heap,original,sizeof(heap));s=(GSP_POWER_STATE){0};writes=0;p=req(1);assert(!gspPowerEngine(&s,&io,&p));p=req(2);assert(!gspPowerEngine(&s,&io,&p));unsigned current=200000;memcpy(heap+0x3bc808,&current,4);p=req(3);assert(gspPowerEngine(&s,&io,&p)!=NV_OK&&writes==4&&s.active);
 puts("PASS: identity/apply/restore exact bytes, 22 object corruptions, 6 ABI/bounds guards, 14 write faults, CURRENT restore guard");
}
