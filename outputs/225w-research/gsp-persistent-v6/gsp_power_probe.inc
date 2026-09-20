/* SPDX-License-Identifier: MIT. Called under the same root/API/GPU locks as v4.
 * Only the validated engine can select three four-byte policy destinations.
 * No commands are run at module load, exit, or automatically on boot.
 */
typedef struct {OBJGPU *gpu;GSP_READ_PROBE_PARAMS *page;} GSP_POWER_CONTEXT;
static GSP_POWER_STATE gspPowerState;
#include "deprecated/gsp_ctgp_probe.h"
static GSP_CTGP_STATE gspCtgpState;
static NV_STATUS gspPowerRead(void *opaque,NvU64 offset,void *dest,NvU32 size)
{
 GSP_POWER_CONTEXT *c=opaque;NV_STATUS st;NvU32 done=0;
 if(offset>GSP_READ_PROBE_HEAP_SIZE || size>GSP_READ_PROBE_HEAP_SIZE-offset)return NV_ERR_INVALID_ARGUMENT;
 while(done<size){
  NvU64 at=offset+done;NvU32 inPage=(NvU32)(at&4095),n=4096-inPage;
  if(n>size-done)n=size-done;
  portMemSet(c->page,0,sizeof(*c->page));c->page->version=4;c->page->operation=1;c->page->pageOffset=at&~4095ULL;
  gspProbe_internalRead=NV_TRUE;
  st=gspReadProbe(c->gpu,c->page,sizeof(*c->page));
  gspProbe_internalRead=NV_FALSE;
  if(st!=NV_OK)return st;
  if(c->page->rpcStatus!=NV_OK)return c->page->rpcStatus;
  if(c->page->stage!=3 || c->page->bytesRead!=4096 || c->page->retainedBuffer)return NV_ERR_INVALID_STATE;
  portMemCopy((NvU8*)dest+done,size-done,c->page->data+inPage,n);done+=n;
 }
 return NV_OK;
}
static NV_STATUS gspPowerWrite(void *opaque,NvU64 offset,NvU32 value)
{
 GSP_POWER_CONTEXT *c=opaque;OBJGPU *g=c->gpu;
 MEMORY_DESCRIPTOR *desc=NULL;void *map=NULL,*priv=NULL;NV_STATUS st;
 NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS x={0};
 RM_API *api=GPU_GET_PHYSICAL_RMAPI(g);NvU64 pa;
 /* Defense in depth: engine must bind targets before its first write. */
 if((offset!=gspPowerState.board+0x114 && offset!=gspPowerState.board+0x108 && offset!=gspPowerState.pmgr+0x3d1c) ||
    (value!=175000 && value!=225000) || (offset&3) || offset>GSP_READ_PROBE_HEAP_SIZE-4 ||
    gspProbe_poisoned || gspProbe_callsUsed>=GSP_READ_PROBE_MAX_CALLS)return NV_ERR_INVALID_STATE;
 st=memdescCreate(&desc,g,4096,4096,NV_TRUE,ADDR_SYSMEM,NV_MEMORY_CACHED,0);
 if(st!=NV_OK)return st;
 st=memdescAlloc(desc);if(st!=NV_OK)goto cleanup;
 st=memdescMapOld(desc,0,4096,NV_TRUE,NV_PROTECT_READ_WRITE,&map,&priv);if(st!=NV_OK)goto cleanup;
 portMemSet(map,0,4096);portMemCopy(map,4096,&value,4);
 pa=memdescGetPhysAddr(desc,AT_GPU,0);
 if(!pa || (pa&4095) || pa>(~(NvU64)0)-4095){st=NV_ERR_INVALID_DATA;goto cleanup;}
 x.src.baseAddr=pa;x.src.size=4096;x.src.aperture=1;x.src.cpuCacheAttrib=0;
 x.dst.baseAddr=GSP_READ_PROBE_SOURCE+(offset&~4095ULL);x.dst.size=4096;
 x.dst.offset=offset&4095;x.dst.aperture=2;x.dst.cpuCacheAttrib=0;
 x.transferSize=4;x.memop=NV2080_CTRL_MEMMGR_MEMORY_OP_MEMCPY;
 ++gspProbe_callsUsed;
 st=api->Control(api,g->hInternalClient,g->hInternalSubdevice,NV2080_CTRL_CMD_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP,&x,sizeof(x));
 if(st!=NV_OK){
  gspProbe_poisoned=NV_TRUE;gspProbe_retainedDesc=desc;gspProbe_retainedMap=map;gspProbe_retainedPriv=priv;
  NV_PRINTF(LEVEL_ERROR,"GSP power probe RPC failed: 0x%x; source retained until reboot\n",st);
  return st;
 }
cleanup:
 if(map!=NULL)memdescUnmapOld(desc,NV_TRUE,map,priv);
 memdescFree(desc);memdescDestroy(desc);return st;
}
static NV_STATUS gspCtgpControl(void *opaque,NvU32 command,NvU32 value)
{
 GSP_POWER_CONTEXT *c=opaque;OBJGPU *g=c->gpu;NV_STATUS st;
 RM_API *api=GPU_GET_PHYSICAL_RMAPI(g);
 NV2080_CTRL_CMD_INTERNAL_PMGR_PFM_REQ_HNDLR_CONFIGURE_TURBO_V2_PARAMS offset={0};
 NV2080_CTRL_CMD_INTERNAL_PMGR_PFM_REQ_HNDLR_CONFIGURE_TGP_MODE_PARAMS mode={0};
 if(gspProbe_poisoned || gspCtgpState.poisoned)return NV_ERR_INVALID_STATE;
 if(command==0x20800ad3U && (value==145000 || value==0)){
  offset.ctgpOffsetmW=value;
  st=api->Control(api,g->hInternalClient,g->hInternalSubdevice,command,&offset,sizeof(offset));
 }else if(command==0x20800ad2U && value<=1){
  mode.bEnable=(NvBool)value;
  st=api->Control(api,g->hInternalClient,g->hInternalSubdevice,command,&mode,sizeof(mode));
 }else return NV_ERR_INVALID_ARGUMENT;
 /* A failed RPC may have changed firmware state. Never retry automatically. */
 if(st!=NV_OK)gspProbe_poisoned=NV_TRUE;
 return st;
}
static NV_STATUS gspPowerProbe(OBJGPU *g,GSP_POWER_PROBE_PARAMS *p,NvU32 size)
{
 GSP_POWER_CONTEXT ctx={0};GSP_POWER_IO io={&ctx,gspPowerRead,gspPowerWrite};NV_STATUS st;
 if(size!=sizeof(*p))return NV_ERR_INVALID_ARGUMENT;
 p->stage=0;p->result=0;p->armed=0;p->active=0;p->poisoned=0;p->writes=0;
 p->maxValue=p->feValue=p->upperValue=p->currentValue=0;
 if(gspProbe_poisoned)gspPowerState.poisoned=1;
 /* Leave ample DMA budget for the entire apply+restore sequence. */
 if(gspProbe_callsUsed>GSP_READ_PROBE_MAX_CALLS-(p->operation==3?128:(p->operation==5?256:512))){st=NV_ERR_INVALID_STATE;goto output;}
 ctx.gpu=g;ctx.page=portMemAllocNonPaged(sizeof(*ctx.page));
 if(ctx.page==NULL){st=NV_ERR_NO_MEMORY;goto output;}
 /* Root gate is in the dispatcher; metadata check enforces GPU/CC/WPR layout. */
 portMemSet(ctx.page,0,sizeof(*ctx.page));ctx.page->version=4;
 st=gspReadProbe(g,ctx.page,sizeof(*ctx.page));
 if(st==NV_OK){
  /* Bind once for the first identity write. The engine validates these offsets
   * and every ownership link before gspPowerWrite can be reached. */
  if(!gspPowerState.armed&&!gspPowerState.used&&!gspPowerState.poisoned){
   gspPowerState.gpu=p->gpuOffset;gspPowerState.pmgr=p->pmgrOffset;gspPowerState.board=p->boardOffset;
  }
  if(p->operation==1)gspProbe_reserveRestore=NV_TRUE;
  if(p->operation==4 || p->operation==5){
   NvU32 operation=p->operation;
   GSP_CTGP_IO ctgpIo={&ctx,gspPowerRead,gspCtgpControl};
   /* Reuse every v5 object/ABI/ownership check before internal commands. */
   p->operation=0;st=gspPowerEngine(&gspPowerState,&io,p);p->operation=operation;
   if(st==NV_OK && !gspPowerState.active)st=NV_ERR_INVALID_STATE;
   if(st==NV_OK){
    p->stage=30;
    /* If activation rejected its preflight, no cTGP command ran. The
     * validated <=175W CURRENT permits release to proceed to ceiling restore. */
    if(operation==5 && !gspCtgpState.used && !gspCtgpState.active && p->currentValue<=175000)
     st=NV_OK;
    else st=gspCtgpRun(&gspCtgpState,&ctgpIo,p->pmgrOffset,p->boardOffset,operation,&p->currentValue);
    if(gspCtgpState.poisoned)gspPowerState.poisoned=1;
    if(st==NV_OK)p->stage=100;
   }
  }else if(p->operation==3 && gspCtgpState.active)st=NV_ERR_INVALID_STATE;
  else st=gspPowerEngine(&gspPowerState,&io,p);
  if(st==NV_OK&&p->operation==3)gspProbe_reserveRestore=NV_FALSE;
 }
output:
 p->result=st;p->poisoned=gspPowerState.poisoned||gspProbe_poisoned;
 p->active=gspPowerState.active;p->armed=gspPowerState.armed;p->writes=gspPowerState.writes;
 if(ctx.page!=NULL)portMemFree(ctx.page);
 return NV_OK; /* Return experiment status in the ABI, even on a partial update. */
}
