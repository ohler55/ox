// Copyright (c) 2011, 2021 Peter Ohler. All rights reserved.
// Licensed under the MIT License. See LICENSE file in the project root for license details.

#if HAVE_PTHREAD_MUTEX_INIT
#include <pthread.h>
#endif
#include <stdlib.h>

#include "cache.h"

// The stdlib calloc, realloc, and free are used instead of the Ruby ALLOC,
// ALLOC_N, REALLOC, and xfree since the later could trigger a GC which will
// either corrupt memory or if the mark function locks will deadlock.

#define REHASH_LIMIT 4
#define MIN_SHIFT 8
#define REUSE_MAX 8192

#if HAVE_PTHREAD_MUTEX_INIT
#define CACHE_LOCK(c) pthread_mutex_lock(&((c)->mutex))
#define CACHE_UNLOCK(c) pthread_mutex_unlock(&((c)->mutex))
#else
#define CACHE_LOCK(c) rb_mutex_lock((c)->mutex)
#define CACHE_UNLOCK(c) rb_mutex_unlock((c)->mutex)
#endif

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND           \
    do {                   \
        v0 += v1;          \
        v1 = ROTL(v1, 13); \
        v1 ^= v0;          \
        v0 = ROTL(v0, 32); \
        v2 += v3;          \
        v3 = ROTL(v3, 16); \
        v3 ^= v2;          \
        v0 += v3;          \
        v3 = ROTL(v3, 21); \
        v3 ^= v0;          \
        v2 += v1;          \
        v1 = ROTL(v1, 17); \
        v1 ^= v2;          \
        v2 = ROTL(v2, 32); \
    } while (0)

static uint64_t hash_key[2];

void ox_hash_seed_init(void) {
    VALUE seed = rb_funcall(rb_cRandom, rb_intern("new_seed"), 0);
    VALUE mask = ULL2NUM(UINT64_MAX);

    hash_key[0] = NUM2ULL(rb_funcall(seed, rb_intern("&"), 1, mask)) ^ (uint64_t)rb_memhash("ox0", 3);
    hash_key[1] = NUM2ULL(rb_funcall(rb_funcall(seed, rb_intern(">>"), 1, INT2FIX(64)), rb_intern("&"), 1, mask)) ^
                  (uint64_t)rb_memhash("ox1", 3);
}

typedef struct _slot {
    struct _slot     *next;
    VALUE             val;
    uint64_t          hash;
    volatile uint32_t use_cnt;
    uint8_t           klen;
    char              key[CACHE_MAX_KEY];
} *Slot;

typedef struct _cache {
    volatile Slot  *slots;
    volatile size_t cnt;
    VALUE (*form)(const char *str, size_t len);
    uint64_t size;
    uint64_t mask;
    VALUE (*intern)(struct _cache *c, const char *key, size_t len, const char **keyp);
    volatile Slot reuse;
    size_t        rcnt;
#if HAVE_PTHREAD_MUTEX_INIT
    pthread_mutex_t mutex;
#else
    VALUE mutex;
#endif
    uint8_t xrate;
    bool    mark;
} *Cache;

const rb_data_type_t ox_cache_type = {
    "Ox/Cache",
    {
        ox_cache_mark,
        ox_cache_free,
        NULL,
    },
    0,
    0,
};

static uint64_t hash_calc(const uint8_t *key, size_t len) {
    const uint8_t *end = key + (len & ~(size_t)7);
    uint64_t       v0  = 0x736f6d6570736575ULL ^ hash_key[0];
    uint64_t       v1  = 0x646f72616e646f6dULL ^ hash_key[1];
    uint64_t       v2  = 0x6c7967656e657261ULL ^ hash_key[0];
    uint64_t       v3  = 0x7465646279746573ULL ^ hash_key[1];
    uint64_t       b   = (uint64_t)len << 56;
    uint64_t       m;
    size_t         i;

    for (; key < end; key += 8) {
        m = 0;
        for (i = 0; i < 8; i++) {
            m |= (uint64_t)key[i] << (8 * i);
        }
        v3 ^= m;
        SIPROUND;
        v0 ^= m;
    }
    for (i = 0; i < (len & 7); i++) {
        b |= (uint64_t)key[i] << (8 * i);
    }
    v3 ^= b;
    SIPROUND;
    v0 ^= b;
    v2 ^= 0xff;
    SIPROUND;
    SIPROUND;
    SIPROUND;

    return v0 ^ v1 ^ v2 ^ v3;
}

