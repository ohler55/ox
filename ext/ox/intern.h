// Copyright (c) 2011, 2021 Peter Ohler. All rights reserved.
// Licensed under the MIT License. See LICENSE file in the project root for license details.
#ifndef OX_INTERN_H
#define OX_INTERN_H

#include <ruby.h>
#include <ruby/encoding.h>
#include <stdbool.h>

struct _parseInfo;

extern void ox_hash_init();

extern VALUE ox_str_intern(const char *key, size_t len, const char **keyp);
extern VALUE ox_sym_intern(const char *key, size_t len, const char **keyp);
extern ID    ox_attr_intern(const char *key, size_t len);
extern VALUE
ox_class_intern(const char *key, size_t len, bool safe, struct _parseInfo *pi, int auto_define, VALUE error_class);

extern char *ox_strndup(const char *s, size_t len);

extern VALUE ox_utf8_name(const char *str, size_t len, rb_encoding *encoding, const char **strp);
extern VALUE ox_enc_sym(const char *str, size_t len, rb_encoding *encoding, const char **strp);
extern VALUE ox_enc_name(const char *str, size_t len, rb_encoding *encoding, const char **strp);

#endif /* OX_INTERN_H */
