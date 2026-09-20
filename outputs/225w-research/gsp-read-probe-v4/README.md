# GSP heap page reader v4 — live objects verified

The v3 hardware experiment successfully read 256 bytes at 0x3ef024000. v4
extends that transport to 4096-byte pages so we can locate power-policy objects.
It does not change power limits, invoke setters or write GPU memory.

Target remains only this MECHREVO RTX 5080 Laptop, PCI10de:2c19/subsystem1d05:6041,
NVIDIA615.71.09, CachyOS7.2.6-1-cachyos. No universal-hardware claim.

## Interface and limits

Private root-only command0x2080ff71, version4, ABI4200bytes. Inputs are operation0
(metadata), operation1 (one page), relative aligned pageOffset, and zero reserved
words. Page length is always4096; no absolute user-supplied source or destination.
The kernel checks exact observed WPR bounds and host boot-input sizes, then limits
offsets to [0,0x73dc000-4096]. Source is0x3ef024000+offset, cached FB; destination
is a newly allocated/mapped owned host page. Failed RPC retains that destination
and disables all further reads until reboot. Successful RPC frees it. Total
budget32768calls per module load. State assumes this single matching GPU.

As with v3, **do not hot-unload after a failed RPC**. Recovery is reboot into the
ordinary CachyOS entry. Nothing sends diagnostic commands automatically at boot.

The launcher verifies runtime module marker/srcversion and installed firmware
SHA256. A fresh startup NVLOG confirms the physical heap before metadata. Later
ranges reuse that evidence only in the same boot with matching module srcversion;
this avoids depending on startup records surviving in a circular log. The kernel
continues checking WPR bounds per call. Optimized Python (-O/PYTHONOPTIMIZE) is
explicitly rejected before setup so validation cannot disappear.

## Why this enables the next step

The common power-policy constructor at0x178a72c installs these method pointers:

| Object field | Pointer | Initializer AUIPC PC |
| --- | --- | --- |
| +0x2d0 |0x177f3c4 BoardSet |0x178aa46 |
| +0x2d8 |0x177c0f4 |0x178aa52 |
| +0x2e0 |0x177c250 getter |0x178aa5e |

All three PC-relative address calculations were checked against original ELF
instruction words. The offline scanner accepts the common signature or the verified type0 board
subclass (table0x4193460, overridden BoardSet0x17b2248), then reports raw
header/type/unit values and effective MAX (+0x108), CURRENT (+0x1f8). A lone
175000 value is not a match. A match is still only a **candidate**: a stale copy
or non-board policy can match, and derived objects can override methods and be
missed. Never use scanner output alone as a write target.

PMGR+0x1cc8 is a group pointer in Info handler0x1787ef0 (loads at0x1787f42/7f78).
GPU+0x2210 links to PMGR; PMGR+0x3d1c stores UPPER. These live links and the
VA-to-PA mapping have now been verified for the boot listed below. The scanner
alone is still insufficient: analyze_live.py independently checks ownership, all
15 populated policy entries, public GET values and targeted rereads.

## Validation

- The old v3 failed the new4096-byte DMA expectation before the implementation edit.
- 18 host lifecycle scenarios passed under ASan/UBSan, including two different
  successful pages, timeout retention, poisoned subsequent request, layout/identity/
  ABI/CC/virtualization rejection and the full32768-call budget boundary.
- Builder checks passed for first/last page, alignment, bounds, integer wraparound,
  destination validation, read direction and both ABI sizes.
- Mocked collector tests passed: metadata without DMA, ordered pages, stop at first
  failed RPC, no writing the failed page. Real LD_PRELOAD/ioctl page reads also succeeded in the current boot;
  nvidia-smi exit code alone is insufficient.
- Five scanner tests passed, including the board subclass, cross-page candidate
  and false-positive/truncated-input rejection. Synthetic tests alone are not a
  live PMGR discovery. verification.json describes the earlier four-test build.
- Independent review found the optimized-Python validation gap, now fixed; checked
  again that -O is refused. Artifact ownership is applied even on diagnostic failure.
