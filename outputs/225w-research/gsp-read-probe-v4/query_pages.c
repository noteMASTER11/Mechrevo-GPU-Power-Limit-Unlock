/* SPDX-License-Identifier: MIT. Explicit bounded read-only heap collector. */
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
#include <fcntl.h>
#include "nvtypes.h"
#include "ctrl/ctrl2080/ctrl2080internal.h"
#include "deprecated/gsp_read_probe.h"
typedef struct {uint32_t client,object,cmd,flags;uint64_t params;uint32_t size,status;} Control;
_Static_assert(sizeof(Control)==32,"NVOS54 ABI");
_Static_assert(sizeof(GSP_READ_PROBE_PARAMS)==4200,"probe ABI");
static int probe_pages(int (*next)(int,unsigned long,...),int fd,unsigned long req,
 const Control *info,uint64_t offset,unsigned count,FILE *out){
 if(count>8192 || (offset&4095) || offset>GSP_READ_PROBE_HEAP_SIZE ||
    (uint64_t)count*4096>GSP_READ_PROBE_HEAP_SIZE-offset || (count&&!out) || (!count&&offset))return -1;
 for(unsigned i=0;i<(count?count:1);i++){
  GSP_READ_PROBE_PARAMS p={.version=4,.operation=count?1:0,.pageOffset=offset+(uint64_t)i*4096};
  Control q=*info;q.cmd=GSP_READ_PROBE_CMD;q.params=(uintptr_t)&p;q.size=sizeof(p);q.status=0;
  int rc=next(fd,req,&q);
  if(rc || q.status || p.rpcStatus || p.retainedBuffer || p.version!=4 ||
     p.stage!=(count?3U:1U) || p.bytesRead!=(count?4096U:0U) ||
     p.pageOffset!=offset+(uint64_t)i*4096 ||
     (count && p.source!=GSP_READ_PROBE_SOURCE+p.pageOffset)){
   fprintf(stderr,"GSP_PAGES ERROR page=%u rc=%d status=%x stage=%u rpc=%x bytes=%u retained=%u\n",i,rc,q.status,p.stage,p.rpcStatus,p.bytesRead,p.retainedBuffer);return -1;
  }
  if(!count)fprintf(stderr,"GSP_PAGES METADATA wpr=%llx..%llx calls=%u retained=%u\n",(unsigned long long)p.wprStart,(unsigned long long)p.wprEnd,p.callsUsed,p.retainedBuffer);
  else {
   if(fwrite(p.data,1,4096,out)!=4096 || fflush(out)!=0){perror("heap output");return -1;}
   if((i+1)%256==0)fprintf(stderr,"GSP_PAGES progress=%u/%u\n",i+1,count);
#ifndef PROBE_TEST
   usleep(5000);
#endif
  }
 }
 fprintf(stderr,"GSP_PAGES SUCCESS offset=%llx pages=%u bytes=%llu\n",(unsigned long long)offset,count,(unsigned long long)count*4096);
 return 0;
}
#ifndef PROBE_TEST
static int module_ok(void){
 char marker[80]={0};FILE *f=fopen("/sys/module/nvidia/parameters/GspReadProbeBuild","r");if(!f)return 0;
 int ok=fgets(marker,sizeof(marker),f)!=NULL&&!strcmp(marker,"heap-pages-v4-20260920\n");fclose(f);return ok;
}
static int number(const char *name,uint64_t *v){
 const char *s=getenv(name);char *end;if(!s||!*s||*s=='-')return 0;
 errno=0;*v=strtoull(s,&end,0);return !errno&&!*end;
}
int ioctl(int fd,unsigned long req,...){
 static int (*next)(int,unsigned long,...);static int once;
 if(!next)next=dlsym(RTLD_NEXT,"ioctl");
 va_list ap;va_start(ap,req);void *arg=va_arg(ap,void*);va_end(ap);
 int rc=next(fd,req,arg),saved_errno=errno;
 if(!once&&!rc&&_IOC_TYPE(req)=='F'&&_IOC_NR(req)==0x2a&&_IOC_SIZE(req)==32&&arg){
  Control *info=arg;
  if(info->cmd==0x2080a618&&!info->status){
   once=1;uint64_t offset,count;FILE *out=NULL;
   if(!module_ok()||!number("CODEX_GSP_OFFSET",&offset)||!number("CODEX_GSP_COUNT",&count)||count>8192){fprintf(stderr,"GSP_PAGES refused marker or parameters\n");goto done;}
   if(count){
    const char *path=getenv("CODEX_GSP_OUTPUT");if(!path)goto done;
    int ofd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(ofd<0){perror("open output");goto done;}
    out=fdopen(ofd,"wb");if(!out){close(ofd);goto done;}
   }
   probe_pages(next,fd,req,info,offset,(unsigned)count,out);
   if(out&&fclose(out)!=0)fprintf(stderr,"GSP_PAGES ERROR closing output\n");
  }
 }
 done:errno=saved_errno;return rc;
}
#endif