static void rehash(Cache c) {
    uint64_t osize = c->size;
    Slot    *slots;
    Slot    *end;
    Slot    *sp;

    // Growing is only an optimization, and rehash() runs with the lock held, so
    // on failure keep the current size rather than raising.
    if (NULL == (slots = realloc((void *)c->slots, sizeof(Slot) * osize * 4))) {
        return;
    }
    c->size  = osize * 4;
    c->mask  = c->size - 1;
    c->slots = slots;
    memset((Slot *)c->slots + osize, 0, sizeof(Slot) * osize * 3);
    end = (Slot *)c->slots + osize;
    for (sp = (Slot *)c->slots; sp < end; sp++) {
        Slot s    = *sp;
        Slot next = NULL;

        *sp = NULL;
        for (; NULL != s; s = next) {
            uint64_t h      = s->hash & c->mask;
            Slot    *bucket = (Slot *)c->slots + h;

            next    = s->next;
            s->next = *bucket;
            *bucket = s;
        }
    }
}

static VALUE ox_lockless_intern(Cache c, const char *key, size_t len, const char **keyp) {
    uint64_t       h      = hash_calc((const uint8_t *)key, len);
    Slot          *bucket = (Slot *)c->slots + (h & c->mask);
    Slot           b;
    volatile VALUE rkey;

    while (REUSE_MAX < c->rcnt) {
        if (NULL != (b = c->reuse)) {
            c->reuse = b->next;
            free(b);
            c->rcnt--;
        } else {
            // An accounting error occured somewhere so correct it.
            c->rcnt = 0;
        }
    }
    for (b = *bucket; NULL != b; b = b->next) {
        if ((uint8_t)len == b->klen && 0 == strncmp(b->key, key, len)) {
            b->use_cnt += 16;
            if (NULL != keyp) {
                *keyp = b->key;
            }
            return b->val;
        }
    }
    rkey = c->form(key, len);
    if (NULL == (b = c->reuse)) {
        if (NULL == (b = calloc(1, sizeof(struct _slot)))) {
            // Nothing modified yet and no lock held, so raising is safe.
            rb_memerror();
        }
    } else {
        c->reuse = b->next;
        c->rcnt--;
    }
    b->hash = h;
    memcpy(b->key, key, len);
    b->klen     = (uint8_t)len;
    b->key[len] = '\0';
    b->val      = rkey;
    b->use_cnt  = 4;
    b->next     = *bucket;
    *bucket     = b;
    c->cnt++;  // Don't worry about wrapping. Worse case is the entry is removed and recreated.
    if (NULL != keyp) {
        *keyp = b->key;
    }
    if (REHASH_LIMIT < c->cnt / c->size) {
        rehash(c);
    }
    return rkey;
}

static VALUE ox_locking_intern(Cache c, const char *key, size_t len, const char **keyp) {
    uint64_t       h;
    Slot          *bucket;
    Slot           b;
    uint64_t       old_size;
    volatile VALUE rkey;

    CACHE_LOCK(c);
    while (REUSE_MAX < c->rcnt) {
        if (NULL != (b = c->reuse)) {
            c->reuse = b->next;
            free(b);
            c->rcnt--;
        } else {
            // An accounting error occured somewhere so correct it.
            c->rcnt = 0;
        }
    }
    h      = hash_calc((const uint8_t *)key, len);
    bucket = (Slot *)c->slots + (h & c->mask);
    for (b = *bucket; NULL != b; b = b->next) {
        if ((uint8_t)len == b->klen && 0 == strncmp(b->key, key, len)) {
            b->use_cnt += 4;
            if (NULL != keyp) {
                *keyp = b->key;
            }
            CACHE_UNLOCK(c);

            return b->val;
        }
    }
    old_size = c->size;
    // The creation of a new value may trigger a GC which be a problem if the
    // cache is locked so make sure it is unlocked for the key value creation.
    if (NULL != (b = c->reuse)) {
        c->reuse = b->next;
        c->rcnt--;
    }
    CACHE_UNLOCK(c);
    if (NULL == b && NULL == (b = calloc(1, sizeof(struct _slot)))) {
        rb_memerror();  // the lock was released above
    }
    rkey    = c->form(key, len);
    b->hash = h;
    memcpy(b->key, key, len);
    b->klen     = (uint8_t)len;
    b->key[len] = '\0';
    b->val      = rkey;
    b->use_cnt  = 16;

    // Lock again to add the new entry.
    CACHE_LOCK(c);
    if (old_size != c->size) {
        h      = hash_calc((const uint8_t *)key, len);
        bucket = (Slot *)c->slots + (h & c->mask);
    }
    b->next = *bucket;
    *bucket = b;
    c->cnt++;  // Don't worry about wrapping. Worse case is the entry is removed and recreated.
    if (NULL != keyp) {
        *keyp = b->key;
    }
    if (REHASH_LIMIT < c->cnt / c->size) {
        rehash(c);
    }
    CACHE_UNLOCK(c);

    return rkey;
}

