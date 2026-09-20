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
typedef struct {NvU64 lo,hi;} NV_RANGE;
#define NV_RANGE_EMPTY ((NV_RANGE){1,0})
#define GPU_GET_KERNEL_BUS(g) (g)
static void kbusCarveoutWprs_HAL(OBJGPU *g,OBJGPU *bus,NV_RANGE *r){(void)g;(void)bus;r[1]=(NV_RANGE){0x100000,0x3fffff};if(failure==8)r[1]=NV_RANGE_EMPTY;}

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
static NV_STATUS rpc(RM_API *api,NvU32 c,NvU32 o,NvU32 cmd,void *buf,NvU32 n){
 (void)api;assert(c==7&&o==8&&cmd==0x20800afa&&n==96);calls++;
 NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS *p=buf;
 assert(p->src.baseAddr==0x100000&&p->src.aperture==2&&p->dst.baseAddr==0x900000&&p->dst.aperture==1&&p->transferSize==256&&p->memop==0);
 if(failure==4)return NV_ERR_TIMEOUT;
 memset(active->data,0x5a,256);return NV_OK;
}
#include "deprecated/gsp_read_probe.inc"
static void scenario(int test){
 GspFwWprMeta m={GSP_FW_WPR_META_MAGIC,0,0,0,0x10000,0,0x100000};
 KernelGsp k={&m,NULL};RM_API api={rpc};OBJGPU g={{0x2c1910de,0x60411d05},&k,&api,7,8,0,0};
 GSP_READ_PROBE_PARAMS p={.version=2,.operation=1};NV_STATUS s;
 if(test==5){p.operation=0;s=gspReadProbe(&g,&p,sizeof(p));assert(!s&&p.stage==1&&!calls&&!descs);return;}
 if(test==6){p.reserved[0]=1;s=gspReadProbe(&g,&p,sizeof(p));assert(s==NV_ERR_INVALID_ARGUMENT&&!calls&&!descs);return;}
 if(test==7){g.idInfo.PCIDeviceID=0x2c1810de;s=gspReadProbe(&g,&p,sizeof(p));assert(s==NV_ERR_NOT_SUPPORTED&&!calls&&!descs);return;}
 if(test==8){failure=8;s=gspReadProbe(&g,&p,sizeof(p));assert(s==NV_ERR_INVALID_DATA&&!calls&&!descs);return;}
 if(test==9){s=gspReadProbe(&g,&p,0);assert(s==NV_ERR_INVALID_ARGUMENT&&!calls&&!descs);return;}
 failure=test;s=gspReadProbe(&g,&p,sizeof(p));
 if(test>=1&&test<=3){assert(s!=NV_OK&&!descs&&!pages&&!calls);return;}
 if(test==4){assert(!s&&p.rpcStatus==NV_ERR_TIMEOUT&&p.stage==2&&p.bytesRead==0&&p.retainedBuffer==1&&descs==1&&pages==1&&!unmaps);}
 else {assert(!s&&p.stage==3&&p.bytesRead==256&&!descs&&!pages&&unmaps==1);for(int i=0;i<256;i++)assert(p.data[i]==0x5a);}
 assert(calls==1);p=(GSP_READ_PROBE_PARAMS){.version=2,.operation=1};s=gspReadProbe(&g,&p,sizeof(p));assert(!s&&calls==1);
 if(test==4){memdescFree(active);memdescDestroy(active);}
}
int main(void){for(int i=0;i<10;i++){pid_t p=fork();assert(p>=0);if(!p){scenario(i);_exit(0);}int status;assert(waitpid(p,&status,0)==p);assert(WIFEXITED(status)&&WEXITSTATUS(status)==0);}puts("PASS: 10 host lifecycle/guard scenarios; hardware permissions not tested");}
