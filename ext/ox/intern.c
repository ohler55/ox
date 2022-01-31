// Copyright (c) 2011, 2021 Peter Ohler. All rights reserved.
// Licensed under the MIT License. See LICENSE file in the project root for license details.

#include "intern.h"

#include <stdint.h>

#if HAVE_PTHREAD_MUTEX_INIT
#include <pthread.h>
#endif

#include "cache.h"
#include "intern.h"
#include "ox.h"

// Only used for the class cache so 256 should be sufficient.
#define HASH_SLOT_CNT ((uint64_t)256)
#define HASH_MASK (HASH_SLOT_CNT - 1)

// almost the Murmur hash algorithm
#define M 0x5bd1e995

typedef struct _keyVal {
    struct _keyVal *next;
    const char *    key;
    size_t          len;
    VALUE           val;
} * KeyVal;

typedef struct _hash {
    struct _keyVal slots[HASH_SLOT_CNT];
#if HAVE_PTHREAD_MUTEX_INIT
    pthread_mutex_t mutex;
#else
    VALUE mutex;
#endif
} * Hash;

struct _hash class_hash;
struct _hash attr_hash;

static struct _cache *str_cache = NULL;
static VALUE          str_cache_obj;

static struct _cache *sym_cache = NULL;
static VALUE          sym_cache_obj;

static struct _cache *attr_cache = NULL;
static VALUE          attr_cache_obj;

static VALUE form_str(const char *str, size_t len) {
    return rb_str_freeze(rb_utf8_str_new(str, len));
}

static VALUE form_sym(const char *str, size_t len) {
    return rb_to_symbol(rb_str_freeze(rb_utf8_str_new(str, len)));
}

static VALUE form_attr(const char *str, size_t len) {
    char buf[256];

    if (sizeof(buf) - 2 <= len) {
        char *b = ALLOC_N(char, len + 2);
        ID    id;

        if ('~' == *str) {
            memcpy(b, str + 1, len - 1);
            b[len - 1] = '\0';
            len -= 2;
        } else {
            *b = '@';
            memcpy(b + 1, str, len);
            b[len + 1] = '\0';
        }
        id = rb_intern3(buf, len + 1, rb_utf8_encoding());
        xfree(b);
        return id;
    }
    if ('~' == *str) {
        memcpy(buf, str + 1, len - 1);
        buf[len - 1] = '\0';
        len -= 2;
    } else {
        *buf = '@';
        memcpy(buf + 1, str, len);
        buf[len + 1] = '\0';
    }
    return (VALUE)rb_intern3(buf, len + 1, rb_utf8_encoding());
}

void ox_hash_init() {
    VALUE cache_class = rb_define_class_under(Ox, "Cache", rb_cObject);

    str_cache     = cache_create(0, form_str, true, false);
    str_cache_obj = Data_Wrap_Struct(cache_class, cache_mark, cache_free, str_cache);
    rb_gc_register_address(&str_cache_obj);

    sym_cache     = cache_create(0, form_sym, true, false);
    sym_cache_obj = Data_Wrap_Struct(cache_class, cache_mark, cache_free, sym_cache);
    rb_gc_register_address(&sym_cache_obj);

    attr_cache     = cache_create(0, form_attr, false, false);
    attr_cache_obj = Data_Wrap_Struct(cache_class, cache_mark, cache_free, attr_cache);
    rb_gc_register_address(&attr_cache_obj);

    memset(class_hash.slots, 0, sizeof(class_hash.slots));
#if HAVE_PTHREAD_MUTEX_INIT
    pthread_mutex_init(&class_hash.mutex, NULL);
#else
    class_hash.mutex = rb_mutex_new();
    rb_gc_register_address(&class_hash.mutex);
#endif
}

VALUE
ox_str_intern(const char *key, size_t len, const char **keyp) {
    // For huge cache sizes over half a million the rb_enc_interned_str
    // performs slightly better but at more "normal" size of a several
    // thousands the cache intern performs about 20% better.
#if HAVE_RB_ENC_INTERNED_STR && 0
    return rb_enc_interned_str(key, len, rb_utf8_encoding());
#else
    return cache_intern(str_cache, key, len, keyp);
#endif
}

VALUE
ox_sym_intern(const char *key, size_t len, const char **keyp) {
    return cache_intern(sym_cache, key, len, keyp);
}

ID ox_attr_intern(const char *key, size_t len) {
    return cache_intern(attr_cache, key, len, NULL);
}

char *ox_strndup(const char *s, size_t len) {
    char *d = ALLOC_N(char, len + 1);

    memcpy(d, s, len);
    d[len] = '\0';

    return d;
}

void intern_cleanup() {
    cache_free(str_cache);
    cache_free(sym_cache);
    cache_free(attr_cache);
}

VALUE
ox_utf8_name(const char *str, size_t len, rb_encoding *encoding, const char **strp) {
    return ox_str_intern(str, len, strp);
}

VALUE
ox_utf8_sym(const char *str, size_t len, rb_encoding *encoding, const char **strp) {
    return ox_sym_intern(str, len, strp);
}

VALUE
ox_enc_sym(const char *str, size_t len, rb_encoding *encoding, const char **strp) {
    VALUE sym = rb_str_new2(str);

    rb_enc_associate(sym, encoding);
    if (NULL != strp) {
        *strp = StringValuePtr(sym);
    }
    return rb_to_symbol(sym);
}

VALUE
ox_enc_name(const char *str, size_t len, rb_encoding *encoding, const char **strp) {
    VALUE sym = rb_str_new2(str);

    rb_enc_associate(sym, encoding);
    if (NULL != strp) {
        *strp = StringValuePtr(sym);
    }
    return sym;
}
