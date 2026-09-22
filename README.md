# NVIDIA Laptop GPU Power Policy for Linux

This repository contains the live-validated **V11 semantic resolver** used to raise the power policy of a MECHREVO RTX 5080 Laptop GPU on Linux. It patches NVIDIA's open kernel module, discovers the current GSP power-policy topology at boot, applies a guarded transaction, and asks NVIDIA's original GSP controls to activate a 225 W base/current policy under a 250 W ceiling.

No VBIOS or EEPROM is flashed. The ordinary stock boot entry remains the recovery path.

## Verified result

| Property | Stock | V11 readback |
|---|---:|---:|
| Total ceiling / MAX | 175 W | 250 W |
| Base/internal policy | 150 W | 225 W |
| CURRENT | 80 W at captured boot | 225 W |
| Entry 13 envelope | 210000 | 250000 |
| Entry 14 envelope | 60000 | 100000 |

V11 completed 16 protected-heap writes, zero rollback writes, and two original GSP control calls. A second semantic resolution recognized the resulting active CURRENT record. Four FurMark workloads reached 202.32–209.63 W peak instantaneous power with no Xid, PMU halt, hardware brake, or board-limit event. NVIDIA reported `SW Power Cap`; synchronized policy status localized that condition to the aggregate TOTAL regulator while the two raised rail envelopes retained substantial headroom.

The result is address-free with respect to the live power topology: user space supplies no heap address, object address, member offset, or saved layout. Compatibility is still bounded by the NVIDIA source/GSP ABI and the Blackwell protected-memory transport used by the adapter.

## Repository layout

- `core/` and `include/`: portable semantic resolver and contract code.
- `patches/nvidia-gsp-unified-tgp-v11.patch`: production patch for the pinned NVIDIA open-module source.
- `src/runtime/`: standalone C launcher and systemd units.
- `scripts/build_unified_v11.py`: reproducible build without installation.
- `tests/`: synthetic relocation, ambiguity, active-state, and contract tests.
- `tools/diagnostics/`: read-only diagnostic utilities.

Detailed theory, installation, validation evidence, recovery, and the complete research history live in the [GitHub Wiki](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/wiki).

## Build and test

```sh
make test
python scripts/build_unified_v11.py --prepare-only
python scripts/build_unified_v11.py
```

The build script pins NVIDIA source commit `61dcc93722ecb418bb5f2e00923f05b4b8051dd1`, builds against the selected kernel headers, compiles the C launcher, verifies module marker `semantic-tgp-v11-20260922`, and emits hashes under `build/unified-v11/`. It does not install modules, edit the bootloader, load a driver, or change a power limit.

The tested host is MECHREVO YAOSHI Series-X6AR55xY, GPU `10de:2c19`, subsystem `1d05:6041`, CachyOS kernel `7.2.6-1-cachyos`, NVIDIA/GSP `615.71.09`, and VBIOS `98.03.5E.00.5C`. Treat other hardware or driver versions as ports requiring fresh validation.

Before activation, software that writes GPU TGP must yield ownership. On the tested host UCC remained active for system profile, fan, and water-cooler control while its **Max TGP** mode stopped TGP writes. Dynamic Boost service `nvidia-powerd` was disabled only for the dedicated V11 boot.

See [LICENSES.md](LICENSES.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