- Clang/LLD build succeeds with existing objtool indirect-jump warnings. Full patch
  applies to pinned clean driver source. See installed.json for boot-image hashes.

## Runtime collection procedure

Select **CachyOS - NVIDIA GSP read probe v4** in Limine. Stock modules and ordinary
boot entry remain available. The previous v3 assets are retained separately.

The authorized operator runs as root from this directory:

1. `python run_live.py metadata`
2. Inspect successful metadata, then `python run_live.py read --pages 1`.
3. Inspect the first4096-byte read and GPU/kernel health before any larger range.
4. If successful, start with `python run_live.py read --offset 4096 --pages 1024`.
   This reads4MiB, sleeping5ms between calls. Maximum8192pages (32MiB) per invocation.
   Later ranges or targeted rereads must stay within the fixed heap and kernel budget.

The launcher saves exact byte counts, hashes, candidate results, power output,
kernel journal and NVLOG. Any RPC or partial-output failure stops the collection;
no automatic retry. Different page snapshots are not atomic as a whole. Artifacts
are local, owned by the workspace user, private files0600/directories0700.
Do not publish raw memory dumps. Nothing is published to the user's repository
before a confirmed power-unlock result.

## Verified live result — 2026-09-20

Boot20afa6a8-ed42-4d5e-ac15-7a54ca42a9eb, markerheap-pages-v4-20260920,
srcversionECF44A8A91D823FD90006D5. Metadata and all1034 page transfers succeeded,
with no retained buffer. Read first4096bytes, then4MiB, then9 targeted pages.
No Xid in the captured kernel journal. GPU remained responsive; maximum175W.
**No GPU-memory or power-policy writes were performed.**

Heap cached VA0x7f2000000 maps to PA0x3ef024000 in this boot. GPU self-pointer,
GPU→PMGR, PMGR self-pointer and PMGR→GPU links agree; all15 populated policy
array entries match public INFO. Targeted rereads confirmed the key fields.
These addresses are not guaranteed to survive reboot.

| Object | Heap offset | Virtual address | Physical address |
| --- | --- | --- | --- |
| GPU |0x196bb0 |0x7f2196bb0 |0x3ef1babb0 |
| PMGR |0x380690 |0x7f2380690 |0x3ef3a4690 |
| Board policy2 |0x3bc610 |0x7f23bc610 |0x3ef3e0610 |

| Field | Offset | Value | Physical address |
| --- | --- | --- | --- |
| Effective MAX |board+0x108 |175000mW |0x3ef3e0718 |
| FE source MAX |board+0x114 |175000mW |0x3ef3e0724 |
| UPPER |PMGR+0x3d1c |175000mW |0x3ef3a83ac |

Offline execution of the original INFO backend0x1787ef0 over this captured heap
completed12149instructions and reproduced all20512bytes of the actual public
A618 response, with zero differences. No functions were stubbed on this path.

Original derived BoardSet0x17b2248 was also executed offline:

- MAX/FD225000 adds a source but leaves effective MAX175000 because FE remains175000.
- MAX/FE225000 writes board+0x114 and+0x108 to225000 and marks+0x44 dirty.
- Calling MAX/FE175000 restores both values; only the setter's dirty flag remains
  different from the original captured heap. Default MAX at+0x10c is unchanged.

This establishes the CPU-side setter behavior for the captured configuration.
It does not prove live memory-write transport, PMU synchronization or225W draw.
UPPER is separate, and CURRENT/F7 was145000 in this capture. Raising MAX alone
cannot demonstrate a power unlock. Rail policies13/14 use a different unit tag;
their210000/60000 values must not be interpreted as watts or changed blindly.

Evidence: live-result.json, board-set-emulation.json, info-emulation.json and the
private live/ boot directory. Next: a bounded write/restore experiment with fresh
object discovery, expected-value checks, readback, explicit failure handling and
policy synchronization before any controlled load. No new reboot is needed for
further reads. No write-capable module has been prepared or installed yet.
