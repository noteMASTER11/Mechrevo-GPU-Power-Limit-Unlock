static NvBool gspProbe_reserveRestore, gspProbe_internalRead;
static OBJGPU *gspProbe_owner;
    static NvBool gspProbe_poisoned = NV_FALSE;
    static NvU32 gspProbe_callsUsed;
    /* Keep an uncertain DMA destination alive after any failed RPC. One allocation
     * maximum; recovery is reboot, not hot unloading this experimental module. */
    static MEMORY_DESCRIPTOR *gspProbe_retainedDesc;
    static void *gspProbe_retainedMap, *gspProbe_retainedPriv;

/* SPDX-License-Identifier: MIT
 * Experimental, explicitly triggered, 4096-byte RM heap page read. GPU group lock held.
 * The caller cannot provide an address, a size, or bytes to write to GPU memory.
 */
static NV_STATUS gspReadProbe(OBJGPU *pGpu, GSP_READ_PROBE_PARAMS *out, NvU32 size)
{
    KernelGsp *kgsp = GPU_GET_KERNEL_GSP(pGpu);
    MEMORY_DESCRIPTOR *desc = NULL;
    void *map = NULL, *priv = NULL;
    NvU32 operation;
    NvU64 start, end, image, imageSize, heap, heapSize, offset;
    NV_STATUS status;
    NV_RANGE wprRegions[2] = {NV_RANGE_EMPTY, NV_RANGE_EMPTY};
    RM_API *api = GPU_GET_PHYSICAL_RMAPI(pGpu);
    NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS transfer;

    if (size != sizeof(*out) || out->version != GSP_READ_PROBE_VERSION ||
        out->operation > 1 || out->reserved[0] || out->reserved[1] ||
        (out->operation == 0 && out->pageOffset != 0))
        return NV_ERR_INVALID_ARGUMENT;
    if (IS_VIRTUAL(pGpu) || gpuIsCCFeatureEnabled(pGpu) || kgsp == NULL ||
        pGpu->idInfo.PCIDeviceID != 0x2c1910deU ||
        pGpu->idInfo.PCISubDeviceID != 0x60411d05U)
        return NV_ERR_NOT_SUPPORTED;

    if (kgsp->pWprMetaHopper != NULL)
    {
        GspFwWprMeta *m = kgsp->pWprMetaHopper;
        if (m->magic != GSP_FW_WPR_META_MAGIC) return NV_ERR_INVALID_DATA;
        start=m->gspFwWprStart; end=m->gspFwWprEnd;
        image=m->gspFwOffset; imageSize=m->sizeOfRadix3Elf;
        heap=m->gspFwHeapOffset; heapSize=m->gspFwHeapSize;
    }
    else if (kgsp->pWprMetaV1 != NULL)
    {
        GspFwWprMetaV1 *m = kgsp->pWprMetaV1;
        if (m->magic != GSP_FW_WPR_META_MAGIC) return NV_ERR_INVALID_DATA;
        start=m->gspFwWprStart; end=m->gspFwWprEnd;
        image=m->gspFwOffset; imageSize=m->sizeOfRadix3Elf;
        heap=m->gspFwHeapOffset; heapSize=m->gspFwHeapSize;
    }
    else return NV_ERR_NOT_SUPPORTED;

    /* Hopper+ boot input does not receive ACR-computed offsets back. Read the
     * actual WPR registers through NVIDIA's architecture-specific read helper.
     * No guessed image/heap address and no hardware register write. */
    kbusCarveoutWprs_HAL(pGpu, GPU_GET_KERNEL_BUS(pGpu), wprRegions);
    if (wprRegions[1].hi < wprRegions[1].lo || wprRegions[1].hi == ~(NvU64)0)
        return NV_ERR_INVALID_DATA;
    start=wprRegions[1].lo; end=wprRegions[1].hi+1;
    /* This probe is deliberately fixed to the observed layout. The first
     * 16 KiB of WPR2 are NOT in the RM FB mapper's allowed regions. */
    if (start != GSP_READ_PROBE_WPR_START || end != GSP_READ_PROBE_WPR_END ||
        imageSize != 0x19be000ULL || heapSize != 0x8a00000ULL)
        return NV_ERR_INVALID_DATA;

    if (gspProbe_owner != NULL && gspProbe_owner != pGpu) return NV_ERR_NOT_SUPPORTED;
    gspProbe_owner=pGpu;
    operation=out->operation; offset=out->pageOffset;
    portMemSet(out,0,sizeof(*out));
    out->version=GSP_READ_PROBE_VERSION; out->operation=operation;
    out->wprStart=start; out->wprEnd=end; out->image=image;
    out->imageSize=imageSize; out->heap=heap; out->heapSize=heapSize;
    out->pageOffset=offset; out->callsUsed=gspProbe_callsUsed;
    out->retainedBuffer=gspProbe_poisoned;
    /* Validate firmware metadata before any allocation or GSP command. */
    if (!gspReadProbeBuild(&transfer,pGpu->idInfo.PCIDeviceID,pGpu->idInfo.PCISubDeviceID,
                           start,end,offset,4096))
        return NV_ERR_INVALID_DATA;
    out->source=GSP_READ_PROBE_SOURCE+offset;
    out->stage=1;
    if (operation == 0) return NV_OK;
    if (gspProbe_poisoned || gspProbe_callsUsed >= GSP_READ_PROBE_MAX_CALLS ||
        (gspProbe_reserveRestore && !gspProbe_internalRead && gspProbe_callsUsed >= GSP_READ_PROBE_MAX_CALLS-128))
        return NV_ERR_INVALID_STATE;

    status=memdescCreate(&desc,pGpu,4096,4096,NV_TRUE,ADDR_SYSMEM,NV_MEMORY_CACHED,0);
    if (status != NV_OK) return status;
    status=memdescAlloc(desc);
    if (status != NV_OK) goto cleanup;
    status=memdescMapOld(desc,0,4096,NV_TRUE,NV_PROTECT_READ_WRITE,&map,&priv);
    if (status != NV_OK) goto cleanup;
    portMemSet(map,0,4096);
    if (!gspReadProbeBuild(&transfer,pGpu->idInfo.PCIDeviceID,pGpu->idInfo.PCISubDeviceID,
                           start,end,offset,memdescGetPhysAddr(desc,AT_GPU,0)))
    { status=NV_ERR_INVALID_DATA; goto cleanup; }
    out->callsUsed=++gspProbe_callsUsed;
    out->stage=2;
    status=api->Control(api,pGpu->hInternalClient,pGpu->hInternalSubdevice,
        NV2080_CTRL_CMD_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP,&transfer,sizeof(transfer));
    out->rpcStatus=status;
    if (status != NV_OK)
    {
        gspProbe_poisoned=NV_TRUE;
        gspProbe_retainedDesc=desc; gspProbe_retainedMap=map; gspProbe_retainedPriv=priv;
        out->retainedBuffer=1;
        NV_PRINTF(LEVEL_ERROR,"GSP read probe RPC failed: 0x%x; destination retained until reboot\n",status);
        /* Static ownership references retained intentionally. */
        (void)gspProbe_retainedDesc; (void)gspProbe_retainedMap; (void)gspProbe_retainedPriv;
        return NV_OK; /* Deliver the actual RPC status in the output. */
    }
    portMemCopy(out->data,sizeof(out->data),map,4096);
    out->bytesRead=4096; out->stage=3;
cleanup:
    if (map != NULL) memdescUnmapOld(desc,NV_TRUE,map,priv);
    memdescFree(desc); memdescDestroy(desc);
    return status;
}
