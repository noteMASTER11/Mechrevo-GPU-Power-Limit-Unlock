# Porting and compatibility

This implementation was tested on one MECHREVO YAOSHI Series-X6AR55xY with an RTX 5080 Laptop GPU, PCI `10de:2c19`, subsystem `1d05:6041`, kernel `7.2.6-1-cachyos`, NVIDIA/GSP `615.71.09`, and VBIOS `98.03.5E.00.5C`. It is a version-specific implementation, not a universal RTX power-limit switch.

The driver source base is `61dcc93722ecb418bb5f2e00923f05b4b8051dd1`. The tested GSP firmware SHA256 is `c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b`. Matching a marketing GPU name is insufficient when these inputs differ.

## What is version-specific

| Dependency | Why it matters |
| --- | --- |
| GPU and subsystem IDs | The implementation explicitly restricts the target identity. |
| Firmware hash and driver source | Internal commands, object layouts and method signatures are private implementation details. |
| Host WPR metadata and startup heap records | Boot input is not automatically the runtime heap; wrong ranges were rejected in v1/v2. |
| VA-to-PA translation and object ownership | Object addresses can change across boots. Historical offsets are only hints. |
| Board policy index, units and initial bounds | The cTGP offset depends on a validated 80000 mW lower bound and original 175000 mW ceilings. |
| Kernel build and initramfs | A successfully built patch is useless if the selected boot loads another module or firmware. |
| Competing TGP controllers | Another writer can change policy state; disable software that controls TGP before activation. |

The same GPU on another vendor's board, another MECHREVO model, a different subsystem ID or a newer driver requires renewed validation. RTX 5090 compatibility has not been demonstrated. The implementation does not claim support for multiple GPUs, confidential-computing mode or virtualization; the relevant guards intentionally reject unsupported contexts.

## A port is a new research result

Begin with read-only public policy discovery and exact identity recording. Establish the runtime accessible heap, validate original-code mapping behavior, then prove a small successful read. Identify live GPU/PMGR/board ownership independently of scanner hits and compare it with public INFO. Determine units before assigning rail names or watts to policy values.

Only after reproducing those relationships should a port define narrow expected-value writes and a recovery state machine for its own layout. Reproduce fault and signal tests, verify the generated boot image, and retain stock recovery. A new target must establish reversible changes and its own cTGP request/readback evidence before claiming success.

Simply changing the PCI allowlist, firmware hash or three offsets to make preflight pass defeats the evidence that justified the original writes. Offline emulation of a different driver is useful, but not proof of live memory access, policy acceptance or physical power.

## Limits of the current result

The service requests 225 W once on each selected boot. Persistence through firmware interventions, suspend, GPU reset and reinitialization has not been established. AC attachment is checked, but PSU capacity was not established in the recorded baseline. The short FurMark run does not establish long-duration thermals or performance across workloads.

No voltage or rail-current modifications are included. Rail policies with another unit tag remain untouched. Expanding those controls is a separate research task, not a necessary step in the demonstrated board-limit path.

Source and evidence live in the [main repository](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock). See [[GPU-access-and-limit-discovery]] and [[Validation-and-evidence]] for the checks a new port would need to reproduce.
