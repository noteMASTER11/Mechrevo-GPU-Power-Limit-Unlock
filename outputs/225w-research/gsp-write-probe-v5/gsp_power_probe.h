/* SPDX-License-Identifier: MIT. Local research ABI; no arbitrary writes. */
#ifndef CODEX_GSP_POWER_PROBE_H
#define CODEX_GSP_POWER_PROBE_H
#define GSP_POWER_PROBE_CMD 0x2080ff72U
#define GSP_POWER_PROBE_VERSION 1U
#define GSP_POWER_HEAP_VA 0x7f2000000ULL
typedef struct {
 NvU32 version,operation;
 NvU64 gpuOffset,pmgrOffset,boardOffset;
 NvU32 reserved[4];
 NvU32 stage,result,armed,active,poisoned,writes;
 NvU32 maxValue,feValue,upperValue,currentValue;
} GSP_POWER_PROBE_PARAMS;
typedef struct {
 NvU32 armed,used,active,poisoned,writes;
 NvU64 gpu,pmgr,board;
} GSP_POWER_STATE;
typedef struct {
 void *context;
 NV_STATUS (*read)(void *,NvU64,void *,NvU32);
 NV_STATUS (*write)(void *,NvU64,NvU32);
} GSP_POWER_IO;

static int gpRange(NvU64 o,NvU64 n) {
 return !(o&7) && n<=0x73dc000ULL && o<=0x73dc000ULL-n;
}
static int gpDisjoint(NvU64 a,NvU64 an,NvU64 b,NvU64 bn) {
 return a+an<=b || b+bn<=a;
}
static NV_STATUS gpCheck(GSP_POWER_IO *io,NvU64 o,NvU64 want,NvU32 n) {
 NvU64 value=0;NV_STATUS st=io->read(io->context,o,&value,n);
 return st!=NV_OK?st:(value==want?NV_OK:NV_ERR_INVALID_DATA);
}
static NV_STATUS gpValidate(GSP_POWER_IO *io,GSP_POWER_PROBE_PARAMS *p,NvU32 expected) {
 NvU64 array=0,group=0;NvU32 mask=0;NV_STATUS st;
#define GP_CHECK(o,v,n) do{st=gpCheck(io,(o),(v),(n));if(st!=NV_OK)return st;}while(0)
#define GP_READ(o,d,n) do{st=io->read(io->context,(o),(d),(n));if(st!=NV_OK)return st;}while(0)
 p->stage=10;
 GP_CHECK(p->gpuOffset+0xa0,GSP_POWER_HEAP_VA+p->gpuOffset,8);
 GP_CHECK(p->gpuOffset+0x2210,GSP_POWER_HEAP_VA+p->pmgrOffset,8);
 GP_CHECK(p->pmgrOffset+0x1b8,GSP_POWER_HEAP_VA+p->pmgrOffset,8);
 GP_CHECK(p->pmgrOffset+0x60,GSP_POWER_HEAP_VA+p->gpuOffset,8);
 GP_READ(p->pmgrOffset+0x1cc0,&array,8);GP_READ(p->pmgrOffset+0x1cc8,&group,8);
 if(array<GSP_POWER_HEAP_VA || group<GSP_POWER_HEAP_VA ||
    !gpRange(array-GSP_POWER_HEAP_VA,256) || !gpRange(group-GSP_POWER_HEAP_VA,16))return NV_ERR_INVALID_DATA;
 GP_CHECK(array-GSP_POWER_HEAP_VA+16,GSP_POWER_HEAP_VA+p->boardOffset,8);
 GP_READ(group-GSP_POWER_HEAP_VA+8,&mask,4);
 if(!(mask&4))return NV_ERR_INVALID_DATA;
 p->stage=11;
 GP_CHECK(p->boardOffset,0,1);GP_CHECK(p->boardOffset+2,2,2);
 GP_CHECK(p->boardOffset+8,0x4193460,8);
 GP_CHECK(p->boardOffset+0x28,0,1);GP_CHECK(p->boardOffset+0x2a,0,1);
 GP_CHECK(p->boardOffset+0x2d0,0x17b2248,8);
 GP_CHECK(p->boardOffset+0x2d8,0x177c0f4,8);GP_CHECK(p->boardOffset+0x2e0,0x177c250,8);
 GP_CHECK(p->boardOffset+0x104,0,1);GP_CHECK(p->boardOffset+0x105,1,1);
 GP_CHECK(p->boardOffset+0x10c,175000,4);GP_CHECK(p->boardOffset+0x110,0xfe,1);
 GP_CHECK(p->pmgrOffset+0x3d14,2,4);
 p->stage=12;
 GP_READ(p->boardOffset+0x108,&p->maxValue,4);
 GP_READ(p->boardOffset+0x114,&p->feValue,4);
 GP_READ(p->pmgrOffset+0x3d1c,&p->upperValue,4);
 GP_READ(p->boardOffset+0x1f8,&p->currentValue,4);
 if(p->maxValue!=expected || p->feValue!=expected || p->upperValue!=expected)return NV_ERR_INVALID_DATA;
 return NV_OK;
#undef GP_CHECK
#undef GP_READ
}
static NV_STATUS gpSet(GSP_POWER_STATE *s,GSP_POWER_IO *io,NvU64 off,NvU32 old,NvU32 value) {
 NV_STATUS st=gpCheck(io,off,old,4);
 if(st!=NV_OK)return st;
 ++s->writes;
 st=io->write(io->context,off,value);
 if(st==NV_OK)st=gpCheck(io,off,value,4);
 /* Any failed write/readback is indeterminate. Never blindly issue another DMA. */
 if(st!=NV_OK)s->poisoned=1;
 return st;
}
static NV_STATUS gspPowerEngine(GSP_POWER_STATE *s,GSP_POWER_IO *io,GSP_POWER_PROBE_PARAMS *p) {
 NV_STATUS st=NV_OK;NvU32 expected=s->active?225000:175000;
 if(p->version!=1 || p->operation>3 || p->reserved[0] || p->reserved[1] || p->reserved[2] || p->reserved[3] ||
    !gpRange(p->gpuOffset,0x2300) || !gpRange(p->pmgrOffset,0x4f28) || !gpRange(p->boardOffset,0x340) ||
    !gpDisjoint(p->gpuOffset,0x2300,p->pmgrOffset,0x4f28) ||
    !gpDisjoint(p->gpuOffset,0x2300,p->boardOffset,0x340) ||
    !gpDisjoint(p->pmgrOffset,0x4f28,p->boardOffset,0x340))return NV_ERR_INVALID_ARGUMENT;
 p->stage=1;
 if(s->poisoned){st=NV_ERR_INVALID_STATE;goto done;}
 if((s->armed || s->active || s->used) && (s->gpu!=p->gpuOffset || s->pmgr!=p->pmgrOffset || s->board!=p->boardOffset)){
  st=NV_ERR_INVALID_STATE;goto done;
 }
 if((p->operation==1&&(s->armed||s->used)) || (p->operation==2&&(!s->armed||s->used)) || (p->operation==3&&!s->active)){
  st=NV_ERR_INVALID_STATE;goto done;
 }
 st=gpValidate(io,p,expected);if(st!=NV_OK)goto done;
 if(p->operation==0){p->stage=100;goto done;}
 /* No reduce/raise cycle when another actor already requested >175W. */
 if(p->currentValue>175000){st=NV_ERR_INVALID_STATE;goto done;}
 if(p->operation==1){
  p->stage=20;st=gpSet(s,io,p->boardOffset+0x114,175000,175000);
  if(st==NV_OK){s->armed=1;s->gpu=p->gpuOffset;s->pmgr=p->pmgrOffset;s->board=p->boardOffset;}
 } else {
  NvU32 value=p->operation==2?225000:175000;
  if(p->operation==2){s->used=1;s->active=1;}
  p->stage=21;st=gpSet(s,io,p->boardOffset+0x114,expected,value);
  if(st==NV_OK){p->stage=22;st=gpSet(s,io,p->boardOffset+0x108,expected,value);}
  if(st==NV_OK){p->stage=23;st=gpSet(s,io,p->pmgrOffset+0x3d1c,expected,value);}
  /* Even a definite allocation/precondition failure can leave a partial update.
   * Stop rather than invent a restoration when the object changed concurrently. */
  if(st!=NV_OK)s->poisoned=1;
  else if(p->operation==3)s->active=0;
 }
 if(st==NV_OK){st=gpValidate(io,p,s->active?225000:175000);if(st!=NV_OK)s->poisoned=1;}
 if(st==NV_OK)p->stage=100;
 done:p->result=st;p->armed=s->armed;p->active=s->active;p->poisoned=s->poisoned;p->writes=s->writes;return st;
}
#endif
