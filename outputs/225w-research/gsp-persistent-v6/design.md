# Boot-scoped Max TGP experiment

The dedicated boot entry requests 225 W automatically; ordinary CachyOS keeps the stock driver. UCC yields ownership of GPU power configuration whenever `codex.max_tgp=225` is present, and can independently yield ownership through its Max TGP checkbox. The UI displays measured system limits, never assumes that the checkbox proves an unlock.

The existing validated MAX/FE/UPPER writes are retained. A new bounded bridge invokes NVIDIA's own internal CONFIGURE_TURBO_V2 and CONFIGURE_TGP_MODE handlers, after checking the pinned GPU, policy links, ceilings, support, lower bound and original configuration. Offset is exactly 225000 - 80000 = 145000 mW. The original generator prepares CURRENT/source F7 = 225000 with this offset. Emulation stops before policy submission; PMU enforcement remains unverified.

Activation is once per boot. A failed/uncertain RPC poisons further actions. An explicit release first removes offset, disables mode, confirms CURRENT <=175000, then restores ceilings. No voltage/current-rail or EEPROM writes. The boot service runs once, records truthful readback, and does not retry failed writes. Reset/reinitialization or firmware overriding configuration must be detected during live verification; continuous enforcement is not yet established.
