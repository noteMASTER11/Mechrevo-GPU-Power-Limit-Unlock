/* Disposable read-only observer: forwards ioctls unchanged; only logs RM control. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
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
    static int control_queried;
    if (observed && !control_queried && !result && before.cmd == 0x2080a618 && before.size == 0x5020) {
        rm_control info; memcpy(&info, arg, sizeof(info));
        if (!info.status) {
            control_queried = 1;
            /* GET control and its 0x3634-byte layout observed in NVML 615.71.09
             * at 0x103ac0..0x103c1f. This is not SET (0x2080e61b). */
            unsigned char control[0x3634] = {0};
            uint32_t mask = (1u << 2) | (1u << 13) | (1u << 14);
            memcpy(control + 0x10, &mask, sizeof(mask));
            rm_control q = before; q.cmd = 0x2080a61a; q.size = sizeof(control);
            q.params = (uintptr_t)control; q.status = 0;
            int qr = real_ioctl(fd, req, &q);
            fprintf(stderr, "PROBE_GET cmd=%08x size=%u mask=%x status=%x rc=%d data=", q.cmd, q.size, mask, q.status, qr);
            if (!qr && !q.status) for (unsigned i=0; i<sizeof(control); i++) fprintf(stderr, "%02x", control[i]);
            fputc('\n', stderr);
            /* GET status: command, size, mask offset read from NVML at 0x105420. */
            unsigned char *status_data = calloc(1, 0x60ef8);
            if (status_data) {
                memcpy(status_data + 4, &mask, 4);
                q = before; q.cmd = 0x2080a619; q.size = 0x60ef8;
                q.params = (uintptr_t)status_data; q.status = 0;
                qr = real_ioctl(fd, req, &q);
                fprintf(stderr, "PROBE_STATUS cmd=%08x size=%u mask=%x status=%x rc=%d\n", q.cmd, q.size, mask, q.status, qr);
                if (!qr && !q.status) {
                    FILE *out = fopen("work/linux-probe/pmgr-status.bin", "wb");
                    if (out) { fwrite(status_data, 1, q.size, out); fclose(out); }
                }
                free(status_data);
            }
        }
    }
    errno = saved_errno; return result;
}
