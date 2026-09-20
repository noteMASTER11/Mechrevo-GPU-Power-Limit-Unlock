# Historical check: enabling GSP does not grant arbitrary access

GSP615.71.09 was already active on the stock Blackwell installation. The faulty VBIOS identity entry was removed; ordinary boot remained available.

EnableGpuFirmware selects firmware use. EnableGpuFirmwareLogs controls logs. EnableDebuggerInterface permits the documented debugger session class, not arbitrary MAX/UPPER writes. Blackwell kgspIsDebugModeEnabled_GB100 consults NV_FUSE_ZB_OPT_SECURE_GSP_DEBUG_DIS; the fuse was not read in this check. Spoofing a host-side predicate would not establish hardware debug access.

Source references: kernel-open/nvidia/nv-reg.h; src/nvidia/src/kernel/gpu/gsp/arch/blackwell/kernel_gsp_gb100.c; generated g_kernel_gsp_nvoc.c. A separate entry that merely enabled GSP would duplicate the existing mode. Later v3–v5 experiments used a concrete internal memory-transfer handler instead.
