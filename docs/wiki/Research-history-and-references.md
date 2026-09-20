# Research history and references

Each stage removed a specific uncertainty. Later success does not retroactively turn an earlier failed address probe, offline emulation, or ceiling-only test into a successful power-unlock experiment.

## Stages

| Stage | Result | Lesson |
| --- | --- | --- |
| VBIOS-in-RAM experiment | Artifacts/initialization trouble; abandoned and stock driver restored | A firmware override was not the successful path. No EEPROM flash occurred. |
| Windows reference emulation | Original Nvpwr.sys instructions executed against synthetic Windows/NVIDIA objects | Revealed the reference sequence, not real Linux GPU behavior. |
| v1 | Metadata GET failed with `0x25` before GSP copy | Host boot-input metadata did not contain the assumed runtime offsets. |
| v2 | WPR metadata succeeded; copy from WPR start rejected with `0x1f`, zero bytes | Reachable GSP handler does not mean every WPR address is mapped. |
| v3 | 256 bytes read successfully from the actual RM heap | Established live read transport, without identifying policy objects. |
| v4 | 4096-byte reader; 1034 transfers; GPU/PMGR/board ownership verified | Connected discovered live objects to all 20512 public INFO bytes. |
| v5 | Seven word writes: identity, three increases and three restores | Proved reversible 175→225→175 W ceilings; CURRENT stayed at 145 W. |
| v6 | Original AD3/AD2 cTGP request; CURRENT and NVML enforced limit reached 225 W | Established live request/enforcement readback beyond ceiling changes. |
| FurMark | Logged run ended normally; owner reported success | Confirms the reported workload result, without a sampled power trace. |

Browse [the research archive](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/tree/main/outputs/225w-research) for code, tests and sanitized checkpoint summaries. The archived preboot v6 check and earlier “not yet verified” notes describe earlier stages. The latest evidence is indexed in [[Validation-and-evidence]].

## Windows reference: useful, with modeled boundaries

The [RTX 5070 Ti laptop reference](https://github.com/LevinAi-arch/rtx-5070ti-laptop-160w-power-limit/tree/93f55e954e4dc3db1aefe5fb3bd4f0a1f8d283c3) supplied the original Nvpwr.sys experiment. The local harness ran its x86-64 code in Unicorn. It did not load Windows kernel code into Linux or run the GUI executable.

The modeled success/restore path validated inputs, used BoardSet with source FE, changed UPPER and cTGP-related state, and requested amount/eligibility. Windows allocation/device APIs and NVIDIA objects/functions were modeled. Therefore the trace distinguishes actual SYS instructions from synthetic dependency effects. The synthetic “5080” profile identifier was not a PCI ID, and the separate 250 W XMG patch was outside this experiment.

See the [Windows emulator and scope](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/tree/main/outputs/225w-research/windows-emulation). The binary is not redistributed; reproduction requires a separately obtained hash-matching input.

## Rail notes and external tools

[mVolt](https://github.com/b00nz/mVolt) informed the distinction between rail overcurrent limits measured in amps and board power caps measured in watts. The reviewed public tree contained documentation/assets. It did not provide evidence that the normal board-watt maximum could simply be exceeded through an ordinary setter. Rail OCP and the demonstrated MAX/UPPER/cTGP path should not be conflated.

[nvidia-tools](https://github.com/Loong0x00/nvidia-tools/tree/b5da62379b6f7f9d33270e7b5c9e8cd79e78e1ef) supplied Linux NvAPI examples. On the local machine, power-topology INFO `0xc12eb19e` and STATUS `0xf40238ef` returned zero through `libnvidia-api.so`, but the STATUS parser's interpretation was invalid: DWORD +4 was `0x501` (1281), exceeding the possible channel count in its 9432-byte buffer. A successful call was not taken as proof that desktop rail labels fit the returned layout. No I2C or tuning command was run in that check.

This is why policy 13/14 values 210000/60000 remain unlabeled as watts and unchanged. Identifying NVVDD/MSVDD or current rails requires independent unit and structure validation on this driver.

The original local v6 deployment also used a UCC patch against base `c337308` to serialize TGP writers and verify external ownership. That integration belongs to the historical tested setup. UCC installation or a particular checkbox is not required by the portable workflow; disable any software that controls TGP before activation.

## Source projects and licensing

- [NVIDIA open GPU kernel modules](https://github.com/NVIDIA/open-gpu-kernel-modules): host driver source used for the bounded bridge; the project records its pinned commit and firmware hash.
- [Uniwill laptop driver / UCC project](https://github.com/nanomatters/uniwill-laptop-driver): companion platform/control work referenced by the local UCC integration.
- [v6 implementation and UCC patch](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/tree/main/outputs/225w-research/gsp-persistent-v6): boot request, ownership patch, tests and emulation.

The UCC patch retains its upstream GPL-3.0 license and is separate from the MIT-licensed NVIDIA driver modifications. Firmware, Windows executables, raw GPU heaps, NVLOG archives, credentials and generated modules/initramfs images are excluded from publication.

The wiki's canonical editable source is [docs/wiki](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/tree/main/docs/wiki). Operational commands belong in [INSTALL.md](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/blob/main/docs/INSTALL.md); this history explains the evidence and is not an alternate installer.
