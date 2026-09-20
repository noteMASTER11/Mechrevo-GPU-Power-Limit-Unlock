/* SPDX-License-Identifier: MIT. Fixed cTGP sequence; caller validates GPU/policy
 * ownership and 225 W ceilings before entry. No arbitrary command forwarding. */
#ifndef CODEX_GSP_CTGP_PROBE_H
#define CODEX_GSP_CTGP_PROBE_H
typedef struct { NvU32 used,active,poisoned; } GSP_CTGP_STATE;
typedef struct {
 void *context;
 NV_STATUS (*read)(void *,NvU64,void *,NvU32);
 NV_STATUS (*control)(void *,NvU32,NvU32);
} GSP_CTGP_IO;
static NV_STATUS gspCtgpRun(GSP_CTGP_STATE *s,GSP_CTGP_IO *io,NvU64 pmgr,NvU64 board,NvU32 op,NvU32 *current)
{
 NvU8 flags[4]={0},index=0;NvU32 words[5]={0};NV_STATUS st;NvU32 before;
 if(op!=4&&op!=5)return NV_ERR_INVALID_ARGUMENT;
 if(s->poisoned || (op==4&&s->used) || (op==5&&!s->active))return NV_ERR_INVALID_STATE;
 st=io->read(io->context,pmgr+0x3d08,flags,4);if(st!=NV_OK)goto readFailed;
 st=io->read(io->context,pmgr+0x3d0c,words,sizeof(words));if(st!=NV_OK)goto readFailed;
 st=io->read(io->context,pmgr+0x26b7,&index,1);if(st!=NV_OK)goto readFailed;
 st=io->read(io->context,board+0x1f8,&before,4);if(st!=NV_OK)goto readFailed;
 if(flags[0]!=1 || index!=2 || words[2]!=2 || words[3]!=80000 || words[4]!=225000 ||
    (words[0]!=0xffffffffU && (words[0]<80000 || words[0]>175000)))return NV_ERR_INVALID_DATA;
 if(op==4 && (flags[1] || flags[2] || words[1] || before>175000))return NV_ERR_INVALID_STATE;
 if(op==5 && (flags[1]!=1 || flags[2]!=1 || words[1]!=145000 || before>225000))return NV_ERR_INVALID_STATE;
 s->used=1;s->active=1;
 st=io->control(io->context,0x20800ad3U,op==4?145000:0);if(st!=NV_OK)goto uncertain;
 st=io->read(io->context,pmgr+0x3d08,flags,4);if(st!=NV_OK)goto uncertain;
 st=io->read(io->context,pmgr+0x3d10,&words[1],4);if(st!=NV_OK)goto uncertain;
 if(flags[2]!=1 || words[1]!=(op==4?145000U:0U)){st=NV_ERR_INVALID_DATA;goto uncertain;}
 st=io->control(io->context,0x20800ad2U,op==4?1:0);if(st!=NV_OK)goto uncertain;
 st=io->read(io->context,pmgr+0x3d08,flags,4);if(st!=NV_OK)goto uncertain;
 st=io->read(io->context,board+0x1f8,current,4);if(st!=NV_OK)goto uncertain;
 if(flags[1]!=(op==4?1:0) || *current>225000){st=NV_ERR_INVALID_DATA;goto uncertain;}
 if(op==5){if(*current>175000){st=NV_ERR_INVALID_DATA;goto uncertain;}s->active=0;}
 /* Lower effective current may reflect another source; preserve explicit release. */
 return op==4&&*current!=225000?NV_ERR_INVALID_STATE:NV_OK;
readFailed:
 if(s->active)s->poisoned=1;
 return st;
uncertain:
 s->poisoned=1;return st;
}
#endif
