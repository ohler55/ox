/* cache_test.c
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

#include "cache.h"

static const char       *data[] = {
    "one",
    "two",
    "one",
    "onex",
    "oney",
    "one",
    0
};

void
ox_cache_test() {
    Cache       c;
    const char  **d;
    VALUE       v;
    VALUE       *slot = 0;;

    ox_cache_new(&c);
    for (d = data; 0 != *d; d++) {
        v = ox_cache_get(c, *d, &slot);
        if (Qundef == v) {
            if (0 == slot) {
                /*printf("*** failed to get a slot for %s\n", *d); */
            } else {
                /*printf("*** added '%s' to cache\n", *d); */
                v = ID2SYM(rb_intern(*d));
                *slot = v;
            }
        } else {
            VALUE       rs = rb_funcall2(v, rb_intern("to_s"), 0, 0);

            printf("*** get on '%s' returned '%s' (%s)\n", *d, StringValuePtr(rs), rb_class2name(rb_obj_class(v)));
        }
        /*ox_cache_print(c); */
    }
    ox_cache_print(c);
}
