# Offline execution of the Windows reference patch

Original Nvpwr.sys executed in Unicorn x86-64 against synthetic Windows/NVIDIA objects. No Windows kernel code was loaded into Linux and no GPU was accessed. GUI EXE was not run. This establishes which actions the SYS initiates, not behavior of real NVIDIA functions replaced by the model.

Reference: https://github.com/LevinAi-arch/rtx-5070ti-laptop-160w-power-limit/tree/93f55e954e4dc3db1aefe5fb3bd4f0a1f8d283c3 . SYS SHA256:e9cb3f6a0a6c92df8e646f931c4078627812680e5e2051a4392330cf8cce80e8. Binary not redistributed here.

The harness loads PE sections/imports and executes GsDriverEntry, DriverEntry and the driver's registered DispatchDeviceControl. Success+restore traversed1425distinct SYS instruction addresses. IOCTL22e004 input:version2,target225000,profile2,reserved0. Restore uses22e008. XMGPowerPatch.sys250W is outside this experiment.

Modeled dependencies: Windows allocation/string/device/IRP/lock APIs; synthetic nvlddmkm616.92 metadata(timestamp6a9b4070,size06d3e000), six signatures, Global→table→Major→Root→Board, PolicyLookup/BoardSet/SetAmount/SetEligibility. Baseline CTGP/UPPER/MAX/CURRENT/F7=175000,amount/eligibility/amountActive=0,LOWER5000. Synthetic GPU ID5080 is not a PCI ID. Unmapped memory and unknown calls stop execution. Actual SYS instructions and modeled NVIDIA effects are logged separately.

Successful sequence: validate/save originals; BoardSet(MAX,FE,175000); Root+3d24UPPER175000; Root+3d14CTGP200000; SetAmount25000 and SetEligibility1; verify armed baseline; BoardSet(MAX,FE,225000); UPPER225000; SetEligibility1; verify applied. Direct xchg writes:SYS RVA3182 UPPER175000,318d CTGP200000,3305 UPPER225000. The modeled generator computes F7=min(CTGP,UPPER-amount)+amount. This path performs no direct VBIOS/code/rail-policy writes; hidden real NVIDIA function behavior is not covered.

Eight cases: success+restore; wrong PE timestamp(C0000059); changed signature(C0000059);250W with5080profile(C000000D);140W baseline(C000000D);wrong ABIversion(C000000D); modeled MAX setter failure(C0000001,rollback); stale modeled F7(C000003E,rollback). Tests check order, early refusal/no writes and restoration.

Reproduction requires Python packages in requirements.txt and separately obtained hash-matching SYS. Run emulate_nvpwr.py --sys /path/to/Nvpwr.sys and test_emulator.py. Scripts are research harnesses, not patch-installation tools.
