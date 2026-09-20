/* SPDX-License-Identifier: MIT
 * Local experiment, ABI fixed to NVIDIA 615.71.09. No policy writes. */
#ifndef CODEX_GSP_READ_PROBE_H
#define CODEX_GSP_READ_PROBE_H
#define GSP_READ_PROBE_CMD 0x2080ff71U
#define GSP_READ_PROBE_VERSION 1U
typedef struct {
 NvU32 version, operation, reserved[2]; /* Only these fields are input. */
 NvU32 stage, rpcStatus, bytesRead, retainedBuffer;
 NvU64 wprStart, wprEnd, image, imageSize, heap, heapSize;
 NvU8 data[64];
} GSP_READ_PROBE_PARAMS;
static int gspReadProbeBuild(NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS *p,
 NvU32 pci, NvU32 sub, NvU64 start, NvU64 end, NvU64 image, NvU64 imageSize, NvU64 dest) {
 if (pci != 0x2c1910deU || sub != 0x60411d05U || !start || end <= start ||
     image < start || image >= end || imageSize < 64 || imageSize > end-image ||
     !dest || (dest & 4095) || dest > (~(NvU64)0)-4095) return 0;
 *p = (NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS){0};
 p->src.baseAddr=image; p->src.size=64; p->src.aperture=2; p->src.cpuCacheAttrib=1;
 p->dst.baseAddr=dest; p->dst.size=4096; p->dst.aperture=1; p->dst.cpuCacheAttrib=0;
 p->transferSize=64; p->memop=NV2080_CTRL_MEMMGR_MEMORY_OP_MEMCPY;
 return 1;
}
#endif
