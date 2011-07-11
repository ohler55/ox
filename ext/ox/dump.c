/* dump.c
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

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "ruby.h"
#include "ruby/oniguruma.h"
#include "base64.h"
#include "cache8.h"
#include "ox.h"

typedef struct _Str {
    const char  *str;
    size_t      len;
} *Str;

typedef struct _Element {
    struct _Str         clas;
    struct _Str         attr;
    unsigned long       id;
    int                 indent; // < 0 indicates no \n
    int                 closed;
    char                type;
} *Element;

typedef struct _Out {
    void                (*w_start)(struct _Out *out, Element e);
    void                (*w_end)(struct _Out *out, Element e);
    void                (*w_time)(struct _Out *out, VALUE obj);
    char                *buf;
    char                *end;
    char                *cur;
    Cache8              circ_cache;
    unsigned long       circ_cnt;
    int                 indent;
    int                 depth; // used by dumpHash
} *Out;

static void     dump_obj_to_xml(VALUE obj, Options copts, Out out);

static void     dump_first_obj(VALUE obj, Options copts, Out out);
static void     dump_obj(ID aid, VALUE obj, unsigned int depth, Out out);
static void     dump_gen_doc(VALUE obj, unsigned int depth, Options copts, Out out);
static void     dump_gen_element(VALUE obj, unsigned int depth, Out out);
static int      dump_gen_attr(VALUE key, VALUE value, Out out);
static int      dump_gen_nodes(VALUE obj, unsigned int depth, Out out);
static void     dump_gen_val_node(VALUE obj, unsigned int depth,
                                  const char *pre, size_t plen,
                                  const char *suf, size_t slen, Out out);

static void     dump_start(Out out, Element e);
static void     dump_end(Out out, Element e);

static void     grow(Out out, size_t len);

static void     dump_value(Out out, const char *value, size_t size);
static int      dump_var(ID key, VALUE value, Out out);
static void     dump_num(Out out, VALUE obj);
static void     dump_time_thin(Out out, VALUE obj);
static void     dump_time_xsd(Out out, VALUE obj);
static int      dump_hash(VALUE key, VALUE value, Out out);

static int      is_xml_friendly(const u_char *str, int len);


static char     xml_friendly_chars[256] = "\
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\
ooxoooxxooooooooooooooooooooxoxo\
oooooooooooooooooooooooooooooooo\
xoooooooooooooooooooooooooooooox\
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

inline static int
is_xml_friendly(const u_char *str, int len) {
    for (; 0 < len; str++, len--) {
        if ('x' == xml_friendly_chars[*str]) {
            return 0;
        }
    }
    return 1;
}

inline static Type
obj_class_code(VALUE obj) {
    switch (rb_type(obj)) {
    case RUBY_T_NIL:            return NilClassCode;
    case RUBY_T_ARRAY:          return ArrayCode;
    case RUBY_T_HASH:           return HashCode;
    case RUBY_T_TRUE:           return TrueClassCode;
    case RUBY_T_FALSE:          return FalseClassCode;
    case RUBY_T_FIXNUM:         return FixnumCode;
    case RUBY_T_FLOAT:          return FloatCode;
    case RUBY_T_STRING:         return (is_xml_friendly((u_char*)StringValuePtr(obj), (int)RSTRING_LEN(obj))) ? StringCode : Base64Code;
    case RUBY_T_SYMBOL:         return SymbolCode;
    case RUBY_T_DATA:           return (rb_cTime == rb_obj_class(obj)) ? TimeCode : 0;
    case RUBY_T_STRUCT:         return (rb_cRange == rb_obj_class(obj)) ? RangeCode : StructCode;
    case RUBY_T_OBJECT:         return (ox_document_clas == rb_obj_class(obj) || ox_element_clas == rb_obj_class(obj)) ? RawCode : ObjectCode;
    case RUBY_T_REGEXP:         return RegexpCode;
    case RUBY_T_BIGNUM:         return BignumCode;
    case RUBY_T_COMPLEX:        return ComplexCode;
    case RUBY_T_RATIONAL:       return RationalCode;
    case RUBY_T_CLASS:          return ClassCode;
    default:                    return 0;
    }
}

inline static void
fill_indent(Out out, int cnt) {
    if (0 <= cnt) {
        *out->cur++ = '\n';
        for (; 0 < cnt; cnt--) {
            *out->cur++ = ' ';
        }
    }
}

inline static void
fill_value(Out out, const char *value, size_t len) {
    if (6 < len) {
        memcpy(out->cur, value, len);
        out->cur += len;
    } else {
        for (; '\0' != *value; value++) {
            *out->cur++ = *value;
        }
    }
}

inline static void
fill_attr(Out out, char name, const char *value, size_t len) {
    *out->cur++ = ' ';
    *out->cur++ = name;
    *out->cur++ = '=';
    *out->cur++ = '"';
    if (6 < len) {
        memcpy(out->cur, value, len);
        out->cur += len;
    } else {
        for (; '\0' != *value; value++) {
            *out->cur++ = *value;
        }
    }
    *out->cur++ = '"';
}

inline static const char*
ulong2str(unsigned long num, char *end) {
    char        *b;

    *end-- = '\0';
    for (b = end; 0 < num || b == end; num /= 10, b--) {
        *b = (num % 10) + '0';
    }
    b++;

    return b;
}

static int
check_circular(Out out, VALUE obj, Element e) {
    unsigned long       *slot;
    unsigned long       id;
    int                 result;
    
    if (0 == (id = ox_cache8_get(out->circ_cache, obj, &slot))) {
        out->circ_cnt++;
        id = out->circ_cnt;
        *slot = id;
        e->id = id;
        result = 0;
    } else {
        e->type = RefCode;  e->clas.len = 0;  e->clas.str = 0;
        e->closed = 1;
        e->id = id;
        out->w_start(out, e);
        result = 1;
    }
    return result;
}

static void
grow(Out out, size_t len) {
    size_t  size = out->end - out->buf;
    long    pos = out->cur - out->buf;
    char    *buf;
        
    size *= 2;
    if (size < len + pos) {
        size += len;
    }
    if (0 == (buf = (char*)realloc(out->buf, size))) {
        rb_raise(rb_eNoMemError, "Failed to create string. [%d:%s]\n", ENOSPC, strerror(ENOSPC));
    }
    out->buf = buf;
    out->end = buf + size;
    out->cur = out->buf + pos;
}

static void
dump_start(Out out, Element e) {
    size_t      size = e->indent + 4;

    if (0 < e->attr.len) { // a="attr"
        size += e->attr.len + 5;
    }
    if (0 < e->clas.len) { // c="class"
        size += e->clas.len + 5;
    }
    if (0 < e->id) { // i="id"
        size += 24; // over estimate, 19 digits
    }
    if (out->end - out->cur < (long)size) {
        grow(out, size);
    }
    if (out->buf < out->cur) {
        fill_indent(out, e->indent);
    }
    *out->cur++ = '<';
    *out->cur++ = e->type;
    if (0 < e->attr.len) {
        fill_attr(out, 'a', e->attr.str, e->attr.len);
    }
    if ((ObjectCode == e->type || StructCode == e->type || ClassCode == e->type) && 0 < e->clas.len) {
        fill_attr(out, 'c', e->clas.str, e->clas.len);
    }
    if (0 < e->id) {
        char            buf[32];
        char            *end = buf + sizeof(buf);
        const char      *s = ulong2str(e->id, end);
        
        fill_attr(out, 'i', s, end - s);
    }
    if (e->closed) {
        *out->cur++ = '/';
    }
    *out->cur++ = '>';
    *out->cur = '\0';
}

static void
dump_end(Out out, Element e) {
    size_t      size = e->indent + 5;

    if (out->end - out->cur < (long)size) {
        grow(out, size);
    }
    fill_indent(out, e->indent);
    *out->cur++ = '<';
    *out->cur++ = '/';
    *out->cur++ = e->type;
    *out->cur++ = '>';
    *out->cur = '\0';
}

inline static void
dump_value(Out out, const char *value, size_t size) {
    if (out->end - out->cur < (long)size) {
        grow(out, size);
    }
    if (6 < size) {
        memcpy(out->cur, value, size);
        out->cur += size;
    } else {
        for (; '\0' != *value; value++) {
            *out->cur++ = *value;
        }
    }
    *out->cur = '\0';
}

inline static void
dump_num(Out out, VALUE obj) {
    char        buf[32];
    char        *b = buf + sizeof(buf) - 1;
    long        num = NUM2LONG(obj);
    int         neg = 0;

    if (0 > num) {
        neg = 1;
        num = -num;
    }
    *b-- = '\0';
    if (0 < num) {
        for (; 0 < num; num /= 10, b--) {
            *b = (num % 10) + '0';
        }
        if (neg) {
            *b = '-';
        } else {
            b++;
        }
    } else {
        *b = '0';
    }
    if (out->end - out->cur < (long)(sizeof(buf) - (b - buf))) {
        grow(out, sizeof(buf) - (b - buf));
    }
    for (; '\0' != *b; b++) {
        *out->cur++ = *b;
    }
    *out->cur = '\0';
}

static void
dump_time_thin(Out out, VALUE obj) {
    char        buf[64];
    char        *b = buf + sizeof(buf) - 1;
    time_t      sec = NUM2LONG(rb_funcall2(obj, tv_sec_id, 0, 0));
    long        usec = NUM2LONG(rb_funcall2(obj, tv_usec_id, 0, 0));
    char        *dot = b - 7;
    long        size;

    *b-- = '\0';
    for (; dot < b; b--, usec /= 10) {
        *b = '0' + (usec % 10);
    }
    *b-- = '.';
    for (; 0 < sec; b--, sec /= 10) {
        *b = '0' + (sec % 10);
    }
    b++;
    size = sizeof(buf) - (b - buf) - 1;
    if (out->end - out->cur < size) {
        grow(out, size);
    }
    memcpy(out->cur, b, size);
    out->cur += size;
}

static void
dump_time_xsd(Out out, VALUE obj) {
    struct tm   *tm;
    time_t      sec = NUM2LONG(rb_funcall2(obj, tv_sec_id, 0, 0));
    long        usec = NUM2LONG(rb_funcall2(obj, tv_usec_id, 0, 0));
    int         tzhour, tzmin;
    char        tzsign = '+';

    if (out->end - out->cur < 33) {
        grow(out, 33);
    }
    // 2010-07-09T10:47:45.895826+09:00
    tm = localtime(&sec);
    if (0 > tm->tm_gmtoff) {
        tzsign = '-';
        tzhour = (int)(tm->tm_gmtoff / -3600);
        tzmin = (int)(tm->tm_gmtoff / -60) - (tzhour * 60);
    } else {
        tzhour = (int)(tm->tm_gmtoff / 3600);
        tzmin = (int)(tm->tm_gmtoff / 60) - (tzhour * 60);
    }
    sprintf(out->cur, "%04d-%02d-%02dT%02d:%02d:%02d.%06ld%c%02d:%02d",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec, usec,
            tzsign, tzhour, tzmin);
}

static void
dump_first_obj(VALUE obj, Options copts, Out out) {
    char        buf[128];
    int         cnt;

    if (Yes == copts->with_xml) {
        if ('\0' == *copts->encoding) {
            dump_value(out, "<?xml version=\"1.0\"?>", 21);
        } else {
            cnt = sprintf(buf, "<?xml version=\"1.0\" encoding=\"%s\"?>", copts->encoding);
            dump_value(out, buf, cnt);
        }
    }
    if (Yes == copts->with_instruct) {
        cnt = sprintf(buf, "%s<?ox version=\"1.0\" mode=\"object\"%s%s?>",
                      (out->buf < out->cur) ? "\n" : "",
                      (Yes == copts->circular) ? " circular=\"yes\"" : ((No == copts->circular) ? " circular=\"no\"" : ""),
                      (Yes == copts->xsd_date) ? " xsd_date=\"yes\"" : ((No == copts->xsd_date) ? " xsd_date=\"no\"" : ""));
        dump_value(out, buf, cnt);
    }
    if (Yes == copts->with_dtd) {
        cnt = sprintf(buf, "%s<!DOCTYPE %c SYSTEM \"ox.dtd\">", (out->buf < out->cur) ? "\n" : "", obj_class_code(obj));
        dump_value(out, buf, cnt);
    }
    dump_obj(0, obj, 0, out);
}

static void
dump_obj(ID aid, VALUE obj, unsigned int depth, Out out) {
    struct _Element     e;
    char                value_buf[64];
    int                 cnt;

    if (0 == aid) {
        //e.attr.str = 0;
        e.attr.len = 0;
    } else {
        e.attr.str = rb_id2name(aid);
        e.attr.len = strlen(e.attr.str);
    }
    e.closed = 0;
    if (0 == depth) {
        e.indent = (0 <= out->indent) ? 0 : -1;
    } else if (0 > out->indent) {
        e.indent = -1;
    } else if (0 == out->indent) {
        e.indent = 0;
    } else {
        e.indent = depth * out->indent;
    }
    e.id = 0;
    switch (rb_type(obj)) {
    case RUBY_T_NIL:
        e.type = NilClassCode;  e.clas.len = 0;  e.clas.str = 0;
        e.closed = 1;
        out->w_start(out, &e);
        break;
    case RUBY_T_ARRAY:
        if (0 != out->circ_cache && check_circular(out, obj, &e)) {
            break;
        }
        cnt = (int)RARRAY_LEN(obj);
        e.type = ArrayCode;  e.clas.len = 5;  e.clas.str = "Array";
        e.closed = (0 >= cnt);
        out->w_start(out, &e);
        if (!e.closed) {
            VALUE       *np = RARRAY_PTR(obj);
            int         i;
            int         d2 = depth + 1;

            for (i = cnt; 0 < i; i--, np++) {
                dump_obj(0, *np, d2, out);
            }
            out->w_end(out, &e);
        }
        break;
    case RUBY_T_HASH:
        if (0 != out->circ_cache && check_circular(out, obj, &e)) {
            break;
        }
        cnt = (int)RHASH_SIZE(obj);
        e.type = HashCode;  e.clas.len = 4;  e.clas.str = "Hash";
        e.closed = (0 >= cnt);
        out->w_start(out, &e);
        if (0 < cnt) {
            unsigned int        od = out->depth;
            
            out->depth = depth + 1;
            rb_hash_foreach(obj, dump_hash, (VALUE)out);
            out->depth = od;
            out->w_end(out, &e);
        }
        break;
    case RUBY_T_TRUE:
        e.type = TrueClassCode;  e.clas.len = 9;  e.clas.str = "TrueClass";
        e.closed = 1;
        out->w_start(out, &e);
        break;
    case RUBY_T_FALSE:
        e.type = FalseClassCode;  e.clas.len = 10;  e.clas.str = "FalseClass";
        e.closed = 1;
        out->w_start(out, &e);
        break;
    case RUBY_T_FIXNUM:
        e.type = FixnumCode;  e.clas.len = 6;  e.clas.str = "Fixnum";
        out->w_start(out, &e);
        dump_num(out, obj);
        e.indent = -1;
        out->w_end(out, &e);
        break;
    case RUBY_T_FLOAT:
        e.type = FloatCode;  e.clas.len = 5;  e.clas.str = "Float";
        cnt = sprintf(value_buf, "%0.16g", RFLOAT_VALUE(obj)); // used sprintf due to bug in snprintf
        out->w_start(out, &e);
        dump_value(out, value_buf, cnt);
        e.indent = -1;
        out->w_end(out, &e);
        break;
    case RUBY_T_STRING:
    {
        const char      *str;

        if (0 != out->circ_cache && check_circular(out, obj, &e)) {
            break;
        }
        str = StringValuePtr(obj);
        cnt = (int)RSTRING_LEN(obj);
        if (is_xml_friendly((u_char*)str, cnt)) {
            e.type = StringCode;  e.clas.len = 6;  e.clas.str = "String";
            out->w_start(out, &e);
            dump_value(out, str, cnt);
            e.indent = -1;
            out->w_end(out, &e);
        } else {
            char                buf64[4096];
            char                *b64 = buf64;
            unsigned long       size = b64_size(cnt);

            e.type = Base64Code;  e.clas.len = 6;  e.clas.str = "Base64";
            if (sizeof(buf64) < size) {
                if (0 == (b64 = (char*)malloc(size + 1))) {
                    rb_raise(rb_eNoMemError, "Failed to create string. [%d:%s]\n", ENOSPC, strerror(ENOSPC));
                }
            }
            to_base64((u_char*)str, cnt, b64);
            out->w_start(out, &e);
            dump_value(out, b64, size);
            e.indent = -1;
            out->w_end(out, &e);
            // TBD pass in flag to free if failure
            if (sizeof(buf64) <= size) {
                free(b64);
            }
        }
        break;
    }
    case RUBY_T_SYMBOL:
    {
        const char      *sym = rb_id2name(SYM2ID(obj));

        e.type = SymbolCode;  e.clas.len = 6;  e.clas.str = "Symbol";
        out->w_start(out, &e);
        dump_value(out, sym, strlen(sym));
        e.indent = -1;
        out->w_end(out, &e);
        break;
    }
    case RUBY_T_DATA:
    {
        VALUE   clas;

        clas = rb_obj_class(obj);
        if (rb_cTime == clas) {
            e.type = TimeCode;  e.clas.len = 4;  e.clas.str = "Time";
            out->w_start(out, &e);
            out->w_time(out, obj);
            e.indent = -1;
            out->w_end(out, &e);
        } else {
            printf("*** dumpAttr: RUBY_T_DATA class: %s\n", rb_class2name(clas));
        }
        break;
    }
    case RUBY_T_STRUCT:
    {
        VALUE   clas;

        if (0 != out->circ_cache && check_circular(out, obj, &e)) {
            break;
        }
        clas = rb_obj_class(obj);
        if (rb_cRange == clas) {
            VALUE       beg = RSTRUCT(obj)->as.ary[0];
            VALUE       end = RSTRUCT(obj)->as.ary[1];
            VALUE       excl = RSTRUCT(obj)->as.ary[2];
            int         d2 = depth + 1;

            e.type = RangeCode;  e.clas.len = 5;  e.clas.str = "Range";
            out->w_start(out, &e);
            dump_obj(beg_id, beg, d2, out);
            dump_obj(end_id, end, d2, out);
            dump_obj(excl_id, excl, d2, out);
            out->w_end(out, &e);
        } else {
            char        num_buf[16];
            VALUE       *vp;
            int         i;
            int         d2 = depth + 1;
            
            e.type = StructCode;
            e.clas.str = rb_class2name(clas);
            e.clas.len = strlen(e.clas.str);
            out->w_start(out, &e);
            cnt = (int)RSTRUCT_LEN(obj);
            for (i = 0, vp = RSTRUCT_PTR(obj); i < cnt; i++, vp++) {
                dump_obj(rb_intern(ulong2str(i, num_buf + sizeof(num_buf) - 1)), *vp, d2, out);
            }
            out->w_end(out, &e);
        }
        break;
    }
    case RUBY_T_OBJECT:
    {
        VALUE   clas;

        if (0 != out->circ_cache && check_circular(out, obj, &e)) {
            break;
        }
        clas = rb_obj_class(obj);
        e.clas.str = rb_class2name(clas);
        e.clas.len = strlen(e.clas.str);
        if (ox_document_clas == clas) {
            e.type = RawCode;
            out->w_start(out, &e);
            dump_gen_doc(obj, depth + 1, 0, out);
            out->w_end(out, &e);
        } else if (ox_element_clas == clas) {
            e.type = RawCode;
            out->w_start(out, &e);
            dump_gen_element(obj, depth + 1, out);
            out->w_end(out, &e);
        } else { // Object
            e.type = ObjectCode;
            cnt = (int)rb_ivar_count(obj);
            e.closed = (0 >= cnt);
            out->w_start(out, &e);
            if (0 < cnt) {
                unsigned int        od = out->depth;
            
                out->depth = depth + 1;
                rb_ivar_foreach(obj, dump_var, (VALUE)out);
                out->depth = od;
                out->w_end(out, &e);
            }
        }
        break;
    }
    case RUBY_T_REGEXP:
    {
#if 1
        VALUE           rs = rb_funcall2(obj, inspect_id, 0, 0);
        const char      *s = StringValuePtr(rs);

        cnt = (int)RSTRING_LEN(rs);
#else
        const char      *s = RREGEXP_SRC_PTR(obj);
        int             options = rb_reg_options(obj);

        cnt = (int)RREGEXP_SRC_LEN(obj);
#endif
        e.type = RegexpCode;  e.clas.len = 6;  e.clas.str = "Regexp";
        out->w_start(out, &e);
        if (is_xml_friendly((u_char*)s, cnt)) {
            //dump_value(out, "/", 1);
            dump_value(out, s, cnt);
        } else {
            char                buf64[4096];
            char                *b64 = buf64;
            unsigned long       size = b64_size(cnt);

            if (sizeof(buf64) < size) {
                if (0 == (b64 = (char*)malloc(size + 1))) {
                    rb_raise(rb_eNoMemError, "Failed to create string. [%d:%s]\n", ENOSPC, strerror(ENOSPC));
                }
            }
            to_base64((u_char*)s, cnt, b64);
            dump_value(out, b64, size);
            if (sizeof(buf64) <= size) {
                free(b64);
            }
        }
#if 0
        dump_value(out, "/", 1);
        if (0 != (ONIG_OPTION_MULTILINE & options)) {
            dump_value(out, "m", 1);
        }
        if (0 != (ONIG_OPTION_IGNORECASE & options)) {
            dump_value(out, "i", 1);
        }
        if (0 != (ONIG_OPTION_EXTEND & options)) {
            dump_value(out, "x", 1);
        }
#endif
        e.indent = -1;
        out->w_end(out, &e);
        break;
    }
    case RUBY_T_BIGNUM:
    {
        VALUE   rs = rb_big2str(obj, 10);
        
        e.type = BignumCode;  e.clas.len = 6;  e.clas.str = "Bignum";
        out->w_start(out, &e);
        dump_value(out, StringValuePtr(rs), RSTRING_LEN(rs));
        e.indent = -1;
        out->w_end(out, &e);
        break;
    }
    case RUBY_T_COMPLEX:
        e.type = ComplexCode;  e.clas.len = 7;  e.clas.str = "Complex";
        out->w_start(out, &e);
        dump_obj(0, RCOMPLEX(obj)->real, depth + 1, out);
        dump_obj(0, RCOMPLEX(obj)->imag, depth + 1, out);
        out->w_end(out, &e);
        break;
    case RUBY_T_RATIONAL:
        e.type = RationalCode;  e.clas.len = 8;  e.clas.str = "Rational";
        out->w_start(out, &e);
        dump_obj(0, RRATIONAL(obj)->num, depth + 1, out);
        dump_obj(0, RRATIONAL(obj)->den, depth + 1, out);
        out->w_end(out, &e);
        break;
    case RUBY_T_CLASS:
    {
        e.type = ClassCode;
        e.clas.str = rb_class2name(obj);
        e.clas.len = strlen(e.clas.str);
        e.closed = 1;
        out->w_start(out, &e);
        break;
    }
    default:
        rb_raise(rb_eNotImpError, "Failed to dump %s Object (%02x)\n", rb_class2name(rb_obj_class(obj)), rb_type(obj));
        break;
    }
}

static int
dump_var(ID key, VALUE value, Out out) {
    dump_obj(key, value, out->depth, out);
    
    return ST_CONTINUE;
}

static int
dump_hash(VALUE key, VALUE value, Out out) {
    dump_obj(0, key, out->depth, out);
    dump_obj(0, value, out->depth, out);
    
    return ST_CONTINUE;
}

static void
dump_gen_doc(VALUE obj, unsigned int depth, Options copts, Out out) {
    VALUE       attrs = rb_attr_get(obj, attributes_id);
    VALUE       nodes = rb_attr_get(obj, nodes_id);

    if (Yes == copts->with_xml) {
        dump_value(out, "<?xml", 5);
        if (Qnil != attrs) {
            rb_hash_foreach(attrs, dump_gen_attr, (VALUE)out);
        }
        dump_value(out, "?>", 2);
    }
    if (Yes == copts->with_instruct) {
        if (out->buf < out->cur) {
            dump_value(out, "\n<?ox version=\"1.0\" mode=\"generic\"?>", 36);
        } else {
            dump_value(out, "<?ox version=\"1.0\" mode=\"generic\"?>", 35);
        }
    }
    if (Qnil != nodes) {
        dump_gen_nodes(nodes, depth, out);
    }
}

static void
dump_gen_element(VALUE obj, unsigned int depth, Out out) {
    VALUE       rname = rb_attr_get(obj, value_id);
    VALUE       attrs = rb_attr_get(obj, attributes_id);
    VALUE       nodes = rb_attr_get(obj, nodes_id);
    const char  *name = StringValuePtr(rname);
    long        nlen = RSTRING_LEN(rname);
    size_t      size;
    int         indent;
    
    if (0 > out->indent) {
        indent = -1;
    } else if (0 == out->indent) {
        indent = 0;
    } else {
        indent = depth * out->indent;
    }
    size = indent + 4 + nlen;
    if (out->end - out->cur < (long)size) {
        grow(out, size);
    }
    fill_indent(out, indent);
    *out->cur++ = '<';
    fill_value(out, name, nlen);
    if (Qnil != attrs) {
        rb_hash_foreach(attrs, dump_gen_attr, (VALUE)out);
    }
    if (Qnil != nodes) {
        int     do_indent;
        
        *out->cur++ = '>';
        do_indent = dump_gen_nodes(nodes, depth, out);
        if (out->end - out->cur < (long)size) {
            grow(out, size);
        }
        if (do_indent) {
            fill_indent(out, indent);
        }
        *out->cur++ = '<';
        *out->cur++ = '/';
        fill_value(out, name, nlen);
    } else {
        *out->cur++ = '/';
    }
    *out->cur++ = '>';
    *out->cur = '\0';
}

static int
dump_gen_nodes(VALUE obj, unsigned int depth, Out out) {
    long        cnt = RARRAY_LEN(obj);
    int         indent_needed = 1;
    
    if (0 < cnt) {
        VALUE   *np = RARRAY_PTR(obj);
        VALUE   clas;
        int     d2 = depth + 1;

        for (; 0 < cnt; cnt--, np++) {
            clas = rb_obj_class(*np);
            if (ox_element_clas == clas) {
                dump_gen_element(*np, d2, out);
            } else if (rb_cString == clas) {
                dump_value(out, StringValuePtr(*np), RSTRING_LEN(*np));
                indent_needed = (1 == cnt) ? 0 : 1;
            } else if (ox_comment_clas == clas) {
                dump_gen_val_node(*np, d2, "<!-- ", 5, " -->", 4, out);
            } else if (ox_cdata_clas == clas) {
                dump_gen_val_node(*np, d2, "<![CDATA[", 9, "]]>", 3, out);
            } else if (ox_doctype_clas == clas) {
                dump_gen_val_node(*np, d2, "<!DOCTYPE ", 10, " >", 2, out);
            } else {
                rb_raise(rb_eTypeError, "Unexpected class, %s, while dumping generic XML\n", rb_class2name(clas));
            }
        }
    }
    return indent_needed;
}

static int
dump_gen_attr(VALUE key, VALUE value, Out out) {
    const char  *ks = (RUBY_T_SYMBOL == rb_type(key)) ? rb_id2name(SYM2ID(key)) : StringValuePtr(key);
    size_t      klen = strlen(ks);
    size_t      size = 4 + klen + RSTRING_LEN(value);
    
    if (out->end - out->cur < (long)size) {
        grow(out, size);
    }
    *out->cur++ = ' ';
    fill_value(out, ks, klen);
    *out->cur++ = '=';
    *out->cur++ = '"';
    fill_value(out, StringValuePtr(value), RSTRING_LEN(value));
    *out->cur++ = '"';

    return ST_CONTINUE;
}

static void
dump_gen_val_node(VALUE obj, unsigned int depth,
                  const char *pre, size_t plen,
                  const char *suf, size_t slen, Out out) {
    VALUE       v = rb_attr_get(obj, value_id);
    const char  *val;
    size_t      vlen;
    size_t      size;
    int         indent;

    if (RUBY_T_STRING != rb_type(v)) {
        return;
    }
    val = StringValuePtr(v);
    vlen = RSTRING_LEN(v);
    if (0 > out->indent) {
        indent = -1;
    } else if (0 == out->indent) {
        indent = 0;
    } else {
        indent = depth * out->indent;
    }
    size = indent + plen + slen + vlen;
    if (out->end - out->cur < (long)size) {
        grow(out, size);
    }
    fill_indent(out, indent);
    fill_value(out, pre, plen);
    fill_value(out, val, vlen);
    fill_value(out, suf, slen);
    *out->cur = '\0';
}

static void
dump_obj_to_xml(VALUE obj, Options copts, Out out) {
    VALUE       clas = rb_obj_class(obj);

    out->w_time = (Yes == copts->xsd_date) ? dump_time_xsd : dump_time_thin;
    out->buf = (char*)malloc(65336);
    out->end = out->buf + 65336;
    out->cur = out->buf;
    out->circ_cache = 0;
    out->circ_cnt = 0;
    if (Yes == copts->circular) {
        ox_cache8_new(&out->circ_cache);
    }
    out->indent = copts->indent;
    if (ox_document_clas == clas) {
        dump_gen_doc(obj, -1, copts, out);
    } else if (ox_element_clas == clas) {
        dump_gen_element(obj, 0, out);
    } else {
        out->w_start = dump_start;
        out->w_end = dump_end;
        dump_first_obj(obj, copts, out);
    }
    dump_value(out, "\n", 1);
}

char*
write_obj_to_str(VALUE obj, Options copts) {
    struct _Out out;
    
    dump_obj_to_xml(obj, copts, &out);

    return out.buf;
}

void
write_obj_to_file(VALUE obj, const char *path, Options copts) {
    struct _Out out;
    size_t      size;
    FILE        *f;    

    dump_obj_to_xml(obj, copts, &out);
    size = out.cur - out.buf;
    if (0 == (f = fopen(path, "w"))) {
        rb_raise(rb_eIOError, "%s\n", strerror(errno));
    }
    if (size != fwrite(out.buf, 1, size, f)) {
        int err = ferror(f);
        rb_raise(rb_eIOError, "Write failed. [%d:%s]\n", err, strerror(err));
    }
    free(out.buf);
    fclose(f);
}
