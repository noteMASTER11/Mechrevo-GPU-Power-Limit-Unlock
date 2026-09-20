# GPU access and limit discovery

The research progressed from public read-only policy calls to validated GSP heap objects. It did not begin by writing guessed offsets or treating every integer resembling a watt limit as a target.

## Establish the public baseline

Read-only RM requests reconstructed from NVML exposed the following interfaces on the tested driver:

| Request | Command | Buffer size | Observed result |
| --- | --- | ---: | --- |
| Policy INFO | `0x2080a618` | 20512 bytes | Success |
| Policy CONTROL | `0x2080a61a` | 13876 bytes | Success |
| Policy STATUS | `0x2080a619` | 397048 bytes | Success |
| Client limit GET, FD/FE | `0x2080a61d` | 12 bytes | NOT_SUPPORTED (`0x56`) |

Board policy index 2 had type 0, ID 0, default 80000 mW and maximum 175000 mW. CURRENT varied with the active policy and boot: earlier captures showed 145 W, while the fresh v6 activation began at 175 W. Neither number is a measurement of actual consumption.

Policies 13 and 14 had type `0x12`, IDs `0x1b`/`0x1c`, and values 210000/60000 with a different unit tag. These were not identified as 210 W and 60 W. Their rail/current meaning requires further version-specific validation; they are not modified.

## Find the actual accessible heap

v1 incorrectly assumed that host boot-input WPR metadata contained runtime ACR-populated heap coordinates. Metadata failed before any copy. v2 obtained WPR2 bounds from the architecture-specific helper, but GSP rejected a copy from WPR start `0x3ef020000`.

Startup NVLOG analysis then located the RM heap at physical `0x3ef024000`, length `0x73dc000`: 16 KiB after WPR start. This distinction explained the failure. An address inside WPR is not necessarily accepted by the RM framebuffer mapper.

The v3 parser checked matching startup records and allocator length rather than trusting static ELF data. v3 successfully copied 256 bytes. v4 extended this to 4096-byte pages and collected 1034 successful transfers: one initial page, 4 MiB, and nine targeted pages. The whole collection is not an atomic snapshot.

## Identify ownership, then confirm behavior

The scanner searches method signatures and board subclass layout, including the common BoardSet/getter family. Scanner matches remain candidates: stale copies and other policies can resemble the live board object. The analyzer independently checks GPU→PMGR, PMGR→GPU, self pointers, the policy group, all 15 populated policy entries, public INFO and targeted rereads.

In the recorded v4 boot, cached heap VA `0x7f2000000` mapped to PA `0x3ef024000`. The GPU, PMGR and board policy had heap offsets `0x196bb0`, `0x380690` and `0x3bc610`. These observed offsets are used only behind fresh per-boot validation of the ownership links and layout; they are not portable addresses to reuse blindly.

As an independent semantic check, original firmware INFO backend execution over the captured heap reproduced all 20512 bytes of the actual public A618 response with zero differences after 12149 instructions. No functions on that path were stubbed. This tied the discovered objects to the public policy response, but still did not prove a write or electrical enforcement.

## Transport boundaries

The v4 reader uses private command `0x2080ff71`, ABI version 4, size 4200 bytes. Metadata performs no DMA; reads use aligned relative page offsets with a fixed 4096-byte length. The host checks observed WPR bounds, boot-input sizes, exact hardware identity, and CC/virtualization exclusions. The read/write implementation shares a 32768-call budget and failure state.

Launchers check module marker/srcversion and firmware hash. Startup mapping evidence can be reused only within the same boot with matching module identity. Optimized Python (`-O` or `PYTHONOPTIMIZE`) is rejected so assertion-based checks cannot disappear.

Do not publish raw heap pages or NVLOG captures: they can contain unrelated machine state. Sanitized summaries and reproducible analyzers are the published evidence. See the [v4 discovery code and records](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/tree/main/outputs/225w-research/gsp-read-probe-v4), [[Validation-and-evidence]], and [[Porting-and-compatibility]].
