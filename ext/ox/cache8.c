
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "ruby.h"
#include "cache8.h"

#define BITS            4
#define MASK            0x000000000000000F
#define SLOT_CNT        16
#define DEPTH           16

struct _Cache8 {
    union {
        struct _Cache8 *slots[SLOT_CNT];
        unsigned long   values[SLOT_CNT];
    };
};

static void     slot_print(Cache8 cache, VALUE key, unsigned int depth);

void
ox_cache8_new(Cache8 *cache) {
    Cache8     *cp;
    int         i;
    
    if (0 == (*cache = (Cache8)malloc(sizeof(struct _Cache8)))) {
        rb_raise(rb_eStandardError, "not enough memory\n");
    }
    for (i = SLOT_CNT, cp = (*cache)->slots; 0 < i; i--, cp++) {
        *cp = 0;
    }
}

unsigned long
ox_cache8_get(Cache8 cache, VALUE key, unsigned long **slot) {
    Cache8      *cp;
    int         i;
    VALUE       k;
    
    for (i = 64 - BITS; 0 < i; i -= BITS) {
        k = (key >> i) & MASK;
        cp = cache->slots + k;
        if (0 == *cp) {
            ox_cache8_new(cp);
        }
        cache = *cp;
    }
    *slot = cache->values + (key & MASK);

    return **slot;
}

void
ox_cache8_print(Cache8 cache) {
    //printf("-------------------------------------------\n");
    slot_print(cache, 0, 0);
}

static void
slot_print(Cache8 c, VALUE key, unsigned int depth) {
    Cache8              *cp;
    unsigned int        i;
    unsigned long       k;

    for (i = 0, cp = c->slots; i < SLOT_CNT; i++, cp++) {
        if (0 != *cp) {
            k = (key << BITS) | i;
            //printf("*** key: 0x%016lx  depth: %u  i: %u\n", k, depth, i);
            if (DEPTH - 1 == depth) {
                printf("0x%016lx: %4lu\n", k, (unsigned long)*cp);
            } else {
                slot_print(*cp, k, depth + 1);
            }
        }
    }
}
