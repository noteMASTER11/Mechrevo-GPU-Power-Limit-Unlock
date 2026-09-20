# MECHREVO RTX 5080 Laptop: 225 W on Linux

The tested MECHREVO RTX 5080 Laptop successfully reached a 225 W enforced power policy on Linux. Fresh v6 boot readback showed MAX, FE MAX, UPPER and CURRENT at 225000 mW, with NVML reporting an enforced limit of 225 W. The owner subsequently reported that the FurMark test worked: “We did it. It works.”

These are two distinct pieces of evidence: recorded software policy readback and a user-reported successful load test. A corresponding 7680 × 4320 FurMark log records 812 frames over approximately 38.6 seconds, average 21 FPS, maximum GPU temperature 59°C, and normal shutdown. It contains no sampled power trace or artifact-count result. This wiki does not claim a measured performance gain or independently measured 225 W consumption.

## Start here

- Follow the repository's [installation and operation guide](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/blob/main/docs/INSTALL.md) for the runnable deployment sequence.
- Read [[How-It-Works]] for the host RMAPI, GSP heap and cTGP chain.
- Read [[GPU-access-and-limit-discovery]] for how live objects and limits were identified.
- Read [[Validation-and-evidence]] for what each experiment actually establishes.
- Read [[Recovery-and-Troubleshooting]] before operating the experimental boot entry.
- Read [[Porting-and-compatibility]] before adapting anything to a different machine or driver.
- Read [[Research-history-and-references]] for v1–v6 and the abandoned approaches.

## Tested configuration

| Component | Recorded identity |
| --- | --- |
| Laptop | MECHREVO YAOSHI Series-X6AR55xY |
| GPU | NVIDIA GeForce RTX 5080 Laptop GPU |
| PCI / subsystem | `10de:2c19` / `1d05:6041` |
| Kernel | `7.2.6-1-cachyos` |
| NVIDIA open module and GSP | `615.71.09` |
| VBIOS | `98.03.5E.00.5C` |
| BIOS | `N.1.13MRO16` |
| NVIDIA source commit | `61dcc93722ecb418bb5f2e00923f05b4b8051dd1` |
| GSP firmware SHA256 | `c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b` |

The ordinary 175 W ceiling was changed in live GSP policy objects. NVIDIA's original internal cTGP commands then requested CURRENT at 225 W. The selected v6 boot uses a dedicated module/initramfs. Software that controls TGP must be disabled to avoid competing policy writes. No EEPROM was flashed; the successful path does not depend on a modified VBIOS.

The implementation is specific to the tested identities and layouts. Similar branding, another RTX 5080 Laptop, or an RTX 5090 does not establish compatibility. The service makes one attempt per selected boot; persistence through suspend, reset, firmware override or reinitialization is not established.

## Documentation and source

The canonical wiki source is [docs/wiki](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/tree/main/docs/wiki) in the main repository. Historical research files retain statements accurate at their original checkpoint. For example, v5 demonstrated reversible ceiling writes while CURRENT stayed at 145 W; that is not the later v6 result.

Published evidence is sanitized. Raw GPU memory, NVLOG archives, private journals, credentials, firmware binaries and generated boot images are not part of this documentation.
