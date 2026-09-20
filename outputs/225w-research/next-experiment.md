> **Historical research checkpoint.** Status statements below describe that stage. See the [current guide](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/blob/main/docs/INSTALL.md) and [successful-run evidence](https://github.com/noteMASTER11/Mechrevo-GPU-Power-Limit-Unlock/blob/main/evidence/README.md) for the working v6 result.

# Current checkpoint and next experiment

v5 completed seven live word writes: identity FE175000, three ceilings225000, three restores175000. Public INFO confirmed225000 then175000. CURRENT stayed145000. No Xid in the captured kernel journal; GPU remained responsive. See gsp-write-probe-v5/live-result.json.

Next: establish a standard operating-power request, verify PMU enforcement, then measure real draw under the owner's benchmark. Raising MAX/UPPER alone does not change CURRENT. v5 allows one apply per boot and that cycle has been consumed; repeating its launcher cannot enable225W. A persistent activation must preserve expected-value guards, diagnostics and a usable restore path.

History: VBIOS override abandoned; v1 host metadata offsets invalid; v2 read atWPR start rejected by mapper; v3 read256bytes at actual heap start succeeded; v4 paged reads located live GPU/PMGR/policy objects; original GSP INFO emulation matched all20512public bytes; v5 SYS→FB writes and restoration succeeded.

mVolt documentation distinguishes rail OCP from board caps and does not promise to exceed the ordinary driver maximum. Linux NvAPI topology methods from Loong's nvidia-tools returned success locally, but its status parser interpreted DWORD+4=0x501 as1281channels, inconsistent with the9432-byte buffer. Preserve raw response and decode the actual layout before labeling rails. No I2C tuning commands were issued.

Publication was explicitly authorized after the reversible ceiling result; actual225W consumption remains unverified.
