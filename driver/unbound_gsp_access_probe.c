/* SPDX-License-Identifier: MIT
 * Read-only proof that a companion module can use NVIDIA's exported RM bridge
 * and the stock GSP memory-transfer command without patching nvidia.ko.
 */
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/pci.h>

#include "nv-modeset-interface.h"
#include "nv-kernel-rmapi-ops.h"
#include "class/cl0000.h"
#include "class/cl0080.h"
#include "class/cl2080.h"
#include "ctrl/ctrl0000/ctrl0000gpu.h"
#include "ctrl/ctrl2080/ctrl2080internal.h"

#define UB_HUBMMU0_PRI_BASE 0x880000
#define UB_WPR2_ADDR_LO (UB_HUBMMU0_PRI_BASE + 0xa824)
#define UB_WPR2_ADDR_HI (UB_HUBMMU0_PRI_BASE + 0xa828)
#define UB_WPR_FIELD_SHIFT 4
#define UB_WPR_ADDR_SHIFT 12
#define UB_WPR_BLOCK_SIZE 0x20000ULL
#define UB_HEAP_START_DELTA 0x4000ULL
#define UB_DEVICE_HANDLE 0x7f000001U
#define UB_SUBDEVICE_HANDLE 0x7f000002U

static nvidia_modeset_rm_ops_t rm_ops;
static nvidia_modeset_stack_ptr rm_stack;
static NvU32 rm_gpu_id;
static NvHandle rm_client;
static bool gpu_open;
static bool stack_allocated;
static bool client_allocated;
static bool device_allocated;
static bool subdevice_allocated;
static void *retained_cpu;
static dma_addr_t retained_dma;
static struct device *retained_device;
static unsigned int transfer_status;
static unsigned long long wpr2_start;
static unsigned long long wpr2_end;
static unsigned long long heap_page_word0;
static unsigned long long heap_page_word1;
static unsigned long long heap_page_word2;
static unsigned long long heap_page_word3;

module_param_named(transfer_status, transfer_status, uint, 0444);
module_param_named(wpr2_start, wpr2_start, ullong, 0444);
module_param_named(wpr2_end, wpr2_end, ullong, 0444);
module_param_named(heap_page_word0, heap_page_word0, ullong, 0444);
module_param_named(heap_page_word1, heap_page_word1, ullong, 0444);
module_param_named(heap_page_word2, heap_page_word2, ullong, 0444);
module_param_named(heap_page_word3, heap_page_word3, ullong, 0444);

static NvU32 ub_rm_alloc(NvHandle root, NvHandle parent, NvHandle object,
                         NvU32 class, void *parameters)
{
    nvidia_kernel_rmapi_ops_t operation = {0};

    operation.op = NV04_ALLOC;
    operation.params.alloc.hRoot = root;
    operation.params.alloc.hObjectParent = parent;
    operation.params.alloc.hObjectNew = object;
    operation.params.alloc.hClass = class;
    operation.params.alloc.pAllocParms = NV_PTR_TO_NvP64(parameters);
    rm_ops.op(rm_stack, &operation);
    return operation.params.alloc.status;
}

static NvU32 ub_rm_control(NvHandle client, NvHandle object, NvU32 command,
                           void *parameters, NvU32 size)
{
    nvidia_kernel_rmapi_ops_t operation = {0};

    operation.op = NV04_CONTROL;
    operation.params.control.hClient = client;
    operation.params.control.hObject = object;
    operation.params.control.cmd = command;
    operation.params.control.params = NV_PTR_TO_NvP64(parameters);
    operation.params.control.paramsSize = size;
    rm_ops.op(rm_stack, &operation);
    return operation.params.control.status;
}

static void ub_rm_free(NvHandle root, NvHandle parent, NvHandle object)
{
    nvidia_kernel_rmapi_ops_t operation = {0};

    operation.op = NV01_FREE;
    operation.params.free.hRoot = root;
    operation.params.free.hObjectParent = parent;
    operation.params.free.hObjectOld = object;
    rm_ops.op(rm_stack, &operation);
}

static int ub_read_wpr(struct pci_dev *pdev)
{
    void __iomem *registers;
    resource_size_t page_start = UB_WPR2_ADDR_LO & PAGE_MASK;
    resource_size_t page_offset = UB_WPR2_ADDR_LO & ~PAGE_MASK;
    u32 low;
    u32 high;

    if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM) ||
        pci_resource_len(pdev, 0) <= UB_WPR2_ADDR_HI)
        return -ENODEV;
    registers = ioremap(pci_resource_start(pdev, 0) + page_start, PAGE_SIZE);
    if (registers == NULL)
        return -ENOMEM;
    low = readl(registers + page_offset);
    high = readl(registers + page_offset + sizeof(u32));
    iounmap(registers);

    wpr2_start = ((u64)(low >> UB_WPR_FIELD_SHIFT)) << UB_WPR_ADDR_SHIFT;
    wpr2_end = (((u64)(high >> UB_WPR_FIELD_SHIFT)) << UB_WPR_ADDR_SHIFT) +
               UB_WPR_BLOCK_SIZE;
    return wpr2_start != 0 && wpr2_end > wpr2_start + UB_HEAP_START_DELTA ? 0 : -EINVAL;
}

static void ub_release_rm(void)
{
    if (subdevice_allocated)
        ub_rm_free(rm_client, UB_DEVICE_HANDLE, UB_SUBDEVICE_HANDLE);
    if (device_allocated)
        ub_rm_free(rm_client, rm_client, UB_DEVICE_HANDLE);
    if (client_allocated)
        ub_rm_free(rm_client, rm_client, rm_client);
    if (gpu_open)
        rm_ops.close_gpu(rm_gpu_id, rm_stack, NV_FALSE);
    if (stack_allocated)
        rm_ops.free_stack(rm_stack);
    subdevice_allocated = false;
    device_allocated = false;
    client_allocated = false;
    gpu_open = false;
    stack_allocated = false;
    rm_stack = NULL;
}

