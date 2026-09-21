/* SPDX-License-Identifier: MIT
 * Read-only observer for NVIDIA's public WPR-region control.
 *
 * The preload library waits for an existing successful subdevice control, then
 * reuses the same RM client/object handles to issue
 * NV2080_CTRL_CMD_FB_GET_WPR_REGION_INFO.  It forwards the application's ioctl
 * unchanged and performs no memory transfer or GPU write.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#define NV2080_CTRL_CMD_FB_GET_WPR_REGION_INFO 0x20801364U

typedef struct {
    uint32_t client;
    uint32_t object;
    uint32_t cmd;
    uint32_t flags;
    uint64_t params;
    uint32_t size;
    uint32_t status;
} rm_control;

typedef struct {
    uint64_t wpr1_start;
    uint64_t wpr1_end;
    uint64_t wpr2_start;
    uint64_t wpr2_end;
    uint64_t wpr2_fb_region_start;
} wpr_region_info;

_Static_assert(sizeof(rm_control) == 32, "unexpected NVOS54 layout");
_Static_assert(sizeof(wpr_region_info) == 40, "unexpected WPR response layout");

int ioctl(int fd, unsigned long request, ...)
{
    static int (*next_ioctl)(int, unsigned long, ...);
    static int queried;
    va_list ap;
    void *arg;
    int rc;
    int saved_errno;

    if (next_ioctl == NULL)
        next_ioctl = dlsym(RTLD_NEXT, "ioctl");

    va_start(ap, request);
    arg = va_arg(ap, void *);
    va_end(ap);

    rc = next_ioctl(fd, request, arg);
    saved_errno = errno;

    if (!queried && rc == 0 && arg != NULL &&
        _IOC_TYPE(request) == 'F' && _IOC_NR(request) == 0x2a &&
        _IOC_SIZE(request) == sizeof(rm_control)) {
        rm_control observed;

        memcpy(&observed, arg, sizeof(observed));
        if (observed.status == 0 && (observed.cmd >> 16) == 0x2080) {
            wpr_region_info info = {0};
            rm_control query = observed;
            int query_rc;

            queried = 1;
            query.cmd = NV2080_CTRL_CMD_FB_GET_WPR_REGION_INFO;
            query.flags = 0;
            query.params = (uintptr_t)&info;
            query.size = sizeof(info);
            query.status = 0;

            query_rc = next_ioctl(fd, request, &query);
            fprintf(stderr,
                    "UNBOUND_WPR rc=%d status=0x%x "
                    "wpr1=0x%llx..0x%llx wpr2=0x%llx..0x%llx "
                    "fb_region=0x%llx\n",
                    query_rc, query.status,
                    (unsigned long long)info.wpr1_start,
                    (unsigned long long)info.wpr1_end,
                    (unsigned long long)info.wpr2_start,
                    (unsigned long long)info.wpr2_end,
                    (unsigned long long)info.wpr2_fb_region_start);
        }
    }

    errno = saved_errno;
    return rc;
}
