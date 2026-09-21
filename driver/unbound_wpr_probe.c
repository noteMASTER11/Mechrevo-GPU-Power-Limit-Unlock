/* SPDX-License-Identifier: MIT
 * Read-only Blackwell WPR2 discovery checkpoint.
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>

#define UB_NVIDIA_VENDOR 0x10de
#define UB_HUBMMU0_PRI_BASE 0x880000
#define UB_WPR2_ADDR_LO (UB_HUBMMU0_PRI_BASE + 0xa824)
#define UB_WPR2_ADDR_HI (UB_HUBMMU0_PRI_BASE + 0xa828)
#define UB_WPR_ADDR_SHIFT 12
#define UB_WPR_FIELD_SHIFT 4
#define UB_WPR_BLOCK_SIZE 0x20000ULL

static unsigned long long wpr2_start;
static unsigned long long wpr2_end;
static unsigned int wpr2_lo_raw;
static unsigned int wpr2_hi_raw;
static unsigned int matched_gpus;

module_param_named(wpr2_start, wpr2_start, ullong, 0444);
module_param_named(wpr2_end, wpr2_end, ullong, 0444);
module_param_named(wpr2_lo_raw, wpr2_lo_raw, uint, 0444);
module_param_named(wpr2_hi_raw, wpr2_hi_raw, uint, 0444);
module_param_named(matched_gpus, matched_gpus, uint, 0444);

static int __init unbound_wpr_init(void)
{
    struct pci_dev *pdev = NULL;
    struct pci_dev *selected = NULL;
    void __iomem *registers;
    resource_size_t page_start;
    resource_size_t page_offset;
    u32 low;
    u32 high;

    for_each_pci_dev(pdev) {
        if (pdev->vendor != UB_NVIDIA_VENDOR)
            continue;
        if ((pdev->class >> 16) != PCI_BASE_CLASS_DISPLAY)
            continue;
        matched_gpus++;
        selected = pdev;
    }

    if (matched_gpus != 1 || selected == NULL) {
        pr_err("unbound_wpr: expected one NVIDIA display GPU, found %u\n",
               matched_gpus);
        return -ENODEV;
    }
    if (!(pci_resource_flags(selected, 0) & IORESOURCE_MEM) ||
        pci_resource_len(selected, 0) <= UB_WPR2_ADDR_HI) {
        pr_err("unbound_wpr: BAR0 does not cover Blackwell HUBMMU registers\n");
        return -ENODEV;
    }

    page_start = UB_WPR2_ADDR_LO & PAGE_MASK;
    page_offset = UB_WPR2_ADDR_LO & ~PAGE_MASK;
    registers = ioremap(pci_resource_start(selected, 0) + page_start, PAGE_SIZE);
    if (registers == NULL)
        return -ENOMEM;

    low = readl(registers + page_offset);
    high = readl(registers + page_offset + sizeof(u32));
    iounmap(registers);
    wpr2_lo_raw = low;
    wpr2_hi_raw = high;

    wpr2_start = ((u64)(low >> UB_WPR_FIELD_SHIFT)) << UB_WPR_ADDR_SHIFT;
    wpr2_end = (((u64)(high >> UB_WPR_FIELD_SHIFT)) << UB_WPR_ADDR_SHIFT) +
               UB_WPR_BLOCK_SIZE;
    if (wpr2_start == 0 || wpr2_end <= wpr2_start) {
        pr_err("unbound_wpr: invalid WPR2 registers low=0x%08x high=0x%08x\n",
               low, high);
        return -EINVAL;
    }

    pr_info("unbound_wpr: %s WPR2 LO=0x%08x HI=0x%08x range=0x%llx..0x%llx (read-only)\n",
            pci_name(selected), low, high, wpr2_start, wpr2_end);
    return 0;
}

static void __exit unbound_wpr_exit(void)
{
}

module_init(unbound_wpr_init);
module_exit(unbound_wpr_exit);
MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("Read-only NVIDIA Blackwell GSP WPR2 discovery probe");
MODULE_VERSION("0.1.0");