static int __init unbound_gsp_access_init(void)
{
    nv_gpu_info_t gpu_info[NV_MAX_GPUS] = {0};
    NV0000_CTRL_GPU_GET_ID_INFO_V2_PARAMS id = {0};
    NV0080_ALLOC_PARAMETERS device_parameters = {0};
    NV2080_ALLOC_PARAMETERS subdevice_parameters = {0};
    NV2080_CTRL_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP_PARAMS transfer = {0};
    struct pci_dev *pdev;
    void *cpu;
    dma_addr_t dma;
    NvU32 count;
    NvU32 status;
    int rc;

    rm_ops.version_string = NV_VERSION_STRING;
    status = nvidia_get_rm_ops(&rm_ops);
    if (status != NV_OK) {
        pr_err("unbound_gsp_access: NVIDIA RM ABI mismatch (running %s)\n",
               rm_ops.version_string ?: "unknown");
        return -EPROTO;
    }
    count = rm_ops.enumerate_gpus(gpu_info);
    if (count != 1) {
        pr_err("unbound_gsp_access: expected one NVIDIA GPU, found %u\n", count);
        return -ENODEV;
    }
    rm_gpu_id = gpu_info[0].gpu_id;
    pdev = to_pci_dev((struct device *)gpu_info[0].os_device_ptr);
    rc = ub_read_wpr(pdev);
    if (rc)
        return rc;
    if (rm_ops.alloc_stack(&rm_stack) != 0)
        return -ENOMEM;
    stack_allocated = true;
    if (rm_ops.open_gpu(rm_gpu_id, rm_stack, NV_FALSE) != 0) {
        rc = -ENODEV;
        goto fail;
    }
    gpu_open = true;

    status = ub_rm_alloc(NV01_NULL_OBJECT, NV01_NULL_OBJECT, NV01_NULL_OBJECT,
                         NV01_ROOT, &rm_client);
    if (status != NVOS_STATUS_SUCCESS || rm_client == 0) {
        rc = -EIO;
        goto fail;
    }
    client_allocated = true;
    id.gpuId = rm_gpu_id;
    status = ub_rm_control(rm_client, rm_client,
                           NV0000_CTRL_CMD_GPU_GET_ID_INFO_V2, &id, sizeof(id));
    if (status != NVOS_STATUS_SUCCESS) {
        rc = -EIO;
        goto fail;
    }
    device_parameters.deviceId = id.deviceInstance;
    device_parameters.hClientShare = rm_client;
    status = ub_rm_alloc(rm_client, rm_client, UB_DEVICE_HANDLE, NV01_DEVICE_0,
                         &device_parameters);
    if (status != NVOS_STATUS_SUCCESS) {
        rc = -EIO;
        goto fail;
    }
    device_allocated = true;
    status = ub_rm_alloc(rm_client, UB_DEVICE_HANDLE, UB_SUBDEVICE_HANDLE,
                         NV20_SUBDEVICE_0, &subdevice_parameters);
    if (status != NVOS_STATUS_SUCCESS) {
        rc = -EIO;
        goto fail;
    }
    subdevice_allocated = true;

    cpu = dma_alloc_coherent(&pdev->dev, PAGE_SIZE, &dma, GFP_KERNEL);
    if (cpu == NULL) {
        rc = -ENOMEM;
        goto fail;
    }
    memset(cpu, 0, PAGE_SIZE);
    transfer.src.baseAddr = wpr2_start + UB_HEAP_START_DELTA;
    transfer.src.size = PAGE_SIZE;
    transfer.src.aperture = 2;
    transfer.dst.baseAddr = dma;
    transfer.dst.size = PAGE_SIZE;
    transfer.dst.aperture = 1;
    transfer.transferSize = PAGE_SIZE;
    transfer.memop = NV2080_CTRL_MEMMGR_MEMORY_OP_MEMCPY;
    transfer_status = ub_rm_control(rm_client, UB_SUBDEVICE_HANDLE,
        NV2080_CTRL_CMD_INTERNAL_MEMMGR_MEMORY_TRANSFER_WITH_GSP,
        &transfer, sizeof(transfer));
    if (transfer_status != NVOS_STATUS_SUCCESS) {
        retained_cpu = cpu;
        retained_dma = dma;
        retained_device = &pdev->dev;
        try_module_get(THIS_MODULE);
        pr_err("unbound_gsp_access: read RPC failed 0x%x; DMA page retained until reboot\n",
               transfer_status);
        ub_release_rm();
        return 0;
    }

    heap_page_word0 = ((u64 *)cpu)[0];
    heap_page_word1 = ((u64 *)cpu)[1];
    heap_page_word2 = ((u64 *)cpu)[2];
    heap_page_word3 = ((u64 *)cpu)[3];
    dma_free_coherent(&pdev->dev, PAGE_SIZE, cpu, dma);
    ub_release_rm();
    pr_info("unbound_gsp_access: stock RM bridge read WPR2 heap page at 0x%llx\n",
            wpr2_start + UB_HEAP_START_DELTA);
    return 0;

fail:
    ub_release_rm();
    return rc;
}

static void __exit unbound_gsp_access_exit(void)
{
    if (retained_cpu != NULL)
        pr_err("unbound_gsp_access: retained DMA page unexpectedly reached exit\n");
}

module_init(unbound_gsp_access_init);
module_exit(unbound_gsp_access_exit);
MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("Read-only NVIDIA GSP heap access probe through exported RM ops");
MODULE_VERSION("0.1.0");
