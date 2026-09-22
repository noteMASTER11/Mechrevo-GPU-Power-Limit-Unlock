/* SPDX-License-Identifier: MIT
 * PMGR base-TGP wire helpers reconstructed from libnvidia-ml.so.615.71.09.
 * GET and SET use the same total size and entry stride but different headers.
 */
#ifndef MECHREVO_BASE_TGP_WIRE_H
#define MECHREVO_BASE_TGP_WIRE_H

#include <stdint.h>
#include <string.h>

#define BASE_TGP_GET 0x2080a61aU
#define BASE_TGP_SET 0x2080e61bU
#define BASE_TGP_SIZE 0x3634U
#define BASE_TGP_FULL_MASK ((1U << 2) | (1U << 13) | (1U << 14))
#define BASE_TGP_SET_HEADER 0x000000ffU

static inline uint32_t base_tgp_get32(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline void base_tgp_put32(unsigned char *p, uint32_t v)
{
    memcpy(p, &v, sizeof(v));
}

static inline unsigned char *base_tgp_entry(unsigned char *p, unsigned index)
{
    return p + 0x14U + index * 0xc4U;
}

static inline void base_tgp_prepare_get(unsigned char *p)
{
    memset(p, 0, BASE_TGP_SIZE);
    base_tgp_put32(p + 0x10U, BASE_TGP_FULL_MASK);
}

static inline void base_tgp_prepare_set_policy2(unsigned char *p, uint32_t target_mw)
{
    unsigned char *entry;
    memset(p, 0, BASE_TGP_SIZE);
    base_tgp_put32(p + 0x0cU, BASE_TGP_SET_HEADER);
    entry = base_tgp_entry(p, 2);
    base_tgp_put32(entry, 0U);
    base_tgp_put32(entry + 4U, target_mw);
}

#endif
