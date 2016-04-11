
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "cache16.h"

struct _Cache16 {
    VALUE               value;
    struct _Cache16     *slots[16];
};

static void     slot_print(Cache16 cache, unsigned int depth);
static void     v2s(VALUE v, char *buf, unsigned long len);

void
ox_cache16_new(Cache16 *cache) {
    Cache16     *cp;
    int         i;
    
    if (0 == (*cache = (Cache16)malloc(sizeof(struct _Cache16)))) {
        rb_raise(rb_eStandardError, "not enough memory\n");
    }
    (*cache)->value = Qundef;
    for (i = 16, cp = (*cache)->slots; 0 < i; i--, cp++) {
        *cp = 0;
    }
}

VALUE
ox_cache16_get(Cache16 cache, const char *key, VALUE **slot) {
    unsigned char       *k = (unsigned char*)key;
    Cache16             *cp;

    for (; '\0' != *k; k++) {
        cp = cache->slots + (unsigned int)(*k >> 4); // upper 4 bits
        if (0 == *cp) {
            ox_cache16_new(cp);
        }
        cache = *cp;
        cp = cache->slots + (unsigned int)(*k & 0x0F); // lower 4 bits
        if (0 == *cp) {
            ox_cache16_new(cp);
        }
        cache = *cp;
    }
    *slot = &cache->value;

    return cache->value;
}

void
ox_cache16_print(Cache16 cache) {
    //printf("-------------------------------------------\n");
    slot_print(cache, 0);
}

static void
slot_print(Cache16 c, unsigned int depth) {
    char                indent[256];
    Cache16             *cp;
    unsigned int        i;

    if (sizeof(indent) - 1 < depth) {
        depth = ((int)sizeof(indent) - 1);
    }
    memset(indent, ' ', depth);
    indent[depth] = '\0';
    for (i = 0, cp = c->slots; i < 16; i++, cp++) {
        if (0 == *cp) {
            //printf("%s%02u:\n", indent, i);
        } else {
            if (Qundef == (*cp)->value) {
                printf("%s%02u:\n", indent, i);
            } else {
                char            value[1024];
                const char      *clas;

                if (Qundef == (*cp)->value) {
                    strcpy(value, "undefined");
                    clas = "";
                } else {
                    v2s((*cp)->value, value, sizeof(value));
                    clas = rb_class2name(rb_obj_class((*cp)->value));
                }
                printf("%s%02u: %s (%s)\n", indent, i, value, clas);
            }
            slot_print(*cp, depth + 2);
        }
    }
}

static void
v2s(VALUE v, char *buf, unsigned long len) {
    VALUE       rs = rb_funcall2(v, rb_intern("to_s"), 0, 0);

    snprintf(buf, len, "%s", StringValuePtr(rs));
}
