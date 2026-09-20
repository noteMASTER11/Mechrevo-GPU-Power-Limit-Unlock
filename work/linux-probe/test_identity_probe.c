/* User-space fault-injection harness. Executes the actual probe helpers with
 * mocked DMA/registry services and the driver's actual SHA256 implementation.
 * Never accesses GPU devices. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvtypes.h"
#include "nvmisc.h"
typedef int NV_STATUS;
enum { NV_OK=0, NV_ERR_NOT_SUPPORTED=1, NV_ERR_NO_MEMORY=2, NV_ERR_INVALID_DATA=3, TEST_ERROR=4 };
#define RM_PAGE_SIZE 4096
#define ADDR_SYSMEM 0
#define NV_MEMORY_CACHED 0
#define MEMDESC_FLAGS_NONE 0
#define NV_FB_ALLOC_RM_INTERNAL_OWNER_UNNAMED_TAG_15 0
#define NV_PROTECT_READ_WRITE 0
#define AT_GPU 0
#define LEVEL_ERROR 0
#define LEVEL_WARNING 0
#define NV_PRINTF(...) ((void)0)
#define IS_VIRTUAL(g) ((g)->virt)
typedef struct { NvU64 size; unsigned char *data; } MEMORY_DESCRIPTOR;
typedef struct { struct {NvU64 pa,size;} vbiosOverrideArgs; } GSP_ARGUMENTS_CACHED;
typedef struct { MEMORY_DESCRIPTOR *pTaskLogDescriptor; NvU64 *pTaskLogBuffer; } RM_LIBOS_LOG_MEM;
typedef struct { MEMORY_DESCRIPTOR *pIdentityVbiosMD; GSP_ARGUMENTS_CACHED *pGspArgumentsCached; RM_LIBOS_LOG_MEM rmLibosLogMem[8]; } KernelGsp;
typedef struct { struct {NvU32 PCIDeviceID,PCISubDeviceID;} idInfo; int virt; } OBJGPU;
static NvU8 *rom;
static NvU32 rom_size, enable;
static int fail, mutate, md_count, data_count, heap_count, unmaps, registry_missing;
static NV_STATUS osReadRegistryDword(OBJGPU *g,const char *n,NvU32 *v){(void)g;(void)n;*v=enable;return registry_missing?TEST_ERROR:NV_OK;}
static NV_STATUS osReadRegistryBinary(OBJGPU *g,const char *n,NvU8 *p,NvU32 *sz){(void)g;(void)n;if(fail==2 || *sz<rom_size)return TEST_ERROR;memcpy(p,rom,rom_size);*sz=rom_size;if(mutate)p[100]^=1;return NV_OK;}
static void *portMemAllocNonPaged(size_t sz){if(fail==1)return NULL;void *p=malloc(sz);if(p)heap_count++;return p;}
static void portMemFree(void *p){if(p){heap_count--;free(p);}}
#define portMemCmp memcmp
#define portMemSet memset
static void portMemCopy(void *d,size_t cap,const void *s,size_t sz){assert(sz<=cap);memcpy(d,s,sz);if(fail==6)((NvU8*)d)[7]^=1;}
static NV_STATUS memdescCreate(MEMORY_DESCRIPTOR **d,OBJGPU *g,NvU64 sz,int align,int contig,int addr,int cache,int flags){(void)g;(void)align;(void)addr;(void)cache;(void)flags;assert(contig);if(fail==3)return TEST_ERROR;*d=calloc(1,sizeof(**d));assert(*d);(*d)->size=sz;md_count++;return NV_OK;}
static NV_STATUS alloc_md(MEMORY_DESCRIPTOR *d){if(fail==4)return TEST_ERROR;d->data=malloc(d->size);assert(d->data);data_count++;return NV_OK;}
#define memdescTagAlloc(status,tag,d) ((status)=alloc_md(d))
static NvU64 memdescGetSize(MEMORY_DESCRIPTOR *d){return d->size;}
static NV_STATUS memdescMap(MEMORY_DESCRIPTOR *d,int off,NvU64 sz,int sys,int prot,NvP64 *va,NvP64 *priv){(void)off;(void)sys;(void)prot;assert(sz==d->size);if(fail==5)return TEST_ERROR;*va=d->data;*priv=NULL;return NV_OK;}
static void memdescUnmap(MEMORY_DESCRIPTOR *d,int sys,void *va,NvP64 priv){(void)sys;(void)priv;assert(va==d->data);unmaps++;}
static void memdescFree(MEMORY_DESCRIPTOR *d){if(d->data){free(d->data);d->data=NULL;data_count--;}}
static void memdescDestroy(MEMORY_DESCRIPTOR *d){assert(!d->data);free(d);md_count--;}
static NvU64 memdescGetPhysAddr(MEMORY_DESCRIPTOR *d,int at,int off){(void)d;(void)at;(void)off;return 0x12345000;}
#include "kernel_gsp_identity_probe.h"
static void clean(KernelGsp *k){_kgspFreeIdentityVbios(k);_kgspFreeIdentityVbios(k);assert(!md_count&&!data_count&&!heap_count);}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);rom_size=KGSP_IDENTITY_ROM_SIZE;rom=malloc(rom_size);assert(fread(rom,1,rom_size,f)==rom_size);assert(fgetc(f)==EOF);fclose(f);
 OBJGPU g={{0x2c1910de,0x60411d05},0};GSP_ARGUMENTS_CACHED args={0};KernelGsp k={.pGspArgumentsCached=&args};
 enable=0;assert(_kgspPrepareIdentityVbios(&g,&k)==NV_OK);assert(!k.pIdentityVbiosMD);
 enable=1;registry_missing=1;assert(_kgspPrepareIdentityVbios(&g,&k)==NV_OK);assert(!k.pIdentityVbiosMD);registry_missing=0;
 enable=2;assert(_kgspPrepareIdentityVbios(&g,&k)==NV_ERR_NOT_SUPPORTED);enable=1;
 g.idInfo.PCIDeviceID++;assert(_kgspPrepareIdentityVbios(&g,&k)==NV_ERR_NOT_SUPPORTED);g.idInfo.PCIDeviceID--;
 g.idInfo.PCISubDeviceID++;assert(_kgspPrepareIdentityVbios(&g,&k)==NV_ERR_NOT_SUPPORTED);g.idInfo.PCISubDeviceID--;
 g.virt=1;assert(_kgspPrepareIdentityVbios(&g,&k)==NV_ERR_NOT_SUPPORTED);g.virt=0;
 for(fail=1;fail<=6;fail++){assert(_kgspPrepareIdentityVbios(&g,&k)!=NV_OK);clean(&k);}fail=0;
 rom_size--;assert(_kgspPrepareIdentityVbios(&g,&k)==NV_ERR_INVALID_DATA);clean(&k);rom_size++;
 mutate=1;assert(_kgspPrepareIdentityVbios(&g,&k)==NV_ERR_INVALID_DATA);clean(&k);mutate=0;
 assert(_kgspPrepareIdentityVbios(&g,&k)==NV_OK);assert(md_count==1&&data_count==1&&!heap_count);
 assert(!memcmp(k.pIdentityVbiosMD->data,rom,rom_size));for(NvU64 i=rom_size;i<k.pIdentityVbiosMD->size;i++)assert(k.pIdentityVbiosMD->data[i]==0);
 MEMORY_DESCRIPTOR *saved=k.pIdentityVbiosMD;assert(_kgspPrepareIdentityVbios(&g,&k)==NV_OK&&saved==k.pIdentityVbiosMD&&md_count==1);
 _kgspPopulateIdentityVbios(&k,NV_TRUE);assert(!args.vbiosOverrideArgs.pa&&!args.vbiosOverrideArgs.size);
 _kgspPopulateIdentityVbios(&k,NV_FALSE);assert(args.vbiosOverrideArgs.pa==0x12345000&&args.vbiosOverrideArgs.size==rom_size);
 clean(&k);memset(&args,0,sizeof(args));_kgspPopulateIdentityVbios(&k,NV_FALSE);assert(!args.vbiosOverrideArgs.pa&&!args.vbiosOverrideArgs.size);

 NvU64 ring[9]={0};
 assert(!_kgspIdentityCopyRecord(NULL,9,0x12345000));
 for(int pos=1;pos<9;pos++){
  memset(ring,0,sizeof(ring));int sz=pos==1?8:pos-1;int pa=sz==1?8:sz-1;int ts=pos==8?1:pos+1;
  ring[pos]=0x02040000003b1518ULL;ring[sz]=KGSP_IDENTITY_ROM_SIZE;ring[pa]=0x12345000;ring[ts]=1234;
  assert(_kgspIdentityCopyRecord(ring,9,0x12345000));
  assert(!_kgspIdentityCopyRecord(ring,9,0x23456000));
  ring[pos]^=1;assert(!_kgspIdentityCopyRecord(ring,9,0x12345000));ring[pos]^=1;
  ring[sz]--;assert(!_kgspIdentityCopyRecord(ring,9,0x12345000));ring[sz]++;
  ring[ts]=0;assert(!_kgspIdentityCopyRecord(ring,9,0x12345000));
 }
 _kgspCheckIdentityVbiosLog(&k);
 assert(unmaps==2);free(rom);puts("PASS: opt-in/device guards, exact ROM SHA256, six injected failures, DMA copy/padding, repeated preparation, resume exclusion, idempotent cleanup, all ring-wrap positions and false-marker rejection");
}
