/* SPDX-License-Identifier: MIT */
#include "unbound_core.h"

const struct ub_layout_profile ub_profile_gsp_615_71_v5 = {
    .name = "gsp-615.71-v5",
    .gpu_size = 0x2300,
    .gpu_self = 0xa0,
    .gpu_pmgr = 0x2210,
    .pmgr_size = 0x4f28,
    .pmgr_gpu = 0x60,
    .pmgr_self = 0x1b8,
    .pmgr_policy_array = 0x1cc0,
    .pmgr_policy_group = 0x1cc8,
    .pmgr_ctgp_supported = 0x3d08,
    .pmgr_ctgp_policy_index = 0x3d14,
    .pmgr_ctgp_lower = 0x3d18,
    .pmgr_ctgp_upper = 0x3d1c,
    .board_size = 0x340,
    .board_index = 2,
    .board_selector_mode = 0x104,
    .board_selector_count = 0x105,
    .board_effective_max = 0x108,
    .board_stock_max = 0x10c,
    .board_source_tag = 0x110,
    .board_source_max = 0x114,
    .board_current = 0x1f8,
};
