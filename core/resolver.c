/* SPDX-License-Identifier: MIT */
#include "unbound_core.h"

static int ub_range(ub_u64 offset, ub_u64 length, ub_u64 size)
{
    return offset <= size && length <= size - offset;
}

static int ub_read(const struct ub_reader *r, ub_u64 offset, void *out, ub_u32 size)
{
    if (!ub_range(offset, size, r->size))
        return UB_RESOLVE_OVERFLOW;
    return r->read(r->context, offset, out, size) == 0 ? 0 : UB_RESOLVE_IO;
}

static int ub_u8_at(const struct ub_reader *r, ub_u64 o, ub_u8 *v)
{
    return ub_read(r, o, v, sizeof(*v));
}

static int ub_u16_at(const struct ub_reader *r, ub_u64 o, ub_u16 *v)
{
    return ub_read(r, o, v, sizeof(*v));
}

static int ub_u32_at(const struct ub_reader *r, ub_u64 o, ub_u32 *v)
{
    return ub_read(r, o, v, sizeof(*v));
}

static int ub_u64_at(const struct ub_reader *r, ub_u64 o, ub_u64 *v)
{
    return ub_read(r, o, v, sizeof(*v));
}

static int ub_pointer_offset(ub_u64 pointer, ub_u64 va_base, ub_u64 size,
                             ub_u64 object_size, ub_u64 *offset)
{
    if (pointer < va_base)
        return 0;
    *offset = pointer - va_base;
    return ub_range(*offset, object_size, size);
}

int ub_resolve_snapshot(const struct ub_reader *r,
                        const struct ub_layout_profile *p,
                        struct ub_resolved_snapshot *out)
{
    ub_u64 pmgr;
    ub_u32 count = 0;
    struct ub_resolved_snapshot found = {0};

    if (!r || !r->read || !p || !out || r->size < p->pmgr_size)
        return UB_RESOLVE_INVALID_ARGUMENT;

    for (pmgr = 0; ub_range(pmgr, p->pmgr_size, r->size); pmgr += 8) {
        ub_u64 self, va_base, gpu_ptr, gpu, array_ptr, array, group_ptr, group;
        ub_u64 board_ptr, board;
        ub_u8 supported, type, mode, selector_count, source;
        ub_u16 index;
        ub_u32 policy_index, lower, upper, mask, stock, effective, source_max, current;

        if (ub_u64_at(r, pmgr + p->pmgr_self, &self))
            return UB_RESOLVE_IO;
        if (self < pmgr)
            continue;
        va_base = self - pmgr;
        if ((va_base & 7) != 0)
            continue;

        if (ub_u64_at(r, pmgr + p->pmgr_gpu, &gpu_ptr) ||
            !ub_pointer_offset(gpu_ptr, va_base, r->size, p->gpu_size, &gpu))
            continue;
        if (ub_u64_at(r, gpu + p->gpu_self, &self) || self != va_base + gpu)
            continue;
        if (ub_u64_at(r, gpu + p->gpu_pmgr, &self) || self != va_base + pmgr)
            continue;

        if (ub_u8_at(r, pmgr + p->pmgr_ctgp_supported, &supported) || supported != 1 ||
            ub_u32_at(r, pmgr + p->pmgr_ctgp_policy_index, &policy_index) ||
            policy_index != p->board_index ||
            ub_u32_at(r, pmgr + p->pmgr_ctgp_lower, &lower) || lower != 80000 ||
            ub_u32_at(r, pmgr + p->pmgr_ctgp_upper, &upper))
            continue;

        if (ub_u64_at(r, pmgr + p->pmgr_policy_array, &array_ptr) ||
            !ub_pointer_offset(array_ptr, va_base, r->size, 256, &array) ||
            ub_u64_at(r, pmgr + p->pmgr_policy_group, &group_ptr) ||
            !ub_pointer_offset(group_ptr, va_base, r->size, 16, &group) ||
            ub_u32_at(r, group + 8, &mask) || !(mask & (1U << p->board_index)) ||
            ub_u64_at(r, array + (ub_u64)p->board_index * 8, &board_ptr) ||
            !ub_pointer_offset(board_ptr, va_base, r->size, p->board_size, &board))
            continue;

        if (ub_u8_at(r, board, &type) || type != 0 ||
            ub_u16_at(r, board + 2, &index) || index != p->board_index ||
            ub_u8_at(r, board + p->board_selector_mode, &mode) || mode != 0 ||
            ub_u8_at(r, board + p->board_selector_count, &selector_count) || selector_count != 1 ||
            ub_u8_at(r, board + p->board_source_tag, &source) || source != 0xfe ||
            ub_u32_at(r, board + p->board_stock_max, &stock) || stock != 175000 ||
            ub_u32_at(r, board + p->board_effective_max, &effective) ||
            ub_u32_at(r, board + p->board_source_max, &source_max) ||
            ub_u32_at(r, board + p->board_current, &current))
            continue;

        found = (struct ub_resolved_snapshot) {
            .heap_va_base = va_base,
            .gpu_offset = gpu,
            .pmgr_offset = pmgr,
            .policy_array_offset = array,
            .policy_group_offset = group,
            .board_offset = board,
            .policy_mask = mask,
            .stock_max_mw = stock,
            .lower_mw = lower,
            .upper_mw = upper,
            .effective_max_mw = effective,
            .source_max_mw = source_max,
            .current_mw = current,
        };
        count++;
        if (count > 1)
            break;
    }

    if (count == 0)
        return UB_RESOLVE_NOT_FOUND;
    if (count > 1)
        return UB_RESOLVE_AMBIGUOUS;
    found.candidate_count = count;
    *out = found;
    return UB_RESOLVE_OK;
}
