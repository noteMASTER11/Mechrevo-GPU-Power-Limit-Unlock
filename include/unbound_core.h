/* SPDX-License-Identifier: MIT */
#ifndef UNBOUND_CORE_H
#define UNBOUND_CORE_H

#ifdef __KERNEL__
#include <linux/types.h>
typedef u8 ub_u8;
typedef u16 ub_u16;
typedef u32 ub_u32;
typedef u64 ub_u64;
#else
#include <stdint.h>
typedef uint8_t ub_u8;
typedef uint16_t ub_u16;
typedef uint32_t ub_u32;
typedef uint64_t ub_u64;
#endif

enum ub_resolve_status {
    UB_RESOLVE_OK = 0,
    UB_RESOLVE_INVALID_ARGUMENT = -1,
    UB_RESOLVE_IO = -2,
    UB_RESOLVE_NOT_FOUND = -3,
    UB_RESOLVE_AMBIGUOUS = -4,
    UB_RESOLVE_OVERFLOW = -5,
};

struct ub_reader {
    void *context;
    int (*read)(void *context, ub_u64 offset, void *destination, ub_u32 size);
    ub_u64 size;
};

/* Relative member locations are a versioned schema, never live addresses. */
struct ub_layout_profile {
    const char *name;
    ub_u32 gpu_size;
    ub_u32 gpu_self;
    ub_u32 gpu_pmgr;
    ub_u32 pmgr_size;
    ub_u32 pmgr_gpu;
    ub_u32 pmgr_self;
    ub_u32 pmgr_policy_array;
    ub_u32 pmgr_policy_group;
    ub_u32 pmgr_ctgp_supported;
    ub_u32 pmgr_ctgp_policy_index;
    ub_u32 pmgr_ctgp_lower;
    ub_u32 pmgr_ctgp_upper;
    ub_u32 board_size;
    ub_u32 board_index;
    ub_u32 board_selector_mode;
    ub_u32 board_selector_count;
    ub_u32 board_effective_max;
    ub_u32 board_stock_max;
    ub_u32 board_source_tag;
    ub_u32 board_source_max;
    ub_u32 board_current;
};

struct ub_resolved_snapshot {
    ub_u64 heap_va_base;
    ub_u64 gpu_offset;
    ub_u64 pmgr_offset;
    ub_u64 policy_array_offset;
    ub_u64 policy_group_offset;
    ub_u64 board_offset;
    ub_u32 policy_mask;
    ub_u32 stock_max_mw;
    ub_u32 lower_mw;
    ub_u32 upper_mw;
    ub_u32 effective_max_mw;
    ub_u32 source_max_mw;
    ub_u32 current_mw;
    ub_u32 candidate_count;
};

extern const struct ub_layout_profile ub_profile_gsp_615_71_v5;

int ub_resolve_snapshot(const struct ub_reader *reader,
                        const struct ub_layout_profile *profile,
                        struct ub_resolved_snapshot *snapshot);

#endif
