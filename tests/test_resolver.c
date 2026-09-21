/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unbound_core.h"

struct memory_reader {
    unsigned char *bytes;
    size_t size;
};

static int read_memory(void *context, ub_u64 offset, void *destination, ub_u32 size)
{
    struct memory_reader *memory = context;
    if (offset > memory->size || size > memory->size - offset)
        return -1;
    memcpy(destination, memory->bytes + offset, size);
    return 0;
}

static void put8(unsigned char *p, size_t o, uint8_t v) { memcpy(p + o, &v, 1); }
static void put16(unsigned char *p, size_t o, uint16_t v) { memcpy(p + o, &v, 2); }
static void put32(unsigned char *p, size_t o, uint32_t v) { memcpy(p + o, &v, 4); }
static void put64(unsigned char *p, size_t o, uint64_t v) { memcpy(p + o, &v, 8); }

static void fixture(unsigned char *bytes, uint64_t va, size_t shift)
{
    const struct ub_layout_profile *p = &ub_profile_gsp_615_71_v5;
    size_t gpu = shift + 0x1000;
    size_t pmgr = shift + 0x4000;
    size_t board = shift + 0xa000;
    size_t array = shift + 0xb000;
    size_t group = shift + 0xc000;

    put64(bytes, gpu + p->gpu_self, va + gpu);
    put64(bytes, gpu + p->gpu_pmgr, va + pmgr);
    put64(bytes, pmgr + p->pmgr_gpu, va + gpu);
    put64(bytes, pmgr + p->pmgr_self, va + pmgr);
    put64(bytes, pmgr + p->pmgr_policy_array, va + array);
    put64(bytes, pmgr + p->pmgr_policy_group, va + group);
    put8(bytes, pmgr + p->pmgr_ctgp_supported, 1);
    put32(bytes, pmgr + p->pmgr_ctgp_policy_index, 2);
    put32(bytes, pmgr + p->pmgr_ctgp_lower, 80000);
    put32(bytes, pmgr + p->pmgr_ctgp_upper, 225000);
    put64(bytes, array + 16, va + board);
    put32(bytes, group + 8, 4);
    put8(bytes, board, 0);
    put16(bytes, board + 2, 2);
    put8(bytes, board + p->board_selector_mode, 0);
    put8(bytes, board + p->board_selector_count, 1);
    put32(bytes, board + p->board_effective_max, 225000);
    put32(bytes, board + p->board_stock_max, 175000);
    put8(bytes, board + p->board_source_tag, 0xfe);
    put32(bytes, board + p->board_source_max, 225000);
    put32(bytes, board + p->board_current, 225000);
}

int main(void)
{
    const size_t size = 0x30000;
    struct memory_reader memory = { calloc(1, size), size };
    struct ub_reader reader = { &memory, read_memory, size };
    struct ub_resolved_snapshot snapshot;

    assert(memory.bytes != NULL);
    fixture(memory.bytes, 0x912340000ULL, 0);
    assert(ub_resolve_snapshot(&reader, &ub_profile_gsp_615_71_v5, &snapshot) == 0);
    assert(snapshot.heap_va_base == 0x912340000ULL);
    assert(snapshot.gpu_offset == 0x1000);
    assert(snapshot.pmgr_offset == 0x4000);
    assert(snapshot.board_offset == 0xa000);
    assert(snapshot.upper_mw == 225000);

    fixture(memory.bytes, 0xa12340000ULL, 0x18000);
    assert(ub_resolve_snapshot(&reader, &ub_profile_gsp_615_71_v5, &snapshot) ==
           UB_RESOLVE_AMBIGUOUS);

    memset(memory.bytes, 0, size);
    fixture(memory.bytes, 0xb12340000ULL, 0);
    put8(memory.bytes, 0xa000 + ub_profile_gsp_615_71_v5.board_source_tag, 0);
    assert(ub_resolve_snapshot(&reader, &ub_profile_gsp_615_71_v5, &snapshot) ==
           UB_RESOLVE_NOT_FOUND);

    free(memory.bytes);
    puts("resolver tests passed");
    return 0;
}
