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
| L1 | Resolve the known GPU/PMGR/PowerChannel topology from a relocatable heap snapshot | Offline verified |
| L2 | Read protected heap pages through an NVIDIA-owned internal RM/GSP context | Live verified in the v5/v6 in-tree patch |
| L3 | Read the same pages from a standalone companion module through exported RM operations | Rejected by RM (`NV_ERR_NOT_SUPPORTED`) |
| L4 | Run the resolver through the in-tree owner adapter and return one read-only snapshot | In progress |
| L5 | Build and apply the complete reviewed writer contract after every initialization | Not implemented |

L0 and L1 solve different problems. WPR2 LO and HI are lower and upper
protected-memory boundaries. They are not wattage fields and do not reveal the
location of power-policy objects.

## Layering

### Portable core

`core/resolver.c` receives a bounded reader and a versioned relative-layout
schema. It reconstructs the heap virtual-address base from self pointers,
checks the GPU to PMGR ownership chain, checks the policy array/group aliases,
and accepts exactly one PowerChannel candidate. It has no PCI IDs, absolute
GPU addresses, kernel APIs, or write primitive.

The current profile still contains relative member locations learned from GSP
615.71. It removes absolute object addresses but does not yet make the resolver
independent of future GSP object layouts. A future profile must be added from
semantic evidence and tested as a separate schema.

### Owner adapter

Protected heap transfer command `0x20800afa` is marked internal by RM. A normal
kernel RM client created through the exported `nvidia_get_rm_ops` bridge reaches
the command dispatcher with `bInternal == false` and is rejected with
`NV_ERR_NOT_SUPPORTED`. Root privilege does not change that property.

The working transport therefore has to execute inside the NVIDIA open module,
where the GPU's existing `hInternalClient` and `hInternalSubdevice` are owned.
The v5/v6 patch proved that transport. The next adapter will expose only a
fixed resolver/apply operation; it must not export arbitrary FB addresses,
sizes, commands, or data writes.

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

The Linux v5 result changed three validated ceiling fields to 225,000 mW. The
developer reference describes a wider 19-field production contract: seven
core fields, six Type07 Entry13 fields, and six Type07 Entry14 fields. The
Entry13/Entry14 fields may account for NVVDD/MSVDD policy behavior under load,
but that relationship has not yet been proved on Linux and those fields are not
written by this branch.

The final writer must satisfy all of these conditions:

1. Resolve exactly one coherent layout during the current owner epoch.
2. Match every protected stock/default witness for the selected profile.
3. Construct a fixed, distinct writer set inside the kernel.
4. Prove that no writer overlaps a protected field.
5. Compare each pre-state immediately before its write and read it back
   immediately afterward.
6. Stop the epoch after any mismatch, transport error, or ambiguous state.

User space may request a named operation and read results. It may not provide a
GPU address, object offset, target value, or arbitrary GSP command.

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

The BAR0 WPR2 probe is independent of the installed NVIDIA driver version for
Blackwell devices that use the GB100 HUBMMU register layout. The RM/GSP owner
adapter must be built with the matching NVIDIA open-module source and is an
internal-ABI integration. The power-object resolver is independent of absolute
addresses and the laptop model, but remains bound to explicitly reviewed object
schemas. These boundaries are intentional and should not be described as
universal driver-version support.