Cache ox_cache_create(size_t size, VALUE (*form)(const char *str, size_t len), bool mark, bool locking) {
    Cache c     = calloc(1, sizeof(struct _cache));
    int   shift = 0;

    if (NULL == c) {
        rb_memerror();
    }
    for (; REHASH_LIMIT < size; size /= 2, shift++) {
    }
    if (shift < MIN_SHIFT) {
        shift = MIN_SHIFT;
    }
#if HAVE_PTHREAD_MUTEX_INIT
    pthread_mutex_init(&c->mutex, NULL);
#else
    c->mutex = rb_mutex_new();
#endif
    c->size = 1 << shift;
    c->mask = c->size - 1;
    if (NULL == (c->slots = calloc(c->size, sizeof(Slot)))) {
        free(c);
        rb_memerror();
    }
    c->form  = form;
    c->xrate = 1;  // low
    c->mark  = mark;
    if (locking) {
        c->intern = ox_locking_intern;
    } else {
        c->intern = ox_lockless_intern;
    }
    return c;
}

void ox_cache_free(void *ptr) {
    Cache    c = (Cache)ptr;
    uint64_t i;
    Slot     next;
    Slot     s;

    for (i = 0; i < c->size; i++) {
        for (s = c->slots[i]; NULL != s; s = next) {
            next = s->next;
            free(s);
        }
    }
    // ox_cache_mark retires unused slots onto the reuse list, off the buckets.
    for (s = c->reuse; NULL != s; s = next) {
        next = s->next;
        free(s);
    }
    free((void *)c->slots);
    free(c);
}

void ox_cache_mark(void *ptr) {
    Cache    c = (Cache)ptr;
    uint64_t i;

#if !HAVE_PTHREAD_MUTEX_INIT
    rb_gc_mark(c->mutex);
#endif
    if (0 == c->cnt) {
        return;
    }
    for (i = 0; i < c->size; i++) {
        Slot s;
        Slot prev = NULL;
        Slot next;

        for (s = c->slots[i]; NULL != s; s = next) {
            next = s->next;
            if (0 == s->use_cnt) {
                if (NULL == prev) {
                    c->slots[i] = next;
                } else {
                    prev->next = next;
                }
                c->cnt--;
                s->next  = c->reuse;
                c->reuse = s;
                c->rcnt++;
                continue;
            }
            switch (c->xrate) {
            case 0: break;
            case 2: s->use_cnt -= 2; break;
            case 3: s->use_cnt /= 2; break;
            default: s->use_cnt--; break;
            }
            if (c->mark) {
                rb_gc_mark(s->val);
            }
            prev = s;
        }
    }
}

VALUE
ox_cache_intern(Cache c, const char *key, size_t len, const char **keyp) {
    if (CACHE_MAX_KEY <= len) {
        if (NULL != keyp) {
            volatile VALUE rkey = c->form(key, len);

            if (SYMBOL_P(rkey)) {
                *keyp = rb_id2name(rb_sym2id(rkey));
            }
            return rkey;
        }
        return c->form(key, len);
    }
    return c->intern(c, key, len, keyp);
}
