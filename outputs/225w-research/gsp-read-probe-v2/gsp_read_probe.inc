/* SPDX-License-Identifier: MIT
 * Experimental, explicitly triggered, 256-byte WPR metadata read. GPU group lock held.
 * The caller cannot provide an address, a size, or bytes to write to GPU memory.
 */
static NV_STATUS gspReadProbe(OBJGPU *pGpu, GSP_READ_PROBE_PARAMS *out, NvU32 size)
{
    static NvBool attempted = NV_FALSE;
    static GSP_READ_PROBE_PARAMS saved;
    /* Keep an uncertain DMA destination alive after any failed RPC. One allocation
     * maximum; recovery is reboot, not hot unloading this experimental module. */
    static MEMORY_DESCRIPTOR *retainedDesc;
    static void *retainedMap, *retainedPriv;
    KernelGsp *kgsp = GPU_GET_KERNEL_GSP(pGpu);
    MEMORY_DESCRIPTOR *desc = NULL;
    void *map = NULL, *priv = NULL;
    NvU32 operation;
    NvU64 start, end, image, imageSize, heap, heapSize;
    NV_STATUS status;
    NV_RANGE wprRegions[2] = {NV_RANGE_EMPTY, NV_RANGE_EMPTY};
    RM_API *api = GPU_GET_PHYSICAL_RMAPI(pGpu);
    NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS transfer;

    if (size != sizeof(*out) || out->version != GSP_READ_PROBE_VERSION ||
        out->operation > 1 || out->reserved[0] || out->reserved[1])
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

    operation=out->operation;
    portMemSet(out,0,sizeof(*out));
    out->version=GSP_READ_PROBE_VERSION; out->operation=operation;
    out->wprStart=start; out->wprEnd=end; out->image=image;
    out->imageSize=imageSize; out->heap=heap; out->heapSize=heapSize;
    /* Validate firmware metadata before any allocation or GSP command. */
    if (!gspReadProbeBuild(&transfer,pGpu->idInfo.PCIDeviceID,pGpu->idInfo.PCISubDeviceID,
                           start,end,start,256,4096))
        return NV_ERR_INVALID_DATA;
    out->stage=1;
    if (operation == 0) return NV_OK;
    if (attempted) { *out=saved; return NV_OK; }

    status=memdescCreate(&desc,pGpu,4096,4096,NV_TRUE,ADDR_SYSMEM,NV_MEMORY_CACHED,0);
    if (status != NV_OK) return status;
    status=memdescAlloc(desc);
    if (status != NV_OK) goto cleanup;
    status=memdescMapOld(desc,0,4096,NV_TRUE,NV_PROTECT_READ_WRITE,&map,&priv);
    if (status != NV_OK) goto cleanup;
    portMemSet(map,0,4096);
    if (!gspReadProbeBuild(&transfer,pGpu->idInfo.PCIDeviceID,pGpu->idInfo.PCISubDeviceID,
                           start,end,start,256,memdescGetPhysAddr(desc,AT_GPU,0)))
    { status=NV_ERR_INVALID_DATA; goto cleanup; }
    attempted=NV_TRUE;
    out->stage=2;
    status=api->Control(api,pGpu->hInternalClient,pGpu->hInternalSubdevice,
        NV2080_CTRL_CMD_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP,&transfer,sizeof(transfer));
    out->rpcStatus=status;
    if (status != NV_OK)
    {
        retainedDesc=desc; retainedMap=map; retainedPriv=priv;
        out->retainedBuffer=1;
        NV_PRINTF(LEVEL_ERROR,"GSP read probe RPC failed: 0x%x; destination retained until reboot\n",status);
        /* Static ownership references retained intentionally. */
        (void)retainedDesc; (void)retainedMap; (void)retainedPriv;
        saved=*out;
        return NV_OK; /* Deliver the actual RPC status in the output. */
    }
    portMemCopy(out->data,sizeof(out->data),map,256);
    out->bytesRead=256; out->stage=3; saved=*out;
cleanup:
    if (map != NULL) memdescUnmapOld(desc,NV_TRUE,map,priv);
    memdescFree(desc); memdescDestroy(desc);
    return status;
}
