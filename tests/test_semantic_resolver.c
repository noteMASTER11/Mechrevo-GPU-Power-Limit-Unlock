/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic_core.h"

#define HEAP_SIZE 0x20000U
#define ARRAY     0x1000U
#define BOARD     0x8000U
#define SELECTOR  (BOARD + 0x300U)
#define LOWER     (BOARD + 0x200U)
#define CTGP      0x18000U

struct memory { unsigned char bytes[HEAP_SIZE]; };

static int read_memory(void *opaque, uint64_t offset, void *out, uint32_t size)
{
    struct memory *m = opaque;
    if (offset > HEAP_SIZE || size > HEAP_SIZE - offset)
        return 1;
    memcpy(out, m->bytes + offset, size);
    return 0;
}

static void put8(struct memory *m, size_t o, uint8_t v) { memcpy(m->bytes + o, &v, 1); }
static void put32(struct memory *m, size_t o, uint32_t v) { memcpy(m->bytes + o, &v, 4); }
static void put64(struct memory *m, size_t o, uint64_t v) { memcpy(m->bytes + o, &v, 8); }

static void selector(struct memory *m, size_t o, uint32_t effective, uint32_t stock)
{
    put8(m, o, 0);
    put8(m, o + 1, 1);
    put32(m, o + 4, effective);
    put32(m, o + 8, stock);
    put8(m, o + 12, 0xfe);
    put32(m, o + 16, effective);
}

static void fixture(struct memory *m, uint64_t va)
{
    unsigned i;
    memset(m, 0, sizeof(*m));
    for (i = 0; i < 8; i++) {
        size_t object = 0x4000U + i * 0x2000U;
        put64(m, ARRAY + i * 8U, va + object);
        put8(m, object + 2, (uint8_t)i);
    }
    selector(m, LOWER, 80000, 80000);
    selector(m, SELECTOR, 175000, 175000);
    put32(m, CTGP, 2);
    put32(m, CTGP + 4, 80000);
    put32(m, CTGP + 8, 175000);
}

int main(void)
{
    struct memory *m = calloc(1, sizeof(*m));
    struct gs_io io = { m, read_memory, HEAP_SIZE };
    struct gs_resolution first, moved, elevated;
    uint64_t new_va = 0x912340000ULL;
    unsigned i;

    assert(m != NULL);
    fixture(m, 0x7f2000000ULL);
    assert(gs_resolve_autonomous(&io, &first) == GS_OK);
    assert(first.va_base == 0x7f2000000ULL);
    assert(first.policy_array == ARRAY);
    assert(first.board_object == BOARD);
    assert(first.selector == SELECTOR);
    assert(first.lower_selector == LOWER);
    assert(first.ctgp_tuple == CTGP);
    assert(first.index_member == 2);
    assert(first.board_max_effective == SELECTOR + 4);
    assert(first.board_max_source == SELECTOR + 16);
    assert(first.pmgr_upper == CTGP + 8);

    for (i = 0; i < 8; i++)
        put64(m, ARRAY + i * 8U, new_va + 0x4000U + i * 0x2000U);
    assert(gs_resolve_autonomous(&io, &moved) == GS_OK);
    assert(moved.va_base == new_va);
    assert(moved.board_object == first.board_object);

    put32(m, SELECTOR + 4, 250000);
    put32(m, SELECTOR + 16, 250000);
    put32(m, CTGP + 8, 250000);
    assert(gs_resolve_autonomous(&io, &elevated) == GS_OK);
    assert(elevated.current_upper_mw == 250000);
    assert(elevated.stock_upper_mw == 175000);

    memcpy(m->bytes + SELECTOR + 0x40, m->bytes + SELECTOR, 20);
    assert(gs_resolve_autonomous(&io, &elevated) == GS_AMBIGUOUS);

    fixture(m, 0x7f2000000ULL);
    put8(m, SELECTOR + 12, 0);
    assert(gs_resolve_autonomous(&io, &elevated) == GS_NOT_FOUND);

    free(m);
    puts("semantic resolver tests passed");
    return 0;
}
