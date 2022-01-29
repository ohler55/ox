/* sax.h
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#ifndef OX_SAX_H
#define OX_SAX_H

#include <stdbool.h>

#include "ox.h"
#include "sax_buf.h"
#include "sax_has.h"
#include "sax_hint.h"
#include "sax_stack.h"

typedef struct _saxOptions {
    int      symbolize;
    int      convert_special;
    int      smart;
    SkipMode skip;
    char     strip_ns[64];
    Hints    hints;
} * SaxOptions;

typedef struct _saxDrive {
    struct _buf        buf;
    struct _nStack     stack; /* element name stack */
    VALUE              handler;
    VALUE              value_obj;
    struct _saxOptions options;
    VALUE (*get_name)(const char *name, size_t len, rb_encoding *encoding, const char **namep);
    void (*set_pos)(VALUE handler, long pos);
    void (*set_line)(VALUE handler, long line);
    void (*set_col)(VALUE handler, long col);

    struct _has has;
    rb_encoding *encoding;

    int         err;
    int         blocked;
    bool        abort;
    bool utf8;
} * SaxDrive;

extern void ox_collapse_return(char *str);
extern void ox_sax_parse(VALUE handler, VALUE io, SaxOptions options);
extern void ox_sax_drive_cleanup(SaxDrive dr);
extern void ox_sax_drive_error(SaxDrive dr, const char *msg);
extern int  ox_sax_collapse_special(SaxDrive dr, char *str, long pos, long line, long col);

extern VALUE ox_sax_value_class;

extern VALUE str2sym(SaxDrive dr, const char *str, size_t len, const char **strp);
//extern VALUE str2sym(SaxDrive dr, const char *str, const char **strp);

#endif /* OX_SAX_H */
