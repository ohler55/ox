/* cache8_test.c
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#include <stdio.h>
#include "cache8.h"

static slot_t data[] = {
    0x000000A0A0A0A0A0ULL,
    0x0000000000ABCDEFULL,
    0x0123456789ABCDEFULL,
    0x0000000000000001ULL,
    0x0000000000000002ULL,
    0x0000000000000003ULL,
    0x0000000000000004ULL,
    0
};

void
ox_cache8_test() {
    Cache8      c;
    slot_t      v;
    slot_t      *d;
    slot_t      cnt = 1;
    slot_t      *slot = 0;

    ox_cache8_new(&c);
    for (d = data; 0 != *d; d++) {
        v = ox_cache8_get(c, *d, &slot);
        if (0 == v) {
            if (0 == slot) {
                printf("*** failed to get a slot for 0x%016llx\n", (unsigned long long)*d);
            } else {
                printf("*** adding 0x%016llx to cache with value %llu\n", (unsigned long long)*d, (unsigned long long)cnt);
                *slot = cnt++;
            }
        } else {
            printf("*** get on 0x%016llx returned %llu\n", (unsigned long long)*d, (unsigned long long)v);
        }
        /*ox_cache8_print(c); */
    }
    ox_cache8_print(c);
}
