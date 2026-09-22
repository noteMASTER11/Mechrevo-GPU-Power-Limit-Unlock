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
_Static_assert(sizeof(GSP_POWER_PROBE_PARAMS)==336,"semantic power ABI");
static int semanticSeen,semanticResult=-1;
static GSP_POWER_PROBE_PARAMS semanticLast;

static int command(int (*next)(int,unsigned long,...),int fd,unsigned long req,const Control *info,
                   unsigned operation,unsigned target,unsigned stock,GSP_POWER_PROBE_PARAMS *out){
 GSP_POWER_PROBE_PARAMS p={.version=GSP_POWER_PROBE_VERSION,.operation=operation,.targetMw=target};
 Control q=*info;q.cmd=GSP_POWER_PROBE_CMD;q.params=(uintptr_t)&p;q.size=sizeof(p);q.status=0;
 int rc=next(fd,req,&q);
 fprintf(stderr,"GSP_SEMANTIC {\"op\":%u,\"rc\":%d,\"status\":%u,\"result\":%u,\"stage\":%u,\"resolver\":%u,\"post_resolver\":%u,\"calls\":%u,\"armed\":%u,\"active\":%u,\"poisoned\":%u,\"writes\":%u,\"rollback_writes\":%u,\"stock\":%u,\"current\":%u,\"base\":%u,\"entry13\":%u,\"entry14\":%u,\"tgp_current\":%u,\"ctgp_offset\":%u,\"ctgp_active\":%u,\"current_record\":[%u,%u,%u,%u,%u],\"heap_source\":\"0x%llx\",\"heap_size\":%llu,\"va_base\":\"0x%llx\",\"array\":\"0x%llx\",\"board\":\"0x%llx\",\"entry13_object\":\"0x%llx\",\"entry14_object\":\"0x%llx\",\"writers\":[\"0x%llx\",\"0x%llx\",\"0x%llx\",\"0x%llx\"]}\n",
  operation,rc,q.status,p.result,p.stage,p.resolverStatus,p.postActivationResolverStatus,p.transferCalls,p.armed,p.active,p.poisoned,p.writes,
  p.rollbackWrites,p.stockUpperMw,p.currentUpperMw,p.currentBaseMw,p.currentEntry13,p.currentEntry14,
  p.currentCurrentMw,p.ctgpOffsetMw,p.ctgpActive,
  p.currentRecordWords[0],p.currentRecordWords[1],p.currentRecordWords[2],p.currentRecordWords[3],p.currentRecordWords[4],
  (unsigned long long)p.heapSource,(unsigned long long)p.heapSize,
  (unsigned long long)p.vaBase,(unsigned long long)p.policyArray,(unsigned long long)p.boardObject,
  (unsigned long long)p.entry13Object,(unsigned long long)p.entry14Object,
  (unsigned long long)p.maxEffectiveMember,(unsigned long long)p.maxSourceMember,(unsigned long long)p.pmgrUpperMember,
  (unsigned long long)p.baseInternalMember);
 if(out)*out=p;
 semanticLast=p;
 if(rc||q.status||p.result||p.stage!=100||p.poisoned||p.resolverStatus||p.version!=GSP_POWER_PROBE_VERSION)return -1;
 if(operation==1&&(p.currentUpperMw!=stock||p.currentBaseMw!=150000||p.currentEntry13!=210000||p.currentEntry14!=60000||p.currentCurrentMw!=p.stockCurrentMw))return -1;
 if(operation>=2&&(p.currentUpperMw!=target||p.currentBaseMw!=225000||p.currentEntry13!=250000||p.currentEntry14!=100000||p.currentCurrentMw!=225000))return -1;
 if(operation==0&&(p.armed||p.active||p.writes))return -1;
 if(operation==1&&(!p.armed||p.active||p.writes))return -1;
 if(operation==2&&(!p.armed||!p.active||p.writes!=16||p.rollbackWrites||!p.ctgpActive||
                   p.ctgpOffsetMw!=p.currentBaseMw-p.stockCurrentMw))return -1;
 return 0;
}
static int run_power(int (*next)(int,unsigned long,...),int fd,unsigned long req,const Control *info,unsigned target,int activate){
 GSP_POWER_PROBE_PARAMS p;
 if(command(next,fd,req,info,0,target,0,&p))return -1;
 unsigned stock=p.stockUpperMw;if(stock<50000||stock>=target)return -1;
 if(!activate)return 0;
 if(command(next,fd,req,info,1,target,stock,NULL))return -1;
 if(command(next,fd,req,info,2,target,stock,NULL))return -1;
 return 0;
}
#ifndef PROBE_TEST
static int marker_ok(void){char b[80]={0};FILE *f=fopen("/sys/module/nvidia/parameters/GspReadProbeBuild","r");
 int ok=f&&fgets(b,sizeof(b),f)&&!strcmp(b,"semantic-tgp-v11-20260922\n");if(f)fclose(f);return ok;}
int ioctl(int fd,unsigned long req,...){
 static int (*next)(int,unsigned long,...);static int once;if(!next)next=dlsym(RTLD_NEXT,"ioctl");
 va_list ap;va_start(ap,req);void *arg=va_arg(ap,void*);va_end(ap);int rc=next(fd,req,arg),saved=errno;
 if(!once&&!rc&&arg&&_IOC_TYPE(req)=='F'&&_IOC_NR(req)==0x2a&&_IOC_SIZE(req)==32){Control *info=arg;
  if(info->cmd==0x2080a618&&info->size==20512&&!info->status){
   once=1;const char *mode=getenv("CODEX_GSP_POWER_MODE"),*value=getenv("CODEX_GSP_TARGET_MW");char *end=NULL;
   unsigned long target=value?strtoul(value,&end,10):0;int activate=mode&&!strcmp(mode,"activate");
   if(geteuid()!=0||!marker_ok()||!mode||(!activate&&strcmp(mode,"inspect"))||!value||!*value||*end||target<200000||target>300000){fprintf(stderr,"GSP_SEMANTIC ERROR preflight\n");goto done;}
   sigset_t blocked,old;sigemptyset(&blocked);sigaddset(&blocked,SIGINT);sigaddset(&blocked,SIGTERM);sigaddset(&blocked,SIGHUP);
   if(sigprocmask(SIG_BLOCK,&blocked,&old)){fprintf(stderr,"GSP_SEMANTIC ERROR signal mask\n");goto done;}
   int result=run_power(next,fd,req,info,(unsigned)target,activate);
   semanticSeen=1;semanticResult=result;
   fprintf(stderr,"GSP_SEMANTIC %s mode=%s target=%lu\n",result?"ERROR":"SUCCESS",mode,target);
   sigprocmask(SIG_SETMASK,&old,NULL);
  }
 }
done:errno=saved;return rc;
}
int semantic_transaction_seen(void){return semanticSeen;}
int semantic_transaction_result(void){return semanticResult;}
const GSP_POWER_PROBE_PARAMS *semantic_transaction_last(void){return &semanticLast;}
#endif
