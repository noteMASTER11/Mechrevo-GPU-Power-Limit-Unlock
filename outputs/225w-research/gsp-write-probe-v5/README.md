# GSP bounded write probe v5 — live write and restore verified

2026-09-20. User authorized an explicit identity/write/readback/restore experiment.
Target: this MECHREVO RTX5080 Laptop10de:2c19/1d05:6041, kernel7.2.6-1-cachyos,
NVIDIA/GSP615.71.09. Live experiment completed: three ceilings temporarily225W and restored175W.
Seven successful four-byte GPU-memory writes including identity and restoration.
**Actual225W consumption under load has not been tested.**

## Boot and recovery

Select **CachyOS - NVIDIA GSP bounded write probe v5** in Limine.
Marker: `power-words-v5-20260920`; srcversion: `56A0D8849546D4DA6E66C72`.
The entry has no automatic power commands. Ordinary CachyOS remains the recovery
entry; stock modules were hash-verified unchanged. Previous v4 assets remain on
boot storage, though the managed diagnostic menu entry now points to v5.
Do not hot-unload this module after a failed RPC. Reboot clears the experiment.

## Explicit operator sequence after boot

From this directory, as root:

1. `python run_live.py inspect` — fresh startup heap evidence and read-only object
   validation. Defaults are address hints from v4, not trusted persistent addresses.
2. Inspect its evidence. If successful, `python run_live.py exercise` — identity
   write175000 to FE MAX; verify; temporarily change three ceilings to225000;
   query public INFO; restore175000; query public INFO again. No load is started.
3. Inspect the log, public INFO result, restored fields, kernel journal and GPU.

`exercise` may be attempted only once per boot by the launcher. The kernel allows
one apply after a successful identity write. `restore` is an explicit recovery
operation for an interrupted but non-poisoned completed apply, not an automatic
retry after failed DMA. If offsets changed, use the preserved v4 read ABI through
`run_pages.py` and `query_pages.so`, rediscover owners, then provide the three
`--gpu-offset`, `--pmgr-offset`, `--board-offset` hints. Never guess write addresses.

## Exactly what can change

Private root-only command0x2080ff72, ABI88bytes/version1:
operation0 inspect,1 identity,2 apply225000,3 restore175000. Input consists of three
relative object offsets; no caller-supplied value or raw destination. Kernel checks
object bounds/disjointness, GPU and PMGR self/back links, policy-array ownership,
board subclass/method signatures, source FE, MIN arbitration and original default.
Shared read/write state binds the first matching GPU; no multi-GPU support claim.

Only three aligned4-byte words are writable:
- board+0x114: FE MAX
- board+0x108: effective MAX
- PMGR+0x3d1c: UPPER

All must start175000; apply expects175000 and writes225000; restore expects225000
and writes175000. CURRENT must be<=175000 for any write operation. Every individual
write has expected-old-value validation and immediate readback. It never writes
CURRENT, rail limits, voltages, executable code or firmware EEPROM.

The original BoardSet writes a single dirty byte at board+0x44. This transport's
firmware copy path requires aligned sizes divisible by4. v5 deliberately leaves
that byte and its neighbors untouched. Standard policy synchronization is still
unproven; this is a **ceiling/transport experiment, not a225W power unlock**.
Even public INFO showing225W is not evidence of225W physical draw.

## Failure behavior

The existing fixed-heap4096-byte reader and writer share a32768-RPC budget and
poison state. Ordinary reads reserve128 calls while the experiment is armed;
restore admits the reserved budget. A failed RPC retains its owned source or
 destination page until reboot and prevents further experimental DMA. A partial
or mismatched update poisons future writes rather than guessing an undo sequence.
State is reported even when admission fails. Do not interpret a failed operation
as a confirmed restoration; inspect active/poisoned/result and use normal reboot.

Both launcher and child defer SIGINT/SIGTERM/SIGHUP through the apply/restore
transaction and evidence persistence. SIGKILL, process crashes and power loss
are outside this software guarantee. No timer/unload/reset hook applies settings.
Each operation writes an exclusive attempt record before sending requests. Dumps
and logs stay local; no GitHub publication before an actual confirmed unlock.

## Verification

`verification.json` records fresh ASan/UBSan engine, kernel-bridge and client tests,
the inherited18-case read lifecycle/builder/client suite, five scanner tests,
real subprocess Ctrl-C regression, optimized-Python refusal and clean-base patch
application. Engine checks exact whole-heap equality after restoration,22 object
corruptions,6 ABI/bounds cases,14 write/readback faults and CURRENT guard.
Bridge tests include read/write/readback timeout retention, reserved budget and
truthful state on admission failure. Tests use captured bytes plus simulated RPCs;
they do not establish that the GPU accepts SYS→FB writes into its live heap.

Independent review found the restore-budget/state-reporting gap and parent
KeyboardInterrupt killing the child. Both were reproduced, fixed and retested.
Clang/LLD module build and extracted-initramfs hashes/dependencies passed; existing
objtool warnings remain. See installed.json and dependency-check.log.

## New reference checks

- https://github.com/b00nz/mVolt — public tree contains documentation/assets.
  Its guide distinguishes rail OCP (amps) from board watt caps; watt cap cannot
  exceed the ordinary driver maximum. OCP unlock stays within reported bounds.
- https://github.com/Loong0x00/nvidia-tools at
  b5da62379b6f7f9d33270e7b5c9e8cd79e78e1ef — Linux NvAPI examples. Locally both
  power-topology INFO0xc12eb19e and STATUS0xf40238ef returned0 via libnvidia-api.so.
  Its STATUS parser is not valid on this machine: DWORD+4 was0x501 (1281), beyond
  the9432-byte buffer's possible channel count. We saved raw bytes and did not
  attach its desktop rail labels. No I2C or tuning command was run.
  Evidence: loong-nvapi-read.json. These references do not replace MAX/UPPER work.


## Hardware result — 2026-09-20 22:46

Boot1a177a25-e406-426e-93eb-1e4bd16a1995. Inspect verified all owners/signatures
and original MAX/FE/UPPER175000. The explicit exercise succeeded:
identity FE175000 → FE/MAX/UPPER225000 → public INFO225000 →
FE/MAX/UPPER175000 → public INFO175000. Seven successful4-byte writes,
no poisoned transport and no Xid in the captured kernel journal. GPU responds
normally after restoration. CURRENT stayed145000 during the write cycle.

This proves live GSP heap SYS→FB writes and reversible ceiling changes with
stock firmware. It does not yet prove PMU synchronization or real225W draw.
The one-apply-per-boot guard is now consumed. Do not run exercise again or remove
its journal to retry; further work must respect the kernel guard. No publication
to GitHub yet. Machine-readable evidence: live-result.json and the live/boot folder.
