#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t NvU32; typedef uint64_t NvU64; typedef uint8_t NvU8; typedef int NV_STATUS;
#define NV_OK 0
#define NV_ERR_INVALID_STATE 1
#define NV_ERR_INVALID_DATA 2
#define NV_ERR_INVALID_ARGUMENT 3
#include "gsp_ctgp_probe.h"
typedef struct { unsigned char bytes[0x5000];unsigned calls,reads,failControl,failRead;unsigned commands[8],values[8]; } Fake;
static void put(Fake*f,unsigned at,unsigned val){memcpy(f->bytes+at,&val,4);}
static int rd(void*v,NvU64 at,void*d,NvU32 n){Fake*f=v;if(++f->reads==f->failRead)return 99;memcpy(d,f->bytes+at,n);return 0;}
static int ctrl(void*v,NvU32 cmd,NvU32 value){Fake*f=v;unsigned i=f->calls++;f->commands[i]=cmd;f->values[i]=value;if(f->calls==f->failControl)return 99;if(cmd==0x20800ad3){put(f,0x3d10,value);f->bytes[0x3d0a]=1;}else {f->bytes[0x3d09]=value;put(f,0x41f8,value?225000:145000);}return 0;}
static void init(Fake*f){memset(f,0,sizeof(*f));f->bytes[0x3d08]=1;f->bytes[0x26b7]=2;put(f,0x3d0c,145000);put(f,0x3d14,2);put(f,0x3d18,80000);put(f,0x3d1c,225000);put(f,0x41f8,145000);}
int main(void){Fake f;GSP_CTGP_STATE s={0};GSP_CTGP_IO io={&f,rd,ctrl};NvU32 current=0;
 init(&f);assert(!gspCtgpRun(&s,&io,0,0x4000,4,&current));assert(current==225000&&s.active&&s.used&&!s.poisoned);assert(f.calls==2&&f.commands[0]==0x20800ad3&&f.values[0]==145000&&f.commands[1]==0x20800ad2&&f.values[1]==1);
 assert(gspCtgpRun(&s,&io,0,0x4000,4,&current));assert(f.calls==2);
 assert(!gspCtgpRun(&s,&io,0,0x4000,5,&current));assert(current==145000&&!s.active);assert(f.commands[2]==0x20800ad3&&f.values[2]==0&&f.commands[3]==0x20800ad2&&f.values[3]==0);
 assert(gspCtgpRun(&s,&io,0,0x4000,4,&current));
 for(unsigned bad=1;bad<=2;bad++){init(&f);memset(&s,0,sizeof(s));f.failControl=bad;assert(gspCtgpRun(&s,&io,0,0x4000,4,&current));assert(s.poisoned&&f.calls==bad);assert(gspCtgpRun(&s,&io,0,0x4000,5,&current));assert(f.calls==bad);}
 for(unsigned bad=3;bad<=4;bad++){init(&f);memset(&s,0,sizeof(s));assert(!gspCtgpRun(&s,&io,0,0x4000,4,&current));f.failControl=bad;assert(gspCtgpRun(&s,&io,0,0x4000,5,&current));assert(s.poisoned&&f.calls==bad);}
 unsigned fields[]={0x3d08,0x3d09,0x3d0a,0x3d10,0x3d14,0x3d18,0x3d1c,0x26b7};
 for(unsigned i=0;i<sizeof(fields)/sizeof(*fields);i++){init(&f);memset(&s,0,sizeof(s));f.bytes[fields[i]]^=1;assert(gspCtgpRun(&s,&io,0,0x4000,4,&current));assert(!f.calls&&!s.active);}
 init(&f);memset(&s,0,sizeof(s));assert(!gspCtgpRun(&s,&io,0,0x4000,4,&current));unsigned total=f.reads;
 for(unsigned i=1;i<=total;i++){init(&f);memset(&s,0,sizeof(s));f.failRead=i;assert(gspCtgpRun(&s,&io,0,0x4000,4,&current));if(f.calls)assert(s.poisoned);}
 puts("ctgp: activation/release, repeat guard, four RPC faults, eight state corruptions and every activation read fault passed");}
