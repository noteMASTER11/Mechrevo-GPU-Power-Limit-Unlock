# How the 225 W request works

The successful path combines a bounded change to three live GSP ceilings with NVIDIA's original cTGP policy commands. Raising a maximum alone was insufficient: v5 showed MAX at 225 W while CURRENT remained at 145 W. v6 adds the missing request step and verifies CURRENT and NVML enforcement.

```text
Root launcher → private host RMAPI bridge
              → GSP transfer using owned DMA pages
              → validated GPU → PMGR → board policy objects
              → FE MAX + effective MAX + UPPER = 225000 mW
              → original AD3(offset = 145000 mW)
              → original AD2(enable = 1)
              → generated source F7 / CURRENT = 225000 mW
              → public policy + NVML enforced-limit readback
```

## Host and GSP boundary

The open NVIDIA host module is patched to expose root-only, narrowly defined diagnostic operations. It uses the internal GSP RMAPI transfer path (`0x20800afa`), with host memory explicitly allocated and mapped for the transaction. The bridge does not accept an arbitrary user pointer as a DMA destination, an unrestricted GPU address, or a caller-selected tuning value.

The read interface uses relative offsets into the verified RM heap. It reads into an owned host page. The write interface uses an owned source page and permits only aligned four-byte values at the three validated fields. A failed RPC may leave GSP still able to access that page: it is retained until reboot and subsequent experimental DMA is refused. See [[Recovery-and-Troubleshooting]].

## Object validation comes before mutation

A number matching 175000 is not a power-policy object. The implementation checks object bounds and disjointness, GPU and PMGR self/back links, the policy group's ownership, board subclass and method signatures, source FE, MIN arbitration, and original defaults. It binds to the first matching GPU; this is not a general multi-GPU facility.

The v5 identity operation writes the original value back to FE MAX and checks readback before permitting one apply. Each later word change checks the expected old value and its immediate readback. At activation, all ceilings must begin at 175000 mW and CURRENT must be no greater than 175000 mW.

| Validated field | Offset in this firmware layout | Applied value |
| --- | --- | ---: |
| FE source MAX | board + `0x114` | 225000 mW |
| Effective MAX | board + `0x108` | 225000 mW |
| UPPER | PMGR + `0x3d1c` | 225000 mW |

These are version-specific field offsets, not a portable address recipe. Fresh object ownership and mapping evidence are required on each applicable boot.

## Why FE and UPPER both matter

Offline execution of the original BoardSet handler showed that adding MAX under source FD leaves the effective maximum at 175 W because FE still limits it. Updating FE changes the FE source and effective MAX. UPPER is a separate bound, so both board fields and UPPER are included in the bounded change.

The original setter also marks a dirty byte. The experiment deliberately does not copy that byte or its neighbors: the transfer mechanism requires aligned sizes divisible by four. v5 therefore established ceiling manipulation, without claiming full policy enforcement from that manipulation alone.

## The v6 cTGP request

After the ceiling change, v6 validates cTGP support, original configuration, board policy index 2 and its 80000 mW lower bound. It invokes the original internal `0x20800ad3` (CONFIGURE_TURBO_V2) with an offset of `225000 − 80000 = 145000` mW, followed by `0x20800ad2` (CONFIGURE_TGP_MODE) with enable set to 1.

The 145000 value here is an offset from the validated 80 W lower bound. It is not the earlier observed 145 W CURRENT value. Original RISC-V handler emulation prepares selector 3/source F7 at 225000 mW. The emulator stops before policy submission; actual boot readback supplies the live evidence that CURRENT became 225000.

v6 requires CURRENT=225000 and public MAX=225000 before recording policy success. NVML enforced-limit readback is checked separately. It never directly writes CURRENT, rail limits, voltage values, executable firmware, or EEPROM.

## Boot scope and competing controllers

The dedicated boot token `codex.max_tgp=225` selects the oneshot path. Its preflight checks include AC power, runtime module and firmware identity, and startup heap evidence. An exclusive attempt record is written before GPU access; a failed mutation is not automatically retried.

Disable software that controls TGP before activation so another controller does not overwrite the requested state. The service makes one request per selected boot and does not continuously fight competing writers. UCC is not a prerequisite for the portable workflow; the original local installation's integration is historical context in [[Research-history-and-references]].

For source, see the [v6 patch and helper directory](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/tree/main/outputs/225w-research/gsp-persistent-v6). For commands, use [INSTALL.md](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/blob/main/docs/INSTALL.md).
