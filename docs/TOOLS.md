# Portable source and tools

| Path | Purpose |
| --- | --- |
| `patches/nvidia-gsp-persistent-v6.patch` | Complete NVIDIA driver delta against `61dcc93722ecb418bb5f2e00923f05b4b8051dd1`, including probe headers/implementation. |
| `src/runtime/query_power.c` | Preload client implementing the private protocol and checking transaction responses. Compiled locally; no binary is shipped. |
| `src/runtime/run_boot.py` | Root-only, token-gated activation/release orchestration with identity/layout validation and a persisted one-attempt journal. |
| `src/runtime/heap_evidence.py` | Parses startup diagnostic mapping evidence required before activation. |
| `src/runtime/transaction_signals.py` | Defers interruption across child transaction completion/evidence recording. |
| `src/runtime/mechrevo-max-tgp.service` | Opt-in boot-scoped oneshot; no retries. |
| `scripts/build.py` | Clone or check an existing pinned checkout, apply patch, compile modules/client, emit hashes. |
| `scripts/prepare_deployment.py` | Read-only host verification plus local manifest/runtime staging. |
| `scripts/install_boot.py` | Explicit privileged installation of a separate verified initramfs and Limine entry. |
| `scripts/test_offline.py` | Python tests and optional sanitizer-enabled C tests, without GPU access. |

Run `python scripts/build.py --help` and `python scripts/test_offline.py --help` for build/test arguments. The deployment commands use the exact paths shown in [INSTALL.md](INSTALL.md).

The normal offline suite includes boot-token, AC state and transaction-readback validation, real subprocess interruption behavior, package layout/hash/base refusal checks, and the cTGP/client mock transaction suites. The optional `tests/test_bridge.c` is the archived host-bridge harness. It requires a matching private heap fixture argument and is **not** run by `test_offline.py`; the fixture is not redistributed. Historical bridge/emulation results are not a claim that a clean clone can reproduce a private firmware/heap experiment unaided.

The NVIDIA kernel and transaction client retain the experimental v6 behavior. Disable competing TGP controllers as a prerequisite; the wrapper neither detects every controller nor terminates software. No particular control-center application is required.

Build artifacts, local manifests, stock-module copies, firmware and diagnostic heap captures are not source dependencies to commit. `build/artifacts/build.json` records locally compiled files, and `build/deployment/manifest.json` records that machine's stock/runtime hashes and absolute paths. Recreate them after moving a checkout or rebuilding. The installer validates them before touching the boot configuration.

The exact tested kernel/PCI/firmware tuple is enforced in `scripts/package_common.py` and by the driver's own validation. Portability here means reproducible source layout and commands; changing the hardware tuple, firmware addresses or kernel is a separate research/porting task.
