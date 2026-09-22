# Semantic TGP v8 validation

v8 is the first boot path whose production resolver receives no configured GSP
heap address, object address, member offset, or stock wattage. It is currently
an experiment prepared for the verified MECHREVO RTX 5080 Laptop host. The
existing v6 instructions remain the established path; v8r6 is a live-validation
candidate.

The first two v8 live boots reached WPR2 but their 256 KiB and 64 KiB memory
descriptors were rejected with `NV_ERR_INVALID_ARGUMENT` before any write. A
smaller transfer length did not change the allocation descriptor. Revision
v8r3 switched to the exact 4 KiB descriptor and transfer shape already
verified by v5. The transport then succeeded, but the resolver's one-page
cache repeatedly fetched pages while rejecting false candidates and exhausted
its 32,768-call budget. It stopped without poisoning the transport or writing
anything. Revision v8r4 reads the bounded 8 MiB object arena sequentially into
one temporary snapshot, then performs the same address-free semantic search in
memory. Live v8r4 then resolved the exact topology previously found manually:
heap source `0x3ef024000`, heap VA base `0x7f2000000`, policy array `0x3823a8`,
board selector `0x3bc610`, and writers `0x3bc718`, `0x3bc724`, and `0x3843ac`.
It verified three ceiling writes to 250,000 mW. Its internal-client base-policy
GET was rejected with `NV_ERR_NOT_SUPPORTED`.

v8r5 moved the base-policy request to the external RM client, where GET
succeeded. A clean-boot, policy-2-only SET immediately returned
`NV_ERR_RESET_REQUIRED`; the GSP reported that the PMU had halted. Eight seconds
later the GNOME Shell channel timed out and the compositor crashed. The failure
happened without a GPU workload, so it does not demonstrate a physical
over-current event. It demonstrates that the inferred SET ABI is invalid or
incomplete on this driver. See the
[incident record](../evidence/2026-09-22-v8r5-pmu-halt.md).

The hidden NVML policy packer was then invoked in a separate diagnostic process
with an ioctl interposer that captured and suppressed `0x2080e61b`. Its output
proved that GET and SET do not share a header: SET writes `0x000000ff` at
`+0x0c`, leaves `+0x10` zero, and places the policy-2 type/value at the usual
`0xc4` entry stride. The reconstructed v8r6 buffer matched all 13,876 captured
bytes. v8r6 keeps the one-shot rule and requires a successful GET readback
before reporting activation.

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
provided by the user-space runner or stored as driver-library offsets.

## Boot transaction

The new live-validation entry is named:

```text
CachyOS - NVIDIA Semantic TGP 250W v8r6
```

This entry removes `quiet` and `splash`, disables Plymouth, and enables systemd
status output. The resolver's progress and final five-second success hold are
therefore visible directly on the text boot console. The ordinary CachyOS
entry keeps its original graphical boot arguments.

Before selecting it:

1. connect AC and the intended cooling system;
2. keep UCC running;
3. enable UCC **Max TGP**, which yields GPU TGP ownership while preserving the
   system profile, fans, and water-cooler controls;
4. keep the ordinary CachyOS entry available.

The v8r6 entry and service use the unique token
`codex.semantic_tgp=250-v8r6`. The service runs the C resolver, verifies the
three ceiling writes, performs one correctly serialized policy-2 SET, and reads
the policy table back. It never retries SET in the same boot. The v8r5 helper
and activation service remain disabled.

After boot, collect the result with:

```sh
systemctl status mechrevo-semantic-tgp.service
journalctl -b -u mechrevo-semantic-tgp.service --no-pager
nvidia-smi --query-gpu=name,power.limit,enforced.power.limit,power.max_limit,power.draw,temperature.gpu --format=csv
```

Do not start a load test unless the service reports success and both NVML
CURRENT and maximum read back at 250,000 mW.

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
all five NVIDIA kernel modules, builds the standalone address-free C runner,
checks the module marker, and writes `build/semantic-v8/build.json`. It does not
copy modules into the running system, change the bootloader, load a module, or
issue a power command.

## Remaining compatibility boundaries

Dynamic object discovery removes the offsets that previously changed whenever
the private GSP object layout moved. v8 still requires:

- the patch to compile against the matching NVIDIA open-module source;
- the Blackwell WPR2 register and ACR heap-prefix convention;
- the current internal protected-memory transfer command;
- the current PMGR direct-TGP GET/SET control layout.

These are the next resolver/adapter boundaries to identify at runtime or encode
as independently validated architecture/ABI adapters.
