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
#include "ctrl/ctrl2080/ctrl2080fb.h"
#include "ctrl/ctrl2080/ctrl2080internal.h"
#include "deprecated/gsp_read_probe.h"
typedef struct { uint32_t client,object,cmd,flags; uint64_t params; uint32_t size,status; } Control;
_Static_assert(sizeof(Control)==32,"NVOS54 ABI");
_Static_assert(sizeof(GSP_READ_PROBE_PARAMS)==144,"probe ABI");
static int module_ok(void) {
 char marker[80]={0}; FILE *f=fopen("/sys/module/nvidia/parameters/GspReadProbeBuild","r");
 if (!f) return 0;
 int ok=fgets(marker,sizeof(marker),f)!=NULL && !strcmp(marker,"image-read-v1-20260920\n");
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
    NV2080_CTRL_FB_GET_WPR_REGION_INFO_PARAMS p={0};
    Control q=*info;q.cmd=NV2080_CTRL_CMD_FB_GET_WPR_REGION_INFO;q.params=(uintptr_t)&p;q.size=sizeof(p);q.status=0;
    int qr=next(fd,req,&q);
    fprintf(stderr,"WPR_GET rc=%d status=0x%x wpr1=%llx..%llx wpr2=%llx..%llx region=%llx\n",qr,q.status,
     (unsigned long long)p.wpr1Start,(unsigned long long)p.wpr1End,
     (unsigned long long)p.wpr2Start,(unsigned long long)p.wpr2End,(unsigned long long)p.fbRegionOfWpr2Start);
   }
  }
 }
 errno=saved_errno;return rc;
}
