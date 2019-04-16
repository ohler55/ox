/* cache.h
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#ifndef OX_CACHE_H
#define OX_CACHE_H

#include "ruby.h"

typedef struct _cache   *Cache;

extern void     ox_cache_new(Cache *cache);

extern VALUE    ox_cache_get(Cache cache, const char *key, VALUE **slot, const char **keyp);

extern void     ox_cache_print(Cache cache);

#endif /* OX_CACHE_H */
