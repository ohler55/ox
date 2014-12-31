/* cache_test.c
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#include "cache.h"

static const char       *data[] = {
#if 1
    "one",
    "two",
    "one",
    "onex",
    "oney",
    "one",
    "tw",
    "onexyzabcdefgh",
#else
    "abc",
    "abcd",
    "ab",
    "a",
    "abcdefghijklmnop",
#endif
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
	/*printf("*** cache_get on %s\n", *d);*/
        v = ox_cache_get(c, *d, &slot, 0);
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
        /*ox_cache_print(c);*/
    }
    ox_cache_print(c);
}
