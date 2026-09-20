# Mechrevo GPU Power Limit Unlock — Linux research

**Verified: live GSP MAX/FE/UPPER ceilings changed from 175 W to 225 W and restored to 175 W. Actual 225 W consumption under load is not verified yet.**

On the tested MECHREVO RTX 5080 Laptop, NVIDIA public GET confirmed225000mW and then175000mW after restoration. CURRENT remained145000mW. The ceiling write is verified; a physical-power unlock remains unverified.

## Tested configuration

- MECHREVO YAOSHI Series-X6AR55xY, RTX 5080 Laptop (10de:2c19, subsystem1d05:6041).
- CachyOS7.2.6-1-cachyos, NVIDIA open module/GSP615.71.09, VBIOS98.03.5E.00.5C.
- Driver source base: NVIDIA/open-gpu-kernel-modules commit61dcc93722ecb418bb5f2e00923f05b4b8051dd1.
- GSP firmware SHA256:c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b.

## Start here

- [v5 live result](outputs/225w-research/gsp-write-probe-v5/live-result.json)
- [v5 protocol, validation and recovery](outputs/225w-research/gsp-write-probe-v5/README.md)
- [Complete driver patch](outputs/225w-research/gsp-write-probe-v5/gsp-write-probe-v5.patch)
- [v4 object discovery](outputs/225w-research/gsp-read-probe-v4/README.md)
- [Next experiment and research history](outputs/225w-research/next-experiment.md)
- [Windows reference emulation](outputs/225w-research/windows-emulation/README.md)

## What is in this checkpoint

Local experimental driver patches/helpers, C/Python tests, collectors, offline emulators, static-analysis notes, and sanitized result summaries. Directory names are retained to preserve research context. Historical v1–v4 and VBIOS records are explicitly historical; the VBIOS override caused artifacts and was abandoned. No EEPROM was flashed.

Raw GPU heap dumps, NVLOG archives, machine journals, credentials, firmware/Windows executables, generated modules and initramfs images are intentionally excluded. Scripts requiring private dumps will need freshly captured fixtures. Absolute workspace paths are replaced by `/path/to/research-workspace`; archived deployment manifests are not portable installers. There is no turnkey installation command in this checkpoint.

## v5 boundaries

The root-only probe validates object ownership, subclass signatures, original values and exact hardware/firmware layout. It permits three aligned4-byte writes only (FE MAX, effective MAX, UPPER), a fixed225000 target, expected-value/readback checks and restore175000. Identity must succeed first; only one apply per boot is permitted. On uncertain RPC failure the DMA page is retained and further experimental DMA stops until reboot. Ordinary termination signals are deferred through the launcher’s write/restore transaction.

CURRENT, voltages, rail limits and the dirty byte are not written by v5. Raising the reported MAX does not prove PMU enforcement or physical power. Do not remove the guards or reuse these offsets blindly on another machine/version. A failed or partial operation is not a confirmed rollback; the ordinary stock boot entry remains the recovery path.

## Evidence

Seven GPU word writes succeeded: one identity write, three increases, three restorations. Public INFO reported225000 then175000. No Xid appeared in the captured kernel journal. Tests cover whole-fixture restoration, malformed ownership/ABI inputs, write/readback faults, reserved rollback budget, and real subprocess interruption behavior. Review-discovered budget/state-reporting and signal-handling defects were fixed before the live test.

## References

- https://github.com/LevinAi-arch/rtx-5070ti-laptop-160w-power-limit
- https://github.com/nanomatters/uniwill-laptop-driver
- https://github.com/b00nz/mVolt
- https://github.com/Loong0x00/nvidia-tools
- https://github.com/NVIDIA/open-gpu-kernel-modules

This checkpoint was published at the owner's explicit request after the reversible ceiling-write result. Earlier notes saying publication was deferred describe the previous research stage.
