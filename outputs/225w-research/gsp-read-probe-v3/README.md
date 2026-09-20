# GSP heap read probe v3 — hardware read succeeded

This diagnostic does **not** raise the GPU power limit. It attempts one 256-byte
FB → owned SYSMEM copy, with no GPU-memory write and no power-policy change.
Live result: **256 bytes read successfully**, `stage=3 rpc_status=0x0`, no retained
buffer, no kernel Xid in the captured boot journal, maximum remains **175 W**.
See `live-result.json` and its evidence directory. The data contains allocator-like
boundary words; it does not identify PMGR or prove a power unlock.

Target: this MECHREVO RTX 5080 Laptop (10de:2c19 / 1d05:6041), NVIDIA 615.71.09,
CachyOS 7.2.6-1-cachyos. No claim of portability to other hardware or firmware.

## Why v2 failed, and why this address differs

The live v2 NVLOG proves that the GSP transfer handler rejected physical address
`0x3ef020000`, the beginning of WPR2. Being inside WPR2 does not make an address
accessible through the GSP RM FB mapper.

The RM startup routine at `0x1afac1a` receives a 0x290-byte descriptor at VA
`0x4194810` through the LibOS call wrapper at `0x1b9fe18`. It logs region fields
before initializing the allocator from descriptor offsets `0xe8` (virtual base)
and `0xf8` (length), at `0x1afad56`. The physical range at offsets `0xd8` / `0xe0`
and that allocator length are present in two consistent copies in our live dump:

| Field | Observed value |
| --- | --- |
| Ordinary FB base / length | 0 / `0x3eed20000` |
| Region at descriptor +0xa8 / +0xb0 | `0x3eed20000` / `0x300000` |
| RM heap physical base / length | `0x3ef024000` / `0x73dc000` |
| RM allocator length | `0x73dc000` |
| RM heap end, exclusive | `0x3f6400000` |

The RM heap starts **16 KiB after WPR2 begins**. The FB mapping branch at
`0x1ba6186` accepts sources inside that region; cached translation uses descriptor
offset `0xd0`. v3 uses the cached mapping, avoiding assumptions about the separate
uncached alias. The root descriptor is populated at runtime, not from nonzero
static ELF contents. This explains why static-data inspection alone was insufficient.

`heap_evidence.py` decodes the exact NVLOG markers and their preceding argument
words. It rejects missing, conflicting or changed heap records. See
`heap-evidence-v2-boot.json` for the original record offsets and log SHA256.

`emulate_fb_translate.py` runs the original RISC-V translator at `0x1b7c694`
using observed physical ranges and **synthetic virtual bases**. It reproduces the
v2 address rejection (0x5b), accepts the v3 source and final valid 256 bytes, and
rejects a read crossing the heap end. This verifies range selection only:
it does not prove hardware permissions, a successful copy, or access to PMGR.

## Changes and checks

- Private command 0x2080ff71, ABI version 3, fixed 336-byte output.
- Fixed source `0x3ef024000`, fixed copy length 256. No user-supplied address,
  length or GPU write payload. Reported image/heap fields remain the original
  host boot-input diagnostics; they are not relabeled as actual heap coordinates.
- Exact PCI/subsystem gate, CC/virtualization refusal, WPR bounds and boot-input
  sizes checked before allocation. One attempt per module load.
- The explicit launcher verifies the firmware hash, module marker/srcversion and
  the **current boot's** startup NVLOG heap range before issuing the request.
- A failed RPC retains the single 4096-byte destination until reboot. No retry
  and no hot unloading this experimental driver after an attempted read.
- 12 host lifecycle/guard scenarios passed with ASan/UBSan. The changed-source
  assertion was first run against v2 and failed as expected, before the v3 edit.
- Request boundary/direction tests passed; decoder rejected missing, conflicting
  and changed records; four original-code translator emulation cases passed.
- Clang/LLD module build succeeded. The log still contains objtool indirect-jump
  warnings; successful compilation is not proof of runtime correctness.
- Full patch applies cleanly to the pinned clean source. The installer extracted
  the generated initramfs and verified module hashes, firmware hash and module
  dependencies. Installed stock modules remained unchanged.

## Boot and explicit probe

Select **CachyOS - NVIDIA GSP read probe v3** in Limine. This replaces the v2 menu
entry and uses separate files in `/boot/codex-gsp-read-v3`. The ordinary CachyOS
entry remains available. Nothing sends a probe automatically at boot.

After boot, the authorized operator runs `run_live.py metadata`, inspects its
result, then `run_live.py read`, each as root. Evidence is stored under
`live/<boot-id>/`. Successful read requires `stage=3 rpc_status=0x0 bytes=256`;
an outer command status of zero alone is insufficient. The first 256 bytes are
saved only after that successful response. The first successful hardware read occurred at 2026-09-20 21:40:45 +04:00;
see `live-result.json`.

Next after a successful read: identify live GPU/PMGR object pointers and their
virtual-to-physical mapping. Their location has **not** been established. No
MAX, UPPER, NVVDD or MSVDD change is part of v3, and the expected limit is 175 W.
