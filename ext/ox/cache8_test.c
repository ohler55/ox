/* cache8_test.c
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *  - Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 *  - Neither the name of Peter Ohler nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
                printf("*** failed to get a slot for 0x%016llx\n", *d);
            } else {
                printf("*** adding 0x%016llx to cache with value %llu\n", *d, cnt);
                *slot = cnt++;
            }
        } else {
            printf("*** get on 0x%016llx returned %llu\n", *d, v);
        }
        /*ox_cache8_print(c); */
    }
    ox_cache8_print(c);
}
