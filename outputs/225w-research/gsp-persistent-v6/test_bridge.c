/* Executes the real host probe against fake allocator/RPC boundaries.
 * Verifies no DMA on bad requests, cleanup, and retained lifetime after RPC failure.
 * It does not emulate GSP access permissions. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "nvtypes.h"
#include "nvstatus.h"
#include "ctrl/ctrl2080/ctrl2080internal.h"
#include "deprecated/gsp_read_probe.h"
#define ADDR_SYSMEM 1
#define NV_MEMORY_CACHED 0
#define NV_PROTECT_READ_WRITE 0
#define AT_GPU 0
#define LEVEL_ERROR 0
#define NV_PRINTF(...) ((void)0)
#define GSP_FW_WPR_META_MAGIC 0xdc3aae21371a60b3ULL
typedef struct { NvU64 magic,gspFwWprStart,gspFwWprEnd,gspFwOffset,sizeOfRadix3Elf,gspFwHeapOffset,gspFwHeapSize; } GspFwWprMeta;
typedef GspFwWprMeta GspFwWprMetaV1;
typedef struct { GspFwWprMeta *pWprMetaHopper; GspFwWprMetaV1 *pWprMetaV1; } KernelGsp;
typedef struct { unsigned char *data; } MEMORY_DESCRIPTOR;
typedef struct RM_API { NV_STATUS (*Control)(struct RM_API*,NvU32,NvU32,NvU32,void*,NvU32); } RM_API;
typedef struct {struct {NvU32 PCIDeviceID,PCISubDeviceID;} idInfo; KernelGsp *kgsp; RM_API *api; NvU32 hInternalClient,hInternalSubdevice; int virt,cc;} OBJGPU;
#define GPU_GET_KERNEL_GSP(g) ((g)->kgsp)
#define GPU_GET_PHYSICAL_RMAPI(g) ((g)->api)
#define IS_VIRTUAL(g) ((g)->virt)
#define gpuIsCCFeatureEnabled(g) ((g)->cc)
#define portMemSet memset
static void portMemCopy(void *d,size_t cap,const void *s,size_t n){assert(n<=cap);memcpy(d,s,n);}
static int failure,descs,pages,calls,unmaps;
static NvU64 expectedOffset;
typedef struct {NvU64 lo,hi;} NV_RANGE;
#define NV_RANGE_EMPTY ((NV_RANGE){1,0})
#define GPU_GET_KERNEL_BUS(g) (g)
static void kbusCarveoutWprs_HAL(OBJGPU *g,OBJGPU *bus,NV_RANGE *r){(void)g;(void)bus;r[1]=(NV_RANGE){0x3ef020000ULL,0x3f9c5ffffULL};if(failure==8)r[1]=NV_RANGE_EMPTY; if(failure==10)r[1].lo+=0x20000; if(failure==11)r[1].hi-=0x20000;}

static MEMORY_DESCRIPTOR *active;
static NV_STATUS memdescCreate(MEMORY_DESCRIPTOR **p,OBJGPU *g,NvU64 n,int align,int contig,int space,int cache,int flags){
 (void)g;(void)cache;(void)flags;assert(n==4096&&align==4096&&contig&&space==1);
 if(failure==1)return NV_ERR_NO_MEMORY;
 *p=calloc(1,sizeof(**p));assert(*p);active=*p;descs++;return NV_OK;
}
static NV_STATUS memdescAlloc(MEMORY_DESCRIPTOR *p){if(failure==2)return NV_ERR_NO_MEMORY;p->data=malloc(4096);assert(p->data);pages++;return NV_OK;}
static NV_STATUS memdescMapOld(MEMORY_DESCRIPTOR *p,int off,int n,int kernel,int prot,void **map,void **priv){(void)off;(void)n;(void)kernel;(void)prot;if(failure==3)return NV_ERR_INVALID_STATE;*map=p->data;*priv=p;return NV_OK;}
static NvU64 memdescGetPhysAddr(MEMORY_DESCRIPTOR *p,int at,int off){(void)p;(void)at;(void)off;return 0x900000;}
static void memdescUnmapOld(MEMORY_DESCRIPTOR *p,int kernel,void *map,void *priv){(void)kernel;assert(map==p->data&&priv==p);unmaps++;}
static void memdescFree(MEMORY_DESCRIPTOR *p){if(p->data){free(p->data);pages--;p->data=NULL;}}
static void memdescDestroy(MEMORY_DESCRIPTOR *p){free(p);descs--;active=NULL;}

static unsigned char heap[0x401000];
static int writeCalls,writeFail,failReadback;
static NV_STATUS rpc(RM_API *api,NvU32 c,NvU32 o,NvU32 cmd,void *buf,NvU32 n){
 (void)api;assert(c==7&&o==8);
 if(cmd==0x20800ad3){assert(n==4);unsigned v=*(unsigned*)buf;assert(v==0||v==145000);memcpy(heap+0x3843a0,&v,4);heap[0x38439a]=1;return NV_OK;}
 if(cmd==0x20800ad2){assert(n==1);unsigned v=*(unsigned char*)buf;heap[0x384399]=v;unsigned current=v?225000:145000;memcpy(heap+0x3bc808,&current,4);return NV_OK;}
 assert(cmd==0x20800afa&&n==96);calls++;
 NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS *p=buf;
 assert(p->memop==0&&p->src.cpuCacheAttrib==0&&p->dst.cpuCacheAttrib==0);
 if(p->src.aperture==2){
  assert(p->dst.aperture==1&&p->dst.baseAddr==0x900000&&p->transferSize==4096);
  NvU64 off=p->src.baseAddr-0x3ef024000ULL;assert(off+4096<=sizeof(heap));
  if(failure==4 || (failReadback&&writeCalls))return NV_ERR_TIMEOUT;
  memcpy(active->data,heap+off,4096);
 }else{
  assert(p->src.aperture==1&&p->src.baseAddr==0x900000&&p->src.size==4096&&p->dst.aperture==2&&p->transferSize==4);
  assert(p->dst.size==4096&&!(p->dst.baseAddr&4095)&&p->dst.offset<=4092);
  NvU64 off=p->dst.baseAddr+p->dst.offset-0x3ef024000ULL;
  assert(off==0x3bc724||off==0x3bc718||off==0x3843ac);
  ++writeCalls;if(writeFail==writeCalls)return NV_ERR_TIMEOUT;
  memcpy(heap+off,active->data,4);
 }
 return NV_OK;
}
#define portMemAllocNonPaged(n) calloc(1,n)
#define portMemFree free
#include "deprecated/gsp_read_probe.inc"
#include "deprecated/gsp_power_probe.h"
#include "deprecated/gsp_power_probe.inc"
static void scenario(int test){
 GspFwWprMeta m={GSP_FW_WPR_META_MAGIC,0,0,0,0x19be000,0,0x8a00000};
 KernelGsp k={&m,NULL};RM_API api={rpc};OBJGPU g={{0x2c1910de,0x60411d05},&k,&api,7,8,0,0};
 GSP_POWER_PROBE_PARAMS p={.version=1,.operation=1,.gpuOffset=0x196bb0,.pmgrOffset=0x380690,.boardOffset=0x3bc610};
 if(test==11){
  assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result);p.operation=2;
  assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result);
  heap[0x38439a]=1; /* Unknown initial offset-active bit: preflight rejects. */
  p.operation=4;assert(!gspPowerProbe(&g,&p,sizeof(p))&&p.result&&!p.poisoned);
  assert(!gspCtgpState.used&&!gspCtgpState.active);
  p.operation=5;assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result);
  p.operation=3;assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result&&!p.active);
  return;
 }
 if(test==10){
  assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result);p.operation=2;
  assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result);
  p.operation=4;assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result&&p.currentValue==225000);
  p.operation=3;assert(!gspPowerProbe(&g,&p,sizeof(p))&&p.result&&p.active);
  gspProbe_callsUsed=GSP_READ_PROBE_MAX_CALLS-256;
  p.operation=5;assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result&&p.currentValue==145000);
  p.operation=3;assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result&&!p.active);
  assert(!descs&&!pages&&writeCalls==7);return;
 }
 if(test==9)failReadback=1;
 if(test==1)writeFail=1;
 if(test==2)failure=4;
 if(test==3)g.virt=1;
 if(test==4)failure=10;
 if(test==5)failure=1;
 if(test==6)p.reserved[0]=1;

 if(test==7||test==8){
  assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result);
  gspProbe_callsUsed=GSP_READ_PROBE_MAX_CALLS-512;p.operation=2;
  assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result&&p.active);
  if(test==7){
   gspProbe_callsUsed=GSP_READ_PROBE_MAX_CALLS-128;int before=calls;
   GSP_READ_PROBE_PARAMS read={.version=4,.operation=1};
   assert(gspReadProbe(&g,&read,sizeof(read))==NV_ERR_INVALID_STATE&&calls==before);
   p.operation=3;assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.result&&!p.active);
  }else{
   gspProbe_callsUsed=GSP_READ_PROBE_MAX_CALLS;
   p.operation=3;assert(!gspPowerProbe(&g,&p,sizeof(p))&&p.result&&p.active&&p.armed&&p.writes==4);
  }return;
 }
 NV_STATUS st=gspPowerProbe(&g,&p,sizeof(p));
 if(test==0){assert(!st&&p.stage==100&&p.armed&&writeCalls==1&&!descs&&!pages);p.operation=2;assert(!gspPowerProbe(&g,&p,sizeof(p))&&p.active&&writeCalls==4);p.operation=3;assert(!gspPowerProbe(&g,&p,sizeof(p))&&!p.active&&writeCalls==7&&!descs&&!pages);return;}
 assert(st || p.result);
 if(test==1||test==2||test==9){
  assert(gspProbe_poisoned&&descs==1&&pages==1);int before=calls;
  GSP_READ_PROBE_PARAMS r={.version=4,.operation=1};assert(gspReadProbe(&g,&r,sizeof(r))==NV_ERR_INVALID_STATE&&calls==before);
  p.operation=3;gspPowerProbe(&g,&p,sizeof(p));assert(calls==before);
  memdescFree(active);memdescDestroy(active);
 } else assert(!descs&&!pages&&!writeCalls);
}
int main(int argc,char **argv){assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(heap,1,sizeof(heap),f)==sizeof(heap));fclose(f);
 for(int i=0;i<12;i++){pid_t p=fork();assert(p>=0);if(!p){scenario(i);_exit(0);}int status;assert(waitpid(p,&status,0)==p);assert(WIFEXITED(status)&&WEXITSTATUS(status)==0);}
 puts("PASS: kernel bridge success/rollback, read/write timeout shared poison+retention, virtualization/layout/allocation/ABI rejection, reserved restore budget, accurate failure state, readback timeout");
}
