# Build and install the v6 experiment

These instructions reproduce the tested **MECHREVO YAOSHI Series-X6AR55xY / RTX 5080 Laptop GPU**, PCI `10de:2c19`, subsystem `1d05:6041`, on **CachyOS kernel `7.2.6-1-cachyos`**, NVIDIA **615.71.09**, with **Limine and mkinitcpio**. They are not a universal laptop power unlock. The source is portable between checkout directories; the hardware offsets and installation checks intentionally remain specific to this tested configuration.

**Before activation, disable software that controls GPU TGP/power limits.** A competing controller can overwrite or conflict with this experiment. The tools do not stop services or verify that every possible controller has been disabled.

The packaged runtime retains the tested GPU transaction, AC, firmware/module identity, heap-layout and one-attempt-per-boot checks. The packaged build and offline checks pass; a fresh installation of this public package has not been boot-tested. The recorded successful boot/load test used the original runtime.

## 1. Inspect the current state without changing it

Obtain this repository and enter its root first:

```sh
git clone https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock.git
cd Mechrevo-GPU-Power-Limit-Unlock
```

Run the following from the repository root. None of these commands raises power or loads a module:

```sh
uname -r
lspci -nn -d 10de:
nvidia-smi --query-gpu=name,driver_version,pci.bus_id,power.limit,enforced.power.limit,power.default_limit,power.min_limit,power.max_limit,power.draw,temperature.gpu --format=csv
nvidia-smi -q -d POWER
sha256sum /usr/lib/firmware/nvidia/615.71.09/gsp_ga10x.bin
```

Expected firmware SHA-256:

```text
c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b
```

NVML exposes supported public readings, not every internal policy limit. `N/A` is unavailable data. A 225 W maximum alone does not establish an enforced 225 W policy or actual 225 W consumption. Preserve your baseline readings.

## 2. Install build prerequisites

Use a working installation of the exact kernel and matching headers, NVIDIA 615.71.09 userspace tools/firmware, and an existing stock Limine boot entry. Keep the stock kernel/modules and a working recovery entry. On that CachyOS installation:

```sh
sudo pacman -S --needed base-devel git python clang lld elfutils mkinitcpio pciutils
```

The matching headers must already be available at `/usr/lib/modules/7.2.6-1-cachyos/build`. A rolling-release update may no longer supply this version. Obtain the matching kernel/header/userspace packages from your existing package cache or trusted CachyOS package archive; these scripts do not downgrade packages or substitute newer versions. Do not mix the pinned module source with a different NVIDIA userspace release. Secure Boot/module-signing setups require their own signing procedure and are not implemented here.

## 3. Fetch, patch, and build

The default command clones official NVIDIA source into `build/nvidia`, checks out the full pinned base commit, applies the complete patch, builds the modules for the tested kernel, and compiles the userspace client:

```sh
python scripts/build.py --jobs 12
```

To use an existing clean checkout at the same commit:

```sh
python scripts/build.py --source /absolute/path/to/open-gpu-kernel-modules --jobs 12
```

Required NVIDIA base: `61dcc93722ecb418bb5f2e00923f05b4b8051dd1`. The helper refuses a different commit or unrelated source edits. It stages the patch in that checkout so added source files are included in subsequent verification. It creates `build/artifacts/query_power.so` and `build/artifacts/build.json`; modules remain in the selected source's `kernel-open` directory. No module is installed or loaded.

For source preparation and offline tests without building modules:

```sh
python scripts/build.py --prepare-only
python scripts/test_offline.py --source build/nvidia
```

## 4. Prepare a local manifest

After a successful full build:

```sh
python scripts/prepare_deployment.py
python -m json.tool build/deployment/manifest.json
```

This checks the tested OS/kernel, GPU/subsystem and firmware, verifies the built module/client hashes, records hashes of your current stock module files, and stages runtime files under `build/deployment/runtime`. It writes only inside the checkout. It refuses to overwrite an existing `build/deployment` directory. Keep the checkout, source and build outputs in place until installation is complete; the generated manifest deliberately records their absolute local paths.

