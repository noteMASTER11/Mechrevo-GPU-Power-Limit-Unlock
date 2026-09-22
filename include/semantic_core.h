/* SPDX-License-Identifier: MIT */
#ifndef CODEX_GSP_SEMANTIC_CORE_H
#define CODEX_GSP_SEMANTIC_CORE_H

#ifndef GS_EXTERNAL_TYPES
#include <stdint.h>
#endif

#define GS_POLICY_LIMIT 32U
#define GS_HEAP_LIMIT (256ULL * 1024ULL * 1024ULL)
#define GS_RAIL_WRITERS 6U

enum gs_status {
    GS_OK = 0,
    GS_INVALID = 1,
    GS_IO = 2,
    GS_NOT_FOUND = 3,
    GS_AMBIGUOUS = 4,
    GS_BUDGET = 5
};

struct gs_policy {
    uint8_t index, type, id, unit;
    uint32_t lower_mw, upper_mw;
};

struct gs_io {
    void *context;
    int (*read)(void *context, uint64_t offset, void *destination, uint32_t size);
    uint64_t heap_size;
};

struct gs_resolution {
    uint64_t va_base, policy_array, board_object, selector, lower_selector, ctgp_tuple;
    uint32_t index_member, type_member, id_member, unit_member;
    uint64_t board_max_effective, board_max_source, pmgr_upper;
    uint32_t stock_upper_mw, current_upper_mw;
    uint64_t base_internal, entry13_object, entry14_object;
    uint64_t entry13_writers[GS_RAIL_WRITERS];
    uint64_t entry14_writers[GS_RAIL_WRITERS];
    uint32_t stock_base_mw, current_base_mw;
    uint32_t stock_entry13, current_entry13;
    uint32_t stock_entry14, current_entry14;
    uint64_t current_selector, current_member;
    uint32_t stock_current_mw, current_current_mw;
};

/* Primary production path: no caller-supplied addresses, offsets, policy
 * records, stock limits or driver-version profile. */
int gs_resolve_autonomous(const struct gs_io *io, struct gs_resolution *out);

int gs_resolve(const struct gs_io *io, uint32_t public_mask,
               const struct gs_policy *policies, uint32_t policy_count,
               struct gs_resolution *out);

#endif
