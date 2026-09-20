# Compatibility and prerequisites

The working implementation is pinned to one machine and GSP layout. A similar laptop name, adequate core temperature, or the presence of an RTX 5080/5090 is not enough to reuse its memory offsets.

## Verified target

- MECHREVO YAOSHI Series-X6AR55xY; RTX 5080 Laptop.
- PCI vendor/device `10de:2c19`, subsystem vendor/device `1d05:6041`.
- NVIDIA open kernel modules and original GSP firmware `615.71.09`.
- NVIDIA source commit `61dcc93722ecb418bb5f2e00923f05b4b8051dd1`.
- `gsp_ga10x.bin` SHA-256: `c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b`.
- VBIOS `98.03.5E.00.5C`; BIOS recorded during research: `N.1.13MRO16`.
- CachyOS kernel `7.2.6-1-cachyos`, matching headers, Clang/LLD 22.1.8, mkinitcpio and Limine.
- Disable software or services that manage GPU TGP/power limits before activation; they can overwrite the requested limit.
- AC power; the owner used external liquid cooling for the successful load test.

The kernel probe also checks WPR/heap layout, object relationships and firmware-specific policy signatures. A user-space compatibility check does not replace these checks.

## Collect identity and stock limits

These commands are read-only:

```bash
uname -r
nvidia-smi --query-gpu=name,pci.bus_id,driver_version,vbios_version,power.min_limit,power.default_limit,power.max_limit,enforced.power.limit --format=csv
lspci -nn -d 10de:
modinfo -F version nvidia
sha256sum /usr/lib/firmware/nvidia/615.71.09/gsp_ga10x.bin
```

For the research machine's PCI address, inspect both device and subsystem IDs:

```bash
for name in vendor device subsystem_vendor subsystem_device; do
    cat "/sys/bus/pci/devices/0000:02:00.0/$name"
done
```

If the address differs, use the NVIDIA GPU address reported by `lspci`, not a guessed device. `power.limit` may be unavailable on this laptop even when `enforced.power.limit` is valid.

## Not validated

- RTX 5090 Laptop, RTX 5070 Ti Laptop, other MECHREVO/Tongfang/Uniwill variants, or desktop GPUs.
- Other NVIDIA/GSP versions or heap layouts, even if the source compiles.
- Other kernels, bootloaders, distributions or module-signing setups.
- Suspend/resume, GPU reset/reinitialization and long-duration sustained operation.
- Rail-current or voltage-limit changes. NVVDD/MSVDD are useful research context, not what this implementation writes.

For a new target, follow the Wiki's porting process: collect identity, preserve stock recovery, rediscover the objects read-only, verify original-code behavior, and only then define a separately validated bounded profile. Do not relax the existing guards to make a mismatching system pass.

Proceed to [Installation](INSTALL.md).
