#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "base_tgp_wire.h"

int main(void)
{
    unsigned char actual[BASE_TGP_SIZE];
    unsigned char expected[BASE_TGP_SIZE];
    memset(expected, 0, sizeof(expected));
    expected[0x0c] = 0xff;
    base_tgp_put32(expected + 0x14U + 2U * 0xc4U + 4U, 100000U);
    base_tgp_prepare_set_policy2(actual, 100000U);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    assert(base_tgp_get32(actual + 0x0cU) == BASE_TGP_SET_HEADER);
    assert(base_tgp_get32(actual + 0x10U) == 0U);
    puts("base TGP SET wire test passed");
    return 0;
}
