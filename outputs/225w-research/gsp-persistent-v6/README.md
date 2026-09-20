# v6: boot-scoped 225 W request and UCC Max TGP

Status: built, tested and installed locally; **not booted or verified at 225 W yet**. The last live v5 result remains a successful 175→225→175 W ceiling roundtrip with CURRENT at 145 W.

A separate `CachyOS - NVIDIA Max TGP 225W v6` entry contains the patched module and adds `codex.max_tgp=225`. The ordinary CachyOS entry and stock modules are preserved. An enabled oneshot runs only under that exact token, checks AC power, module/firmware identity, startup heap mapping and the running UCC daemon's forced external ownership. It journals one attempt per boot before accessing the GPU. It does not retry failed writes.

## Command sequence

1. Validate GPU/PMGR/board ownership and original 175 W ceilings.
2. Perform the v5 identity write and raise only FE MAX, effective MAX and UPPER to 225000 mW.
3. Validate original cTGP state, policy index 2 and lower bound 80000 mW.
4. Call NVIDIA's original internal `0x20800ad3` with offset 145000 mW, then `0x20800ad2` with enable=1.
5. Require readback CURRENT=225000 and public MAX=225000 before recording policy success. Real draw remains separately unverified.

Original RISC-V handler emulation prepares selector3/source F7=225000 mW. It stops before policy submission: this is not an emulated PMU or proof of electrical enforcement. No direct writes to CURRENT, dirty bytes, voltage, current rails or EEPROM were added.

The private root-only operation4 activates cTGP; operation5 releases it using offset0 then mode0. Explicit release requires CURRENT≤175000 before operation3 restores ceilings. If activation rejected before its first cTGP command, the verified operation5 no-op permits ceiling restoration. Indeterminate RPC/readback failure poisons further mutation. Two-phase release reserves256 DMA calls, including the existing128-call ceiling restore allowance.

## UCC

The companion patch against local UCC base `c337308` adds a global authenticated/persisted **Max TGP** checkbox. It serializes ownership changes with actual cTGP sysfs and NVML set/reset writers. The dedicated boot token forces checked/read-only Max TGP and disables the TGP slider, without overwriting the saved preference. Ordinary boots allow toggling the checkbox. Requested/enforced power are independently read from NVML; unsupported values show unavailable.

UCC code and its patch retain the upstream GPL-3.0 license. This patch is provided separately from the NVIDIA MIT-licensed driver modifications.

## Local operation and recovery

After manually booting the new entry, inspect `systemctl status mechrevo-max-tgp.service`, `/var/lib/mechrevo-max-tgp/<boot-id>/activate.json` and the fresh NVML enforced limit before benchmarking. Raw diagnostic files stay root-private. `/run/mechrevo-max-tgp/status.json` contains a small public status summary.

An explicit release can be requested with `sudo python /usr/local/lib/mechrevo-max-tgp/run_boot.py release`. It never blindly retries an uncertain mutation. A failed release requires ordinary stock boot recovery. This is not a general-purpose installer: archived manifests contain placeholders for the original workspace, kernel and build outputs.

The oneshot requests the policy on each selected boot; it does not continuously reapply it. Persistence through firmware overrides, GPU resets or suspend/reinitialization is not established. The stock module, ordinary boot entry and previous UCC package remain the local recovery path.

## Verification

- NVIDIA modules built with Clang/LLD; existing objtool warnings remain.
- Original cTGP generator prepared the expected F7 request offline.
- ASan/UBSan: pure cTGP sequence, all control/read fault positions, bad initial state, v5 engine regression, actual host bridge activation/release and both recovery edge cases.
- Client sequence, boot token/AC/UCC response validation and subprocess signal tests passed.
- UCC build and all18 tests passed, including fake-NVML actual writer suppression and independent read errors.
- Initramfs extracted and module/firmware hashes verified; stock hashes unchanged.
- Installed UCC live DBus reports real enforced power; ordinary boot skips the oneshot.

These checks do not replace the first v6 boot and load test.
