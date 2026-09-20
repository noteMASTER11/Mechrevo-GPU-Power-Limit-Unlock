/* Disposable read-only observer: forwards ioctls unchanged; only logs RM control. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
typedef struct { uint32_t client, object, cmd, flags; uint64_t params; uint32_t size, status; } rm_control;
_Static_assert(sizeof(rm_control) == 32, "NVOS54 layout");
int ioctl(int fd, unsigned long req, ...) {
    static int (*real_ioctl)(int, unsigned long, ...);
    if (!real_ioctl) real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    va_list ap; va_start(ap, req); void *arg = va_arg(ap, void *); va_end(ap);
    int observed = (_IOC_TYPE(req) == 'F' && _IOC_NR(req) == 0x2a && _IOC_SIZE(req) == 32 && arg);
    rm_control before = {0};
    if (observed) memcpy(&before, arg, sizeof(before));
    int result = real_ioctl(fd, req, arg); int saved_errno = errno;
    if (observed) {
        rm_control after; memcpy(&after, arg, sizeof(after));
        fprintf(stderr, "RM cmd=%08x flags=%x size=%u status=%x rc=%d data=", before.cmd, before.flags, before.size, after.status, result);
        if (!result && !after.status && before.params && before.size <= 65536) {
            const unsigned char *p = (const unsigned char *)(uintptr_t)before.params;
            unsigned n = before.size;
            for (unsigned i=0; i<n; i++) fprintf(stderr, "%02x", p[i]);
        }
        fputc('\n', stderr);
    }
    errno = saved_errno; return result;
}