## 5. Install the separate opt-in boot entry

Review the manifest and ensure competing TGP controllers will stay disabled on the experimental boot, then run:

```sh
sudo python scripts/install_boot.py build/deployment/manifest.json
```

The installer rechecks the host and hashes, copies the stock module tree into an isolated build directory, substitutes the four patched NVIDIA modules there, and builds/extracts/verifies an isolated initramfs. It verifies the firmware and module hashes inside the image and checks the module dependency chain. It copies the stock kernel plus this initramfs to `/boot/codex-max-tgp-v6`, installs the boot helper and oneshot service, and appends **CachyOS - NVIDIA Max TGP 225W v6** to `/boot/limine.conf`. The ordinary stock entry and installed stock module files are preserved. A copy of the previous Limine configuration is saved at `build/deployment/runtime/limine-before.conf`.

The installer expects the stock `//linux-cachyos` stanza format with a `boot():/...#<BLAKE2b>` kernel path. It refuses other layouts and existing experimental installations. It is a single-install tool, not an upgrade/repair tool. On a failure, inspect the error and any partial files before retrying; it does not automatically roll back all intermediate installation files. The new boot entry is appended only after image verification and service setup.

It does not invoke the GPU transaction, unload/load drivers, change the default boot entry, or reboot. **Selecting the experimental entry on the next boot activates the 225 W request automatically.** Choose that entry manually when ready; use AC power.

## 6. Verify the selected boot

After manually booting the experimental entry:

```sh
cat /proc/cmdline
systemctl status mechrevo-max-tgp.service --no-pager
cat /run/mechrevo-max-tgp/status.json
sudo journalctl -b -u mechrevo-max-tgp.service --no-pager
nvidia-smi --query-gpu=name,power.limit,enforced.power.limit,power.max_limit,power.draw,temperature.gpu --format=csv
sudo cat /var/lib/mechrevo-max-tgp/"$(cat /proc/sys/kernel/random/boot_id)"/activate.json
```

Require a successful service result, `policy_225_verified`, and a fresh 225 W enforced-limit reading. A maximum-limit reading alone is insufficient. Real workload draw must be checked separately; power draw depends on the workload. Raw diagnostic logs are root-private and should not be published without review. The public runtime status intentionally leaves `actual_draw_verified` false because the helper does not run a load test.

If activation fails, do not restart the service or delete its attempt journal to force a retry. Recover through the ordinary stock boot entry and inspect the failure. There is no continuous reapplication loop; behavior across suspend, GPU resets, and later firmware overrides is not established.

## Recovery and removal

The simplest recovery is to reboot manually into the ordinary CachyOS entry. The experimental runtime requires the exact `codex.max_tgp=225` token and therefore skips ordinary boots.

The explicit release command mutates GPU state: it disables the cTGP request, requires CURRENT to be at most 175 W, and then restores the stock ceilings. Use only on the experimental boot when you deliberately want to release ownership:

```sh
sudo python /usr/local/lib/mechrevo-max-tgp/run_boot.py release
```

An indeterminate failure forbids another mutation attempt; use the stock boot recovery path.

To remove the installation, first boot the ordinary entry and confirm the experimental token is absent. Disable the service with `sudo systemctl disable mechrevo-max-tgp.service`. Edit `/boot/limine.conf` to remove only the block between `# BEGIN CODEX MANAGED: MAX TGP V6` and `# END CODEX MANAGED: MAX TGP V6`, including those markers. Then remove `/boot/codex-max-tgp-v6`, `/etc/systemd/system/mechrevo-max-tgp.service`, and `/usr/local/lib/mechrevo-max-tgp`, and run `sudo systemctl daemon-reload`. Preserve `/var/lib/mechrevo-max-tgp` if you need the diagnostic record. Do not blindly overwrite a subsequently updated Limine configuration with the saved backup.
