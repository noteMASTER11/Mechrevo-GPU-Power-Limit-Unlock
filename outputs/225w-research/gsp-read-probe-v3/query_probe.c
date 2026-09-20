/* SPDX-License-Identifier: MIT. Private command only with this experiment's
 * runtime module marker present. All original NVML ioctls forwarded unchanged. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "nvtypes.h"
#include "ctrl/ctrl2080/ctrl2080internal.h"
#include "deprecated/gsp_read_probe.h"
typedef struct { uint32_t client,object,cmd,flags; uint64_t params; uint32_t size,status; } Control;
_Static_assert(sizeof(Control)==32,"NVOS54 ABI");
_Static_assert(sizeof(GSP_READ_PROBE_PARAMS)==336,"probe ABI");
static int module_ok(void) {
 char marker[80]={0}; FILE *f=fopen("/sys/module/nvidia/parameters/GspReadProbeBuild","r");
 if (!f) return 0;
 int ok=fgets(marker,sizeof(marker),f)!=NULL && !strcmp(marker,"heap-read-v3-20260920\n");
 fclose(f); return ok;
}
int ioctl(int fd,unsigned long req,...) {
 static int (*next)(int,unsigned long,...);
 if (!next) next=dlsym(RTLD_NEXT,"ioctl");
 va_list ap;va_start(ap,req);void *arg=va_arg(ap,void*);va_end(ap);
 int rc=next(fd,req,arg), saved_errno=errno;
 static int once;
 if (!once && !rc && _IOC_TYPE(req)=='F' && _IOC_NR(req)==0x2a && _IOC_SIZE(req)==32 && arg) {
  Control *info=arg;
  if (info->cmd==0x2080a618 && info->status==0) {
   once=1;
   if (!module_ok()) { fprintf(stderr,"GSP_PROBE refused: experimental module is not loaded\n"); }
   else {
    GSP_READ_PROBE_PARAMS p={.version=GSP_READ_PROBE_VERSION};
    const char *mode=getenv("CODEX_GSP_READ_HEAP");
    if (mode && !strcmp(mode,"1")) p.operation=1;
    Control q=*info;q.cmd=GSP_READ_PROBE_CMD;q.params=(uintptr_t)&p;q.size=sizeof(p);q.status=0;
    int qr=next(fd,req,&q);
    fprintf(stderr,"GSP_PROBE rc=%d status=0x%x operation=%u stage=%u rpc_status=0x%x bytes=%u retained=%u\n",
     qr,q.status,p.operation,p.stage,p.rpcStatus,p.bytesRead,p.retainedBuffer);
    if (!qr && !q.status) {
     fprintf(stderr,"GSP_PROBE wpr=%llx..%llx image=%llx size=%llx heap=%llx size=%llx source=%llx data=",
      (unsigned long long)p.wprStart,(unsigned long long)p.wprEnd,(unsigned long long)p.image,
      (unsigned long long)p.imageSize,(unsigned long long)p.heap,(unsigned long long)p.heapSize,(unsigned long long)GSP_READ_PROBE_SOURCE);
     for (unsigned i=0;i<p.bytesRead && i<sizeof(p.data);i++)fprintf(stderr,"%02x",p.data[i]);
     fputc('\n',stderr);
    }
   }
  }
 }
 errno=saved_errno;return rc;
}
