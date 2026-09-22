/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic_core.h"

#define HEAP_SIZE 0x40000U
#define ARRAY 0x1000U
#define OBJECT(i) (0x4000U + (i) * 0x1000U)
#define BOARD OBJECT(2)
#define CURRENT (BOARD + 0x100U)
#define BASE (BOARD + 0x400U)
#define LOWER (BOARD + 0x500U)
#define UPPER (BOARD + 0x600U)
#define CTGP 0x30000U

static const size_t rail_selectors[3] = {0xb4U, 0x104U, 0x1f4U};
struct memory { unsigned char bytes[HEAP_SIZE]; };

static int read_memory(void *opaque, uint64_t offset, void *out, uint32_t size)
{
    struct memory *m = opaque;
    if (offset > HEAP_SIZE || size > HEAP_SIZE - offset) return 1;
    memcpy(out, m->bytes + offset, size);
    return 0;
}
static void put8(struct memory *m, size_t o, uint8_t v) { memcpy(m->bytes + o, &v, 1); }
static void put32(struct memory *m, size_t o, uint32_t v) { memcpy(m->bytes + o, &v, 4); }
static void put64(struct memory *m, size_t o, uint64_t v) { memcpy(m->bytes + o, &v, 8); }

static void selector(struct memory *m, size_t o, uint8_t count,
                     uint32_t effective, uint32_t reference, uint32_t source_value)
{
    put8(m, o, 0); put8(m, o + 1, count);
    put32(m, o + 4, effective); put32(m, o + 8, reference);
    put8(m, o + 12, 0xfe); put32(m, o + 16, source_value);
}
static void rail(struct memory *m, unsigned index, uint8_t id, uint32_t value)
{
    size_t object = OBJECT(index);
    put8(m, object + 0x20, 18); put8(m, object + 0x21, id);
    for (unsigned i = 0; i < 3; i++)
        selector(m, object + rail_selectors[i], 1, value,
                 index == 13 ? 210000 : 60000, value);
}
static void fixture(struct memory *m, uint64_t va)
{
    memset(m, 0, sizeof(*m));
    for (unsigned i = 0; i < 15; i++) {
        put64(m, ARRAY + i * 8U, va + OBJECT(i));
        put8(m, OBJECT(i) + 2, (uint8_t)i);
    }
    selector(m, CURRENT, 2, 80000, 80000, 80000);
    put32(m, BASE, 150000);
    selector(m, LOWER, 1, 80000, 80000, 80000);
    selector(m, UPPER, 1, 175000, 175000, 175000);
    rail(m, 13, 27, 210000); rail(m, 14, 28, 60000);
    put32(m, CTGP, 2); put32(m, CTGP + 4, 80000); put32(m, CTGP + 8, 175000);
}
static void set_rail(struct memory *m, unsigned index, uint32_t value)
{
    size_t object = OBJECT(index);
    for (unsigned i = 0; i < 3; i++) {
        put32(m, object + rail_selectors[i] + 4, value);
        put32(m, object + rail_selectors[i] + 16, value);
    }
}

int main(void)
{
    struct memory *m = calloc(1, sizeof(*m));
    struct gs_io io = {m, read_memory, HEAP_SIZE};
    struct gs_resolution first, moved, elevated, active;
    uint64_t new_va = 0x912340000ULL;

    assert(m != NULL);
    fixture(m, 0x7f2000000ULL);
    assert(gs_resolve_autonomous(&io, &first) == GS_OK);
    assert(first.va_base == 0x7f2000000ULL && first.policy_array == ARRAY);
    assert(first.board_object == BOARD && first.selector == UPPER);
    assert(first.lower_selector == LOWER && first.ctgp_tuple == CTGP);
    assert(first.index_member == 2 && first.board_max_effective == UPPER + 4);
    assert(first.board_max_source == UPPER + 16 && first.pmgr_upper == CTGP + 8);
    assert(first.base_internal == BASE && first.current_selector == CURRENT);
    assert(first.current_current_mw == 80000);

    for (unsigned i = 0; i < 15; i++) put64(m, ARRAY + i * 8U, new_va + OBJECT(i));
    assert(gs_resolve_autonomous(&io, &moved) == GS_OK);
    assert(moved.va_base == new_va && moved.board_object == first.board_object);

    put32(m, UPPER + 4, 250000); put32(m, UPPER + 16, 250000);
    put32(m, CTGP + 8, 250000); put32(m, BASE, 225000);
    set_rail(m, 13, 250000); set_rail(m, 14, 100000);
    assert(gs_resolve_autonomous(&io, &elevated) == GS_OK);
    assert(elevated.current_upper_mw == 250000 && elevated.current_base_mw == 225000);
    assert(elevated.current_entry13 == 250000 && elevated.current_entry14 == 100000);

    selector(m, CURRENT, 3, 225000, 80000, UINT32_MAX);
    assert(gs_resolve_autonomous(&io, &active) == GS_OK);
    assert(active.current_current_mw == 225000);

    memcpy(m->bytes + UPPER + 0x40, m->bytes + UPPER, 20);
    assert(gs_resolve_autonomous(&io, &active) == GS_AMBIGUOUS);

    fixture(m, 0x7f2000000ULL); put8(m, UPPER + 12, 0);
    assert(gs_resolve_autonomous(&io, &active) == GS_NOT_FOUND);

    free(m);
    puts("semantic resolver tests passed");
    return 0;
}
