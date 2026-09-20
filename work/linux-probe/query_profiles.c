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
    static int control_queried;
    if (observed && !control_queried && !result && before.cmd == 0x2080a618 && before.size == 0x5020) {
        rm_control info; memcpy(&info, arg, sizeof(info));
        if (!info.status) {
            control_queried = 1;
            /* Read-only configuration Info and Control commands, exact sizes
             * recovered from this firmware's dispatcher. No SET command. */
            const uint32_t cmds[] = {0x2080a620, 0x2080a622};
            const uint32_t sizes[] = {0x8398, 0x4c};
            unsigned char data[0x8398];
            for (unsigned k=0; k<2; k++) {
                memset(data, 0, sizeof(data));
                rm_control q = before; q.cmd = cmds[k]; q.size = sizes[k];
                q.params = (uintptr_t)data; q.status = 0;
                int qr = real_ioctl(fd, req, &q);
                fprintf(stderr, "PROFILE_GET cmd=%08x size=%u status=%x rc=%d\n", q.cmd, q.size, q.status, qr);
                if (!qr && !q.status) {
                    const char *path = k ? "work/linux-probe/profile-control.bin" : "work/linux-probe/profile-info.bin";
                    FILE *f = fopen(path, "wb");
                    if (f) { fwrite(data, 1, q.size, f); fclose(f); }
                    for (unsigned i=0; i<q.size && i<128; i++) fprintf(stderr, "%02x", data[i]);
                    fputc('\n', stderr);
                }
            }
        }
    }
    errno = saved_errno; return result;
}
