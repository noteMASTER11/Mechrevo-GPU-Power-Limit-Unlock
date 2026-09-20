> **Historical research checkpoint.** Status statements below describe that stage. See the [current guide](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/blob/main/docs/INSTALL.md) and [successful-run evidence](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/blob/main/evidence/README.md) for the working v6 result.

# Research index and original baseline

The latest state is documented in [v5](gsp-write-probe-v5/README.md): live MAX/FE/UPPER writes225000→175000 succeeded. This index condenses earlier working notes in English.

Initial hardware: MECHREVO YAOSHI Series-X6AR55xY, RTX5080 Laptop10de:2c19/subsystem1d05:6041, BDF0000:02:00.0, BIOS N.1.13MRO16, VBIOS98.03.5E.00.5C. Kernel7.2.6-1-cachyos and NVIDIA/GSP615.71.09. Initial default80000mW, maximum175000mW; enforced145000–150000mW at idle. PSU capacity was not established.

Read-only RM requests reconstructed from NVML:

| Command | Bytes | Result |
| --- | ---: | --- |
| Policy INFO2080a618 |20512 |Success |
| Policy CONTROL2080a61a |13876 |Success |
| Policy STATUS2080a619 |397048 |Success |
| Client limit GET2080a61d, sourcesFD/FE |12 |NOT_SUPPORTED0x56 |

Policy2:type0,ID0,default80000,max175000. Policies13/14:type0x12,IDs0x1b/0x1c,default=max210000/60000, with a different unit tag. These are not identified as210W/60W. Their rail/current interpretation requires version-specific verification.

The initial VBIOS-in-RAM override caused artifacts/initialization trouble and was abandoned; the entry was removed and the stock driver restored. No EEPROM flash. The later research moved to actual GSP heap objects. Windows emulation, GSP disassembly, v1–v4 read transports and v5 reversible writes are archived here. UCC expansion and broad Tongfang/Uniwill support remain future work.
