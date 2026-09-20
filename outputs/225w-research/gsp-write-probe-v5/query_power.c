#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include "nvtypes.h"
#include "nvstatus.h"
#include "deprecated/gsp_power_probe.h"
typedef struct {uint32_t client,object,cmd,flags;uint64_t params;uint32_t size,status;} Control;
_Static_assert(sizeof(Control)==32,"NVOS54");
_Static_assert(sizeof(GSP_POWER_PROBE_PARAMS)==88,"power ABI");

static int command(int (*next)(int,unsigned long,...),int fd,unsigned long req,const Control *info,GSP_POWER_PROBE_PARAMS *p){
 Control q=*info;q.cmd=GSP_POWER_PROBE_CMD;q.params=(uintptr_t)p;q.size=sizeof(*p);q.status=0;
 int rc=next(fd,req,&q);
 fprintf(stderr,"GSP_POWER {\"op\":%u,\"rc\":%d,\"status\":%u,\"result\":%u,\"stage\":%u,\"armed\":%u,\"active\":%u,\"poisoned\":%u,\"writes\":%u,\"max\":%u,\"fe\":%u,\"upper\":%u,\"current\":%u}\n",p->operation,rc,q.status,p->result,p->stage,p->armed,p->active,p->poisoned,p->writes,p->maxValue,p->feValue,p->upperValue,p->currentValue);
 unsigned want=p->operation==2?225000:175000;
 return rc||q.status||p->result||p->stage!=100||p->poisoned||p->version!=1||p->maxValue!=want||p->feValue!=want||p->upperValue!=want||p->active!=(p->operation==2)?-1:0;
}
static int public_max(int (*next)(int,unsigned long,...),int fd,unsigned long req,const Control *info,unsigned want){
 unsigned char bytes[20512]={0};Control q=*info;q.cmd=0x2080a618;q.params=(uintptr_t)bytes;q.size=sizeof(bytes);q.status=0;
 int rc=next(fd,req,&q);unsigned value=0,mask=0;memcpy(&mask,bytes+4,4);memcpy(&value,bytes+0x2e4,4);
 fprintf(stderr,"GSP_PUBLIC_MAX {\"rc\":%d,\"status\":%u,\"mask\":%u,\"value\":%u,\"expected\":%u}\n",rc,q.status,mask,value,want);
 return rc||q.status||!(mask&4)||value!=want?-1:0;
}
static int run_power(int (*next)(int,unsigned long,...),int fd,unsigned long req,const Control *info,NvU64 gpu,NvU64 pmgr,NvU64 board,int mode){
 GSP_POWER_PROBE_PARAMS p={.version=1,.gpuOffset=gpu,.pmgrOffset=pmgr,.boardOffset=board};int up,down;
 if(mode==2){p.operation=3;if(command(next,fd,req,info,&p))return -1;return public_max(next,fd,req,info,175000);}
 if(command(next,fd,req,info,&p))return -1;
 if(!mode)return 0;
 p.operation=1;if(command(next,fd,req,info,&p))return -1;
 p.operation=2;if(command(next,fd,req,info,&p))return -1;
 up=public_max(next,fd,req,info,225000);
 /* Restore even when the intermediate GET fails. No retry after an uncertain write. */
 p.operation=3;if(command(next,fd,req,info,&p))return -1;
 down=public_max(next,fd,req,info,175000);
 return up||down?-1:0;
}
#ifndef PROBE_TEST
static int number(const char *name,NvU64 *v){const char *s=getenv(name);char *end;if(!s||!*s||*s=='-')return 0;errno=0;*v=strtoull(s,&end,0);return !errno&&!*end;}
static int marker_ok(void){char b[80]={0};FILE *f=fopen("/sys/module/nvidia/parameters/GspReadProbeBuild","r");if(!f)return 0;int ok=fgets(b,sizeof(b),f)&&!strcmp(b,"power-words-v5-20260920\n");fclose(f);return ok;}
int ioctl(int fd,unsigned long req,...){
 static int (*next)(int,unsigned long,...);static int once;
 if(!next)next=dlsym(RTLD_NEXT,"ioctl");
 va_list ap;va_start(ap,req);void *arg=va_arg(ap,void*);va_end(ap);
 int rc=next(fd,req,arg),saved=errno;
 if(!once&&!rc&&arg&&_IOC_TYPE(req)=='F'&&_IOC_NR(req)==0x2a&&_IOC_SIZE(req)==32){
  Control *info=arg;
  if(info->cmd==0x2080a618&&info->size==20512&&!info->status){
   once=1;NvU64 g,p,b;int mode=-1;const char *m=getenv("CODEX_GSP_POWER_MODE");
   if(m){if(!strcmp(m,"inspect"))mode=0;if(!strcmp(m,"exercise"))mode=1;if(!strcmp(m,"restore"))mode=2;}
   if(geteuid()!=0||!marker_ok()||mode<0||!number("CODEX_GSP_GPU",&g)||!number("CODEX_GSP_PMGR",&p)||!number("CODEX_GSP_BOARD",&b)){fprintf(stderr,"GSP_POWER ERROR preflight\n");goto done;}
   sigset_t blocked,old;sigemptyset(&blocked);sigaddset(&blocked,SIGINT);sigaddset(&blocked,SIGTERM);sigaddset(&blocked,SIGHUP);
   if(sigprocmask(SIG_BLOCK,&blocked,&old)){fprintf(stderr,"GSP_POWER ERROR signal mask\n");goto done;}
   int result=run_power(next,fd,req,info,g,p,b,mode);
   fprintf(stderr,"GSP_POWER %s mode=%s\n",result?"ERROR":"SUCCESS",m);
   sigprocmask(SIG_SETMASK,&old,NULL);
  }
 }
 done:errno=saved;return rc;
}
#endif
