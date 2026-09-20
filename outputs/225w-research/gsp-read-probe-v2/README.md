# Historical v2 WPR read probe

Metadata GET succeeded after obtaining WPR2 bounds from the architecture-specific helper. The read RPC reached GSP but source mapping rejected WPR start0x3ef020000, returning0x1f,zero bytes. This proves rejection of that address, not all GSP heap access. Maximum stayed175W and no GPU writes occurred.

The failed RPC retained its4096-byte SYSMEM destination until reboot; no hot unload. NVLOG source-map marker03030000003d2b48 and null-map path1b04638/1365ae8 identified the failure. Later startup-record analysis located the actual allowed RM heap at3ef024000,size73dc000:16KiB after WPR start. v3 successfully read that region.

The underlying internal RMAPI20800afa handler1365930 was therefore reachable. v3/v4 bounded heap readers superseded this failed-address probe. Historical code and tests remain archived.
