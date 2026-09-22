# Unified resolver architecture

This branch separates verified mechanisms from the parts that still require
research. The target is a boot-time power-policy resolver for NVIDIA open
kernel modules. It must discover live objects after every GPU initialization,
authorize exactly one coherent policy layout, and apply a fixed reviewed
profile without accepting addresses or target values from user space.

## Current capability ladder

| Level | Capability | Status |
|---|---|---|
| L0 | Discover the Blackwell WPR2 boundaries from HUBMMU BAR0 registers | Live verified |
| L1 | Resolve the board-power topology from a heap snapshot without configured addresses or member offsets | Offline verified on the captured heap and synthetic fixtures |
| L2 | Read protected heap pages through an NVIDIA-owned internal RM/GSP context | Live verified in the v5/v6 in-tree patch |
| L3 | Read the same pages from a standalone companion module through exported RM operations | Rejected by RM (`NV_ERR_NOT_SUPPORTED`) |
| L4 | Run the autonomous resolver through the in-tree owner adapter | Live verified by v8r4; the exact manually known topology was recovered without supplied addresses |
| L5 | Re-resolve and update the three ceiling members | Live verified by v8r4 with immediate readback at 250,000 mW |
| L6 | Submit a fixed base-TGP policy through the public PMGR control ABI | Blocked: v8r5 policy-2 SET halted the PMU and required a reboot |

L0 and L1 solve different problems. WPR2 LO and HI are lower and upper
protected-memory boundaries. They are not wattage fields and do not reveal the
location of power-policy objects.

## Layering

### Portable core

`core/semantic_resolver.c` receives a bounded reader and the current heap size.
Its production entry point receives no live addresses, member offsets, PCI IDs,
stock wattage, or driver-version profile. It discovers selector records and a
cTGP tuple by their values and relationships, discovers a 20-slot policy array
by shape, infers the heap VA base from its pointers, and finds the unique common
index member across active policy objects. It accepts exactly one coherent
board-policy topology.

`core/resolver.c` is retained as the earlier profile-based reference. It is no
longer the v8 production algorithm.

### Owner adapter

Protected heap transfer command `0x20800afa` is marked internal by RM. A normal
kernel RM client created through the exported `nvidia_get_rm_ops` bridge reaches
the command dispatcher with `bInternal == false` and is rejected with
`NV_ERR_NOT_SUPPORTED`. Root privilege does not change that property.

The working transport executes inside the NVIDIA open module,
where the GPU's existing `hInternalClient` and `hInternalSubdevice` are owned.
The v5/v6 patch proved that transport. The v8 adapter exposes only inspect,
arm, fixed-ceiling update, and fixed direct-TGP operations. User space supplies
an operation and a reviewed target in the 200--300 W guard range; it cannot
supply a protected-memory address, object offset, or arbitrary GSP command.

### Boot integration

The adapter must run only after GSP and its power objects are initialized. A
driver reload or GPU reinitialization creates a new owner epoch and invalidates
all cached locations. The resolver must run again before any apply attempt.

The experimental machine uses a dedicated Limine entry containing the patched
NVIDIA module. The stock CachyOS entry is the recovery path. A failed or
uncertain write poisons the current epoch and prevents further writes until the
next clean initialization. This project does not attempt live rollback after
an indeterminate GSP transfer.

## Writer scope

The protected-memory writer changes only the three ceiling members returned by
the current semantic resolution. The attempted final direct-TGP step used
NVIDIA's PMGR GET/SET control IDs. GET exposed entries 2, 13, and 14, but
treating that GET response layout as a valid SET request was disproved by
v8r5. A policy-2-only SET halted the PMU. No automatic SET is authorized until
the actual request layout and required preconditions are recovered.

The developer reference describes a wider 19-field runtime structure contract.
That layout remains useful for explaining the NVVDD/MSVDD hypothesis, but v8
does not assume that those 19 private members retain fixed offsets.

The v8 writer implements these conditions:

1. Resolve exactly one coherent layout during the current owner epoch.
2. Match every protected stock/default witness for the selected profile.
3. Construct a fixed, distinct writer set inside the kernel.
4. Prove that no writer overlaps a protected field.
5. Compare each pre-state immediately before its write and read it back
   immediately afterward.
6. Stop the epoch after any mismatch, transport error, or ambiguous state.

It re-runs the resolver before every operation and once more after updating the
three ceilings. Only the three addresses returned by the current resolution
may reach the four-byte write primitive. Failed DMA or failed readback poisons
the boot epoch.

User space may request a named operation, a target inside the driver's reviewed
guard range, and read results. It may not provide a GPU address, object offset,
writer destination, or arbitrary GSP command. The boot service fixes the target
at 250,000 mW.

## Meaning of the 203–207 W observation

NVML's `SW Power Cap` event says that software power scaling is reducing clocks
to remain within currently active power limits. It does not identify which
policy generated the constraint. During the bounded 4K 2xMSAA capture, the
known board-power status reached 207,699 mW, the observed Entry13-associated
value reached 122,344 of 210,000, and the Entry14-associated value reached
50,340 of 60,000. This makes the MSVDD/NVVDD hypothesis worth testing, but no
sample proved either value touching its limit. The next read-only adapter must
capture the exact policy objects and limiter state at a higher sampling rate
before the 19-field writer is enabled.

## Compatibility claim

The WPR2 boundary discovery is independent of the captured power-object
addresses for Blackwell devices using the GB100 HUBMMU layout. The autonomous
power-object resolver is independent of absolute addresses, private member
offsets, laptop model, and the earlier `615.71` layout profile.

The RM/GSP owner adapter must still be built with matching NVIDIA open-module
source and remains an internal-ABI integration. It currently assumes the
Blackwell WPR/ACR heap starts `0x4000` bytes into WPR2 and uses the current PMGR
GET/SET control layout. These remaining boundaries must be detected or
versioned before claiming universal support across driver releases or future
GPU architectures.
