# Validation and evidence

The result has three layers: a reversible live ceiling change in v5, a 225 W CURRENT and NVML enforced-policy result in v6, and a successful FurMark run reported by the owner with a corresponding benchmark log. None of these substitutes for an independent electrical measurement.

## Evidence matrix

| Observation | What it establishes | What it does not establish |
| --- | --- | --- |
| v3: successful 256-byte GSP copy | Reachable RM heap read transport | Policy object identity or write access |
| v4: 1034 page transfers, validated ownership and exact INFO emulation match | Live board object discovery and public-response correspondence | Live writes or PMU enforcement |
| v5: identity + three increases + three restores | Seven successful four-byte writes; reversible MAX/FE/UPPER change | 225 W CURRENT or 225 W load consumption |
| v6: CURRENT=225000, MAX/FE/UPPER=225000 | Successful original cTGP request on the live boot | Sustained physical consumption |
| NVML: enforced limit 225 W, maximum 225 W | Driver-reported enforced policy | External electrical measurement |
| FurMark log and owner success report | Workload ran and exited normally; owner reports it worked | A before/after speed gain or sample-by-sample power trace |

## Fresh v6 boot

The recorded activation sequence on 2026-09-20 began with MAX, FE, UPPER and CURRENT at 175000 mW. Identity succeeded. After the three ceiling writes, MAX/FE/UPPER were 225000 while CURRENT was still 175000. Operation 4 then changed CURRENT to 225000 through the original cTGP commands. All recorded outer return, status and result values were zero; poison state remained clear.

The immediate NVML snapshot reported an enforced limit of 225.00 W and maximum of 225.00 W. Its draw reading was only 11.68 W at 36°C: an idle snapshot is not the load-test power result. See [sanitized first-boot evidence](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/blob/main/evidence/v6-first-boot.json).

## FurMark run

The owner reported success: “We did it. It works.” The corresponding FurMark log records:

| Field | Recorded value |
| --- | --- |
| Workload | OpenGL FurMark |
| Resolution | 7680 × 4320 |
| Frames | 812 |
| Duration | 38559 ms, approximately 38.6 seconds |
| Minimum / average / maximum FPS | 1 / 21 / 29 |
| Maximum GPU temperature | 59°C |
| Exit | Normal shutdown |

See [sanitized FurMark evidence](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/blob/main/evidence/furmark-first-run.json). The log contains no sampled power trace and no artifact-count result. The owner's successful-load-test report is retained separately from the numerical telemetry. No matching 175 W baseline benchmark or long-duration thermal validation is supplied, so the FPS values do not establish an improvement and the recorded temperature is not a steady-state thermal characterization.

## Regression and offline checks

The archived tests exercised malformed ABI and bounds requests, corrupted ownership and signatures, expected-value and readback faults, CURRENT guards, timeout retention, poisoned-state refusal, and reserved recovery budgets. v5 tests checked whole-fixture equality after restoration. Actual subprocess tests covered ordinary signal deferral; optimized Python was refused. Review found and corrected restore-budget/state-reporting and parent signal-handling defects before the live ceiling test.

v6 added tests of cTGP activation/release order, control/read fault positions, original-state validation, host bridge behavior, and release edge cases under ASan/UBSan. The UCC build and 18 tests passed, including suppression of actual competing writer paths with fake NVML and independent read failures. Module builds and extracted initramfs module/firmware identities were checked; pre-existing objtool warnings remained.

These tests establish behavior under their fixtures and modeled faults. The original RISC-V cTGP emulation stops before policy submission. Windows emulation models NVIDIA dependencies. Neither is a substitute for the recorded live result. Tests that require private captures need fresh local fixtures; excluded raw dumps are not silently replaced by public synthetic evidence.

## Reading historical files

The archived v5 records accurately say CURRENT stayed at 145 W and load power was untested at that stage. The original v6 installation checkpoint recorded `new_entry_booted=false`; it predates the fresh activation and FurMark run. Read the current evidence files above for the later result rather than interpreting an old checkpoint as a present failure.

Remaining gaps include sampled load power, independent electrical measurement, controlled 175 W versus 225 W comparisons, extended thermal stability, suspend/resume and GPU-reset behavior, and validation on other hardware. [[Porting-and-compatibility]] defines the present support boundary.
