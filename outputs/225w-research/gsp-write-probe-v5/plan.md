# v5 bounded MAX/UPPER transport experiment

User approved the proposed write/readback/rollback experiment ("Try it").
This is a local research probe, not a production/universal unlock. Existing isolated
615.71.09 worktree; stock modules unchanged; no GitHub publication.

Implement separate root-only command retaining the v4 page reader. Operations:
inspect, identity write, apply fixed225000, restore175000. Dynamic object offsets
must pass range, self-pointer, bidirectional ownership, policy array, subclass
methods and baseline checks. A successful identity write arms one apply per boot.
Only three aligned4-byte fields may be written: FE MAX, effective MAX, UPPER.
No CURRENT, rail, voltage or code writes. Original BoardSet dirty byte is NOT
written: the transport enforces4-byte alignment/size, whereas that setter uses a
byte store. Policy synchronization remains a separate standard-control experiment.

Each field write checks expected old value and immediate readback. Shared poison
and call budget with the reader. Any uncertain RPC retains the staging page and
blocks further DMA. Any partial/mismatched write blocks further writes; recovery
is reboot, not blind retry. Restore checks CURRENT<=175000 before reducing MAX.
No automatic commands on boot, unload or exit. Collector exercise explicitly
performs identity/apply/public-GET/restore; fails closed and records every stage.

- [x] Pure policy engine tests first using captured heap + fault injection.
- [x] Shared DMA lifetime/guards and 4-byte SYS→FB transport; kernel host tests.
- [x] Collector verifies intermediate advertised MAX and restoration, no load.
- [x] Build, review, verify separate initramfs and install manual v5 boot entry.
- [x] Hardware identity/apply/restore succeeded on v5; public GET225000→175000 confirmed.

Initial static evidence: upstream mem_utils.c:_memmgrMemReadOrWriteWithGsp
uses SYS→FB staging in the same API; GSP1365930 maps both sides and performs
4-byte-aligned copies, including the non-CC path. Hardware write remains untested.

Ledger: prepared_not_booted. No GPU writes or GitHub publication. Independent review fixes verified RED→GREEN: reserve and report state on budget refusal; defer parent termination to prevent subprocess.run SIGKILL during restoration. Firmware hash, module hashes, initramfs contents and stock modules verified. Runtime requires manual v5 boot.

Runtime ledger: 2026-09-20 22:46, boot1a177a25-e406-426e-93eb-1e4bd16a1995, seven writes successful and restored. No Xid in captured log, CURRENT145000 throughout. Actual225W draw remains unverified; no publication.
