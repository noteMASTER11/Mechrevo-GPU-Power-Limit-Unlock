# Semantic TGP v8 validation

v8 is the first boot path whose production resolver receives no configured GSP
heap address, object address, member offset, or stock wattage. It is currently
an experiment prepared for the verified MECHREVO RTX 5080 Laptop host. The
existing v6 instructions remain the established path until this entry completes
a clean live boot and load test.

The first v8 live boot reached WPR2 successfully but the initial 256 KiB heap
DMA was rejected with `NV_ERR_INVALID_ARGUMENT` before any write. Revision
v8r2 negotiates the supported power-of-two transfer size from 64 KiB down to
the already verified 4 KiB transport. Only an explicit invalid-argument result
permits a smaller retry; every uncertain transfer error still poisons the boot
epoch. The first attempt performed zero writes and left the stock 175 W maximum
intact.

## What v8 resolves

At every operation the in-tree NVIDIA owner adapter reads the current protected
GSP heap through RM's internal transfer command and requires exactly one
topology containing:

1. a 20-slot policy pointer array with at least eight active policies;
2. one common object member whose value matches each active slot index;
3. a board selector with one source tagged `0xfe` and matching effective/source
   values;
4. a policy-2 cTGP tuple whose lower and upper values are independently present
   in selectors;
5. a unique link from slot 2 to the board selector and cTGP tuple.

The resulting three write destinations are discovered values. They are not
provided by `query_power.so` or stored as driver-library offsets.

## Boot transaction

The isolated entry is named:

```text
CachyOS - NVIDIA Semantic TGP 250W v8
```

Before selecting it:

1. connect AC and the intended cooling system;
2. keep UCC running;
3. enable UCC **Max TGP**, which yields GPU TGP ownership while preserving the
   system profile, fans, and water-cooler controls;
4. keep the ordinary CachyOS entry available.

The boot service waits for GSP-RM and UCC, verifies that `nvidia-powerd` has
yielded Dynamic Boost ownership, and runs operations `inspect`, `arm`,
`ceilings`, and `direct set`. Console output includes green `[SUCCESS]` records
for the unique resolution, verified ceiling writes, and the final 250 W NVML
limit. A verified success remains on screen for five seconds. Any ambiguity,
unexpected pre-state, transfer error, readback mismatch, active competing TGP
request, or wrong module marker stops the transaction.

After boot, collect the result with:

```sh
systemctl status mechrevo-semantic-tgp.service
journalctl -b -u mechrevo-semantic-tgp.service --no-pager
nvidia-smi --query-gpu=name,power.limit,enforced.power.limit,power.max_limit,power.draw,temperature.gpu --format=csv
```

Do not start a load test unless the service completed successfully and UCC
still controls the platform profile and cooling devices.

## Offline validation

Run the portable synthetic tests:

```sh
make test
```

The semantic test checks relocation of the heap VA, elevated values after an
apply, rejection of a duplicate valid selector, and rejection of a damaged
source tag. The private real-heap regression fixture is intentionally excluded
from the repository; its expected unique result is recorded in local research
evidence.

Prepare and build the exact patched NVIDIA tree without installing it:

```sh
python scripts/build_semantic_v8.py --prepare-only
python scripts/build_semantic_v8.py
```

The build script pins NVIDIA `615.71.09`, verifies the exact patch tree, builds
all five NVIDIA kernel modules, builds the address-free preload client, checks
the module marker, and writes `build/semantic-v8/build.json`. It does not copy
modules into the running system, change the bootloader, load a module, or issue
a power command.

## Remaining compatibility boundaries

Dynamic object discovery removes the offsets that previously changed whenever
the private GSP object layout moved. v8 still requires:

- the patch to compile against the matching NVIDIA open-module source;
- the Blackwell WPR2 register and ACR heap-prefix convention;
- the current internal protected-memory transfer command;
- the current PMGR direct-TGP GET/SET control layout.

These are the next resolver/adapter boundaries to identify at runtime or encode
as independently validated architecture/ABI adapters.
