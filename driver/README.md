# Kernel probes

These modules are research checkpoints for the unified resolver. They do not
change power limits.

## `unbound_wpr_probe`

This standalone module reads the Blackwell HUBMMU WPR2 LO and HI registers from
BAR0, exposes the raw register values and decoded half-open range as read-only
module parameters, and unloads normally. It does not read protected memory.

## `unbound_gsp_access_probe`

This module tests whether a separate kernel RM client can invoke NVIDIA's
internal GSP memory-transfer control through `nvidia_get_rm_ops`. Driver
615.71.09 rejected the request with `NV_ERR_NOT_SUPPORTED` because command
`0x20800afa` is internal and the exported bridge creates an external request.

If a transfer RPC returns an error, the module intentionally retains its DMA
destination and takes a self-reference until reboot. This avoids freeing a
buffer after an indeterminate asynchronous transfer. Use this probe only in a
disposable research boot.

## Build

The WPR probe needs only kernel headers. The GSP access probe also needs the
matching NVIDIA open-module source tree and its generated `Module.symvers`:

```bash
make -C driver \
  NVIDIA_SRC=/path/to/matching/open-gpu-kernel-modules \
  NVIDIA_VERSION="$(modinfo -F version nvidia)"
```

This directory is not a finished DKMS package. The successful protected-memory
transport currently lives in the in-tree v5/v6 NVIDIA patch. See
[`docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md) for the integration plan.
