# Recovery and troubleshooting

Keep the ordinary stock boot entry and stock NVIDIA module available. The dedicated v6 entry selects an experimental request once per boot. A failed or partial mutation is not a confirmed rollback, even if the launching command has returned.

Use [INSTALL.md](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/blob/main/docs/INSTALL.md) for the supported deployment and release commands. Historical manifests and paths describe the original machine; they are not portable installers.

## Successful release order

v6 operation 5 releases cTGP with offset 0 followed by mode 0. It requires CURRENT≤175000 before operation 3 restores FE MAX, effective MAX and UPPER to 175000. Lowering ceilings first while CURRENT is still 225000 violates the write guard.

If activation was rejected before its first cTGP command, the implementation permits a verified operation-5 no-op so ceiling restoration can proceed. This narrow known-state case is different from a command that may have reached GSP and then timed out. Indeterminate RPC or readback failures poison further mutation.

Two-phase v6 recovery reserves 256 DMA calls, including the existing 128-call ceiling restoration allowance. This reservation prevents ordinary reads from consuming the recovery allowance; it does not make an uncertain RPC safe to retry.

## Failure behavior

| Symptom | Meaning and next action |
| --- | --- |
| Wrong module, firmware hash, GPU identity or heap evidence | Preflight refuses the unsupported state. Check the selected boot and pinned inputs; do not bypass the checks. |
| Missing dedicated boot token | The oneshot should skip an ordinary boot. Use the intended boot entry if activation is desired. |
| AC check fails | The boot request is refused. Resolve the prerequisite before a fresh supported attempt. |
| Another program controls TGP | Disable that controller before activation; competing writes can overwrite the requested policy. |
| Attempt already recorded | The one-attempt guard is working. Do not delete its journal or remove the kernel guard to retry. |
| DMA/RPC timeout, poisoned state, or ambiguous partial write | Stop experimental operations and reboot into the ordinary stock entry. Do not assume restoration occurred. |
| CURRENT stays above 175000 during release | Ceiling restoration is not safe under the guard. Use stock-boot recovery. |
| Public MAX=225 W but CURRENT or enforced limit is lower | Ceiling change alone does not establish successful activation. Check the cTGP result and whether another controller is changing TGP. |
| NVML requested value unavailable | Unsupported requested-limit reporting is distinct from enforced-limit reporting. Treat requested and enforced readings independently. |
| Policy changes after suspend/reset | Those lifecycle cases are not established. The service does not continuously reapply or retry the request. |

After a failed RPC, the owned DMA page is retained until reboot because firmware access may still be outstanding. Do not hot-unload the experimental module to free it. The implementation stops subsequent experimental DMA instead of guessing that the transfer has ended.

## Read state, not just process exit status

Inspect the operation result, stage, active/armed/poisoned flags, write count and field readback together. An outer ioctl or process result of zero is not sufficient if the embedded firmware status indicates failure. A refused request can still report an already-active or poisoned state.

The installed workflow records an exclusive attempt before GPU access and persists activation evidence. On the original local deployment, the service was `mechrevo-max-tgp.service`, its per-boot activation record lived under `/var/lib/mechrevo-max-tgp/`, and a small public summary was exposed at `/run/mechrevo-max-tgp/status.json`. Follow the current installation guide for deployed paths.

Ordinary SIGINT, SIGTERM and SIGHUP are deferred through the launcher/child transaction and evidence persistence. SIGKILL, process crashes and power loss are outside that guarantee. No timer, unload hook or reset hook silently reapplies the setting.

## Preserve useful evidence

Before further diagnosis, retain the boot identity, exact module and firmware identities, summarized operation response and public policy readings. Keep raw heap, NVLOG and machine journals private. A sanitized summary is sufficient for a public issue; do not upload memory captures as an ordinary bug-report attachment.

A successful stock reboot is the fallback after uncertain mutation. A verified explicit release is the controlled path only when the transport remains healthy and the implementation accepts it. See [[How-It-Works]] and [[Validation-and-evidence]] for the underlying state transitions.
