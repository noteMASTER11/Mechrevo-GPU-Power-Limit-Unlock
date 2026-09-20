# GSP615.71.09 MAX/UPPER reverse-engineering notes

Condensed English record of the static-analysis stage. Live object discovery and writes were established later by v4/v5. Firmware SHA256:c420726d2c76c55f028a59f67ebd7a906563ef807c945bb3baa9e88c36bf334b.

Embedded ELF64/RISC-V begins at outer offset0x1dd040. RX:VA0x1000000,file0,size0xed2000; RW:VA0x4000000,file0xed2000,size0x1af000. Code/file translation is segment-specific. These are GSP addresses, not Linux CPU pointers.

| RM command | GSP handler VA | Bytes |
| --- | --- | ---: |
| CONFIGURE_TURBO_V2 20800ad3 |1768618 |4 |
| INFO2080a618 |1777608 |20512 |
| CONTROL GET2080a61a |1789848 |13876 |
| CONTROL SET2080e61b |178e168 |13876 |
| CLIENT SET2080e61e |178ea48 |12 |

Common BoardSet177f3c4 selects arbiters:0→64,1→b4,2/MAX→104,3/CURRENT→1f4,4→154,5→1a4,6→244 (hex object offsets). Getter177c250 reads effective at arbiter+4. MAX mode0 aggregates the minimum via17b7f38. Therefore adding FD225000 alongside FE175000 leaves MAX175000; replacing FE is necessary. Original derived board setter17b2248 calls the common setter and marks board+44 dirty. v5 writes only the separately verified ceiling words, not that byte.

SET_CONTROL type0 board path17b8aa0 and type12 rail path17b62e0 both call178096c, which reads requested payload+4, checks MIN/MAX, then writes CURRENT(selector3,sourceFE). Its invalid-range path includes an ebreak; out-of-range probing was not used as an unlock. CLIENT SET also checks MAX. The UINT32_MAX sentinel is not225W.

Internal178aed0 accepts selectors0..6 but forcesFD; its caller184de98 is under opcode1800, with host reachability unproven. Callback17bc5a4 usesFE and selector2-entry[6] for entry[6]0..2; configured at17ceef2. Profile GETs later returned empty sets, not a usable unlock path.

PMGR cTGP fields:3d08support,3d09mode,3d0aoffsetActive,3d0crequest,3d10offset,3d14policyIndex,3d18lower,3d1cUPPER. Initializer17cb890 copies bounds from policy INFO. Setter1768618 checks upper>=lower and offset<=upper-lower. Generator17cbba0 has a conditional branch equivalent to F7=min(request,upper-offset)+offset; source3f7 denotes CURRENT/F7. For request200000,offset25000,UPPER175000 the result remains175000; with UPPER225000 it becomes225000. This arithmetic does not establish that the Windows200+25 arrangement maps directly to Linux controls.

The key Linux/Windows difference: Windows reference updates nvlddmkm objects in host kernel memory; Linux counterparts live inside GSP. Editing an NVML result or only removing a host check cannot replace the firmware's MAX enforcement.

HYDRA author descriptions pointed toward shared board policy and cautioned against treating rail-current policy values as watts. These descriptions were hypotheses/source context, not Linux implementation evidence. References: https://www.patreon.com/1usmus ; https://github.com/b00nz/mVolt . Original binary emulation and live v4/v5 evidence are the stronger evidence for this specific firmware/device.
