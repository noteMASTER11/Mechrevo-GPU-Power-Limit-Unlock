# Mechrevo GPU Power Limit Unlock for Linux

**225 W GPU power-limit operation is working on the tested MECHREVO RTX 5080 Laptop.** The v6 driver reports CURRENT, MAX and UPPER at 225,000 mW; NVIDIA NVML subsequently reports an enforced power limit of 225 W. The owner confirmed successful operation in FurMark.

This project documents how to access the GPU's GSP-managed power policies from Linux, inspect their limits, raise the validated board-power ceilings, and submit a new operating-power request through NVIDIA's own firmware handlers. It uses NVIDIA's open kernel module with the original GSP firmware. No hardware shunt modification or EEPROM flashing is involved.

**Start with the [step-by-step installation and operation guide](docs/INSTALL.md).** Read the [compatibility requirements](docs/COMPATIBILITY.md) before building. The verified v6 path remains tied to one hardware/firmware layout. The experimental v8 resolver has now been validated for read-only discovery and ceiling resolution, but its automatic writer is disabled after the v8r5 base-policy SET halted the GPU PMU.

## Experimental semantic resolver (v8)

The v8 resolver receives only a bounded reader for the current GSP heap. It no
longer accepts a heap virtual address, GPU/PMGR/PowerChannel addresses, member
offsets, a stock wattage, or a driver-version layout profile from user space.
It finds a unique topology from policy-array shape, common object identity,
board-selector semantics, and the cTGP lower/upper tuple. Ambiguous or damaged
snapshots fail closed.

The same portable C core is compiled both by the offline tests and by the
in-tree NVIDIA owner adapter. Live v8r4 evidence confirms that the resolver can
relocate the policy topology and identify the three ceiling writers without
stored addresses. The later v8r5 attempt showed that a successful policy GET
does not establish that the corresponding SET buffer is valid. Its
policy-2-only SET returned `NV_ERR_RESET_REQUIRED`, halted the PMU and caused
the graphical session to lose the GPU. That SET path has been reverted and the
installed v8 boot entry is inspection-only while its ABI is investigated.

This removes dependency on the previously captured live addresses and private
object member offsets. It does not yet remove every compatibility boundary:
the patch is still compiled into matching NVIDIA open-module source, uses the
Blackwell WPR/ACR heap prefix, and calls the current internal PMGR control ABI.
See [the v8 validation guide](docs/SEMANTIC-V8.md), [failure record](evidence/2026-09-22-v8r5-pmu-halt.md), and [architecture](docs/ARCHITECTURE.md).

## Read in this order

1. [Compatibility and prerequisites](docs/COMPATIBILITY.md): identify the GPU, driver and kernel; understand what is tested.
2. [Installation and operation](docs/INSTALL.md): inspect stock limits, build the software, disable competing TGP controllers, prepare a separate boot entry, activate and verify.
3. [Tool reference](docs/TOOLS.md): source components, commands and tests.
4. [Unified resolver architecture](docs/ARCHITECTURE.md): separation of WPR discovery, protected-memory transport, semantic resolution and writer authorization.
5. [Semantic v8 validation](docs/SEMANTIC-V8.md): the address-free resolver, isolated boot transaction and remaining compatibility boundaries.
6. [Wiki](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/wiki): GPU access, GSP policies, the exact mechanism, research history, porting and recovery.
7. [Evidence](evidence/README.md): what the first successful run demonstrated and what was not measured.

## What changed

| Value | Stock / before activation | Working v6 readback |
|---|---:|---:|
| Board policy effective MAX | 175 W | 225 W |
| Board policy MAX source FE | 175 W | 225 W |
| Platform UPPER | 175 W | 225 W |
| Board policy CURRENT | Up to 175 W | 225 W |
| NVML enforced power limit | 145–175 W, depending on state | 225 W |

Raising MAX alone was insufficient. The successful sequence also invokes the internal cTGP offset handler (`0x20800ad3`) and cTGP mode handler (`0x20800ad2`), which generate the CURRENT/source F7 request. With UPPER = 225,000 mW and LOWER = 80,000 mW, the fixed offset is 145,000 mW. The Wiki explains why this is an **offset**, not a 145 W target or a rail-current setting.

Before applying the unlock, disable any software or service that controls GPU TGP/power limits. A competing controller can overwrite the requested limit. No particular control-center application is required.

## Tested machine

| Component | Verified configuration |
|---|---|
| Laptop | MECHREVO YAOSHI Series-X6AR55xY |
| GPU | NVIDIA GeForce RTX 5080 Laptop GPU |
| PCI ID / subsystem | `10de:2c19` / `1d05:6041` |
| Distribution / kernel | CachyOS / `7.2.6-1-cachyos` |
| NVIDIA open driver and GSP | `615.71.09` |
| VBIOS | `98.03.5E.00.5C` |
| Bootloader | Limine |
| Cooling during owner test | Owner's water-cooled laptop setup |

The first recorded FurMark run used OpenGL at 7680×4320 for 38.559 seconds and logged a maximum GPU temperature of 59 °C. It contains no sampled power trace. The 225 W policy readback, owner-reported load success and FurMark statistics are documented separately; this short run is not a long-term stability qualification.

## Repository layout

- `scripts/`, `src/`, `patches/`, `tests/`: maintained build/deployment software, runtime helpers, driver changes and checks. `nvidia-gsp-semantic-tgp-v8.patch` contains the current semantic-resolver experiment. See [Tools](docs/TOOLS.md).
- `docs/`: sequential instructions and compatibility notes.
- `docs/wiki/`: version-controlled sources mirrored to the GitHub Wiki.
- `evidence/`: sanitized successful-run records.
- `outputs/225w-research/`, `work/`: preserved research checkpoints, including failed and superseded experiments. Their historical status statements describe those experiments, not the project's current result.

Firmware, raw GPU heap dumps, private machine logs and kernel/initramfs binaries are not bundled. Build from the pinned source and use your installed matching firmware. Archived deployment manifests with `/path/to/research-workspace` are records, not commands to execute; use the current installation guide.

## Operating boundaries

The probe validates the exact GPU, ownership links, policy signatures and expected starting values. It offers a fixed 225 W activation and explicit restoration, not arbitrary memory writes or unrestricted power sliders. Failed or uncertain RPCs stop further mutation. The ordinary stock boot entry remains the recovery path.

Activation runs once on each selected dedicated boot. Continued operation after suspend, GPU reset, firmware override or driver/kernel updates is not established. Those cases require fresh validation rather than automatic retries or removal of guards.

See [component licensing](LICENSES.md) and [contribution guidelines](CONTRIBUTING.md) before redistributing modifications or proposing support for another machine.
