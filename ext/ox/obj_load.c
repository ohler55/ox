/* obj_load.c
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
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "ruby.h"
#include "base64.h"
#include "ox.h"

static void     instruct(PInfo pi, const char *target, Attr attrs);
static void     add_text(PInfo pi, char *text, int closed);
static void     add_element(PInfo pi, const char *ename, Attr attrs, int hasChildren);
static void     end_element(PInfo pi, const char *ename);

static VALUE    parse_time(const char *text, VALUE clas);
static VALUE    parse_xsd_time(const char *text, VALUE clas);
static VALUE    parse_double_time(const char *text, VALUE clas);
static VALUE    parse_regexp(const char *text);

static VALUE            get_var_sym_from_attrs(Attr a);
static VALUE            get_obj_from_attrs(Attr a, PInfo pi);
static VALUE            get_class_from_attrs(Attr a, PInfo pi);
static VALUE            classname2class(const char *name, PInfo pi);
static unsigned long    get_id_from_attrs(PInfo pi, Attr a);
static CircArray        circ_array_new(void);
static void             circ_array_free(CircArray ca);
static void             circ_array_set(CircArray ca, VALUE obj, unsigned long id);
static VALUE            circ_array_get(CircArray ca, unsigned long id);

static void             debug_stack(PInfo pi, const char *comment);
static void             fill_indent(PInfo pi, char *buf, size_t size);


struct _ParseCallbacks   _ox_obj_callbacks = {
    instruct, // instruct,
    0, // add_doctype,
    0, // add_comment,
    0, // add_cdata,
    add_text,
    add_element,
    end_element,
};

ParseCallbacks   ox_obj_callbacks = &_ox_obj_callbacks;

extern ParseCallbacks   ox_gen_callbacks;


inline static ID
name2var(const char *name) {
    VALUE       *slot;
    ID          var_id;

    if ('0' <= *name && *name <= '9') {
        var_id = INT2NUM(atoi(name));
    } else if (Qundef == (var_id = ox_cache_get(attr_cache, name, &slot))) {
        var_id = rb_intern(name);
        *slot = var_id;
    }
    return var_id;
}

inline static VALUE
resolve_classname(VALUE mod, const char *class_name, Effort effort) {
    VALUE       clas;
    ID          ci = rb_intern(class_name);

    switch (effort) {
    case TolerantEffort:
        if (rb_const_defined_at(mod, ci)) {
            clas = rb_const_get_at(mod, ci);
        } else {
            clas = Qundef;
        }
        break;
    case AutoEffort:
        if (rb_const_defined_at(mod, ci)) {
            clas = rb_const_get_at(mod, ci);
        } else {
            clas = rb_define_class_under(mod, class_name, ox_bag_clas);
        }
        break;
    case StrictEffort:
    default:
        // raise an error if name is not defined
        clas = rb_const_get_at(mod, ci);
        break;
    }
    return clas;
}

inline static VALUE
classname2obj(const char *name, PInfo pi) {
    VALUE   clas = classname2class(name, pi);
    
    if (Qundef == clas) {
        return Qnil;
    } else {
        return rb_obj_alloc(clas);
    }
}

#ifndef NO_RSTRUCT
inline static VALUE
structname2obj(const char *name) {
    VALUE       ost;
    const char  *s = name;

    for (; 1; s++) {
        if ('\0' == *s) {
            s = name;
            break;
        } else if (':' == *s) {
            s += 2;
            break;
        }
    }
    ost = rb_const_get(struct_class, rb_intern(s));
// use encoding as the indicator for Ruby 1.8.7 or 1.9.x
#ifdef HAVE_RUBY_ENCODING_H
    return rb_struct_alloc_noinit(ost);
#else
    return rb_struct_new(ost);
#endif
}
#endif

// 2010-07-09T10:47:45.895826162+09:00
inline static VALUE
parse_time(const char *text, VALUE clas) {
    VALUE       t;

    if (Qnil == (t = parse_double_time(text, clas)) &&
        Qnil == (t = parse_xsd_time(text, clas))) {
        VALUE       args[1];

        //printf("**** time parse\n");
        *args = rb_str_new2(text);
        t = rb_funcall2(time_class, parse_id, 1, args);
    }
    return t;
}

static VALUE
classname2class(const char *name, PInfo pi) {
    VALUE       *slot;
    VALUE       clas;
            
    if (Qundef == (clas = ox_cache_get(class_cache, name, &slot))) {
        char            class_name[1024];
        char            *s;
        const char      *n = name;

        clas = rb_cObject;
        for (s = class_name; '\0' != *n; n++) {
            if (':' == *n) {
                *s = '\0';
                n++;
                if (Qundef == (clas = resolve_classname(clas, class_name, pi->effort))) {
                    return Qundef;
                }
                s = class_name;
            } else {
                *s++ = *n;
            }
        }
        *s = '\0';
        if (Qundef != (clas = resolve_classname(clas, class_name, pi->effort))) {
            *slot = clas;
        }
    }
    return clas;
}

static VALUE
get_var_sym_from_attrs(Attr a) {
    for (; 0 != a->name; a++) {
        if ('a' == *a->name && '\0' == *(a->name + 1)) {
            return name2var(a->value);
        }
    }
    return Qundef;
}

static VALUE
get_obj_from_attrs(Attr a, PInfo pi) {
    for (; 0 != a->name; a++) {
        if ('c' == *a->name && '\0' == *(a->name + 1)) {
            return classname2obj(a->value, pi);
        }
    }
    return Qundef;
}

#ifndef NO_RSTRUCT
static VALUE
get_struct_from_attrs(Attr a) {
    for (; 0 != a->name; a++) {
        if ('c' == *a->name && '\0' == *(a->name + 1)) {
            return structname2obj(a->value);
        }
    }
    return Qundef;
}
#endif

static VALUE
get_class_from_attrs(Attr a, PInfo pi) {
    for (; 0 != a->name; a++) {
        if ('c' == *a->name && '\0' == *(a->name + 1)) {
            return classname2class(a->value, pi);
        }
    }
    return Qundef;
}

static unsigned long
get_id_from_attrs(PInfo pi, Attr a) {
    for (; 0 != a->name; a++) {
        if ('i' == *a->name && '\0' == *(a->name + 1)) {
            unsigned long       id = 0;
            const char          *text = a->value;
            char                c;
            
            for (; '\0' != *text; text++) {
                c = *text;
                if ('0' <= c && c <= '9') {
                    id = id * 10 + (c - '0');
                } else {
                    raise_error("bad number format", pi->str, pi->s);
                }
            }
            return id;
        }
    }
    return 0;
}

static CircArray
circ_array_new() {
    CircArray   ca;
    
    if (0 == (ca = (CircArray)malloc(sizeof(struct _CircArray)))) {
        rb_raise(rb_eNoMemError, "not enough memory\n");
    }
    ca->objs = ca->obj_array;
    ca->size = sizeof(ca->obj_array) / sizeof(VALUE);
    ca->cnt = 0;
    
    return ca;
}

static void
circ_array_free(CircArray ca) {
    if (ca->objs != ca->obj_array) {
        free(ca->objs);
    }
    free(ca);
}

static void
circ_array_set(CircArray ca, VALUE obj, unsigned long id) {
    if (0 < id) {
        unsigned long   i;

        if (ca->size < id) {
            unsigned long       cnt = id + 512;

            if (ca->objs == ca->obj_array) {
                if (0 == (ca->objs = (VALUE*)malloc(sizeof(VALUE) * cnt))) {
                    rb_raise(rb_eNoMemError, "not enough memory\n");
                }
                memcpy(ca->objs, ca->obj_array, sizeof(VALUE) * ca->cnt);
            } else { 
                if (0 == (ca->objs = (VALUE*)realloc(ca->objs, sizeof(VALUE) * cnt))) {
                    rb_raise(rb_eNoMemError, "not enough memory\n");
                }
            }
            ca->size = cnt;
        }
        id--;
        for (i = ca->cnt; i < id; i++) {
            ca->objs[i] = Qundef;
        }
        ca->objs[id] = obj;
        if (ca->cnt <= id) {
            ca->cnt = id + 1;
        }
    }
}

static VALUE
circ_array_get(CircArray ca, unsigned long id) {
    VALUE       obj = Qundef;

    if (id <= ca->cnt) {
        obj = ca->objs[id - 1];
    }
    return obj;
}

static VALUE
parse_regexp(const char *text) {
    const char  *te;
    int         options = 0;
            
    te = text + strlen(text) - 1;
#ifdef HAVE_RUBY_ENCODING_H
    for (; text < te && '/' != *te; te--) {
        switch (*te) {
        case 'i':       options |= ONIG_OPTION_IGNORECASE;      break;
        case 'm':       options |= ONIG_OPTION_MULTILINE;       break;
        case 'x':       options |= ONIG_OPTION_EXTEND;          break;
        default:                                                break;
        }
    }
#endif
    return rb_reg_new(text + 1, te - text - 1, options);
}

static void
instruct(PInfo pi, const char *target, Attr attrs) {
    if (0 == strcmp("xml", target)) {
#ifdef HAVE_RUBY_ENCODING_H
        for (; 0 != attrs->name; attrs++) {
            if (0 == strcmp("encoding", attrs->name)) {
                pi->encoding = rb_enc_find(attrs->value);
            }
        }
#endif
    }
}

static void
add_text(PInfo pi, char *text, int closed) {
    if (!closed) {
        raise_error("Text not closed", pi->str, pi->s);
    }
    if (DEBUG <= pi->trace) {
        char    indent[128];

        fill_indent(pi, indent, sizeof(indent));
        printf("%s '%s' to type %c\n", indent, text, pi->h->type);
    }
    switch (pi->h->type) {
    case NoCode:
    case StringCode:
        pi->h->obj = rb_str_new2(text);
#ifdef HAVE_RUBY_ENCODING_H
        if (0 != pi->encoding) {
            rb_enc_associate(pi->h->obj, pi->encoding);
        }
#endif
        if (0 != pi->circ_array) {
            circ_array_set(pi->circ_array, pi->h->obj, (unsigned long)pi->id);
        }
        break;
    case FixnumCode:
    {
        long        n = 0;
        char        c;
        int         neg = 0;

        if ('-' == *text) {
            neg = 1;
            text++;
        }
        for (; '\0' != *text; text++) {
            c = *text;
            if ('0' <= c && c <= '9') {
                n = n * 10 + (c - '0');
            } else {
                raise_error("bad number format", pi->str, pi->s);
            }
        }
        if (neg) {
            n = -n;
        }
        pi->h->obj = LONG2FIX(n);
        break;
    }
    case FloatCode:
        pi->h->obj = rb_float_new(strtod(text, 0));
        break;
    case SymbolCode:
    {
        VALUE   sym;
        VALUE   *slot;

        if (Qundef == (sym = ox_cache_get(symbol_cache, text, &slot))) {
            sym = ID2SYM(rb_intern(text));
            *slot = sym;
        }
        pi->h->obj = sym;
        break;
    }
    case TimeCode:
        pi->h->obj = parse_time(text, time_class);
        break;
    case String64Code:
    {
        char            buf[1024];
        char            *str = buf;
        unsigned long   str_size = b64_orig_size(text);
        VALUE           v;
        
        if (sizeof(buf) <= str_size) {
            if (0 == (str = (char*)malloc(str_size + 1))) {
                rb_raise(rb_eNoMemError, "not enough memory\n");
            }
        }
        from_base64(text, (u_char*)str);
        v = rb_str_new(str, str_size);
#ifdef HAVE_RUBY_ENCODING_H
        if (0 != pi->encoding) {
            rb_enc_associate(v, pi->encoding);
        }
#endif
        if (0 != pi->circ_array) {
            circ_array_set(pi->circ_array, v, (unsigned long)pi->h->obj);
        }
        pi->h->obj = v;
        if (buf != str) {
            free(str);
        }
        break;
    }
    case Symbol64Code:
    {
        VALUE           sym;
        VALUE           *slot;
        char            buf[1024];
        char            *str = buf;
        unsigned long   str_size = b64_orig_size(text);
        
        if (sizeof(buf) <= str_size) {
            if (0 == (str = (char*)malloc(str_size + 1))) {
                rb_raise(rb_eNoMemError, "not enough memory\n");
            }
        }
        from_base64(text, (u_char*)str);
        if (Qundef == (sym = ox_cache_get(symbol_cache, str, &slot))) {
            sym = ID2SYM(rb_intern(str));
            *slot = sym;
        }
        pi->h->obj = sym;
        if (buf != str) {
            free(str);
        }
        break;
    }
    case RegexpCode:
        if ('/' == *text) {
            pi->h->obj = parse_regexp(text);
        } else {
            char                buf[1024];
            char                *str = buf;
            unsigned long       str_size = b64_orig_size(text);
        
            if (sizeof(buf) <= str_size) {
                if (0 == (str = (char*)malloc(str_size + 1))) {
                    rb_raise(rb_eNoMemError, "not enough memory\n");
                }
            }
            from_base64(text, (u_char*)str);
            pi->h->obj = parse_regexp(str);
            if (sizeof(buf) <= str_size) {
                free(str);
            }
        }
        break;
    case BignumCode:
        pi->h->obj = rb_cstr_to_inum(text, 10, 1);
        break;
    default:
        pi->h->obj = Qnil;
        break;
    }
}

static void
add_element(PInfo pi, const char *ename, Attr attrs, int hasChildren) {
    Attr                a;
    Helper              h;
    unsigned long       id;

    if (TRACE <= pi->trace) {
        char    buf[1024];
        char    indent[128];
        char    *s = buf;
        char    *end = buf + sizeof(buf) - 2;

        s += snprintf(s, end - s, " <%s%s", (hasChildren) ? "" : "/", ename);
        for (a = attrs; 0 != a->name; a++) {
            s += snprintf(s, end - s, " %s=%s", a->name, a->value);
        }
        *s++ = '>';
        *s++ = '\0';
        if (DEBUG <= pi->trace) {
            debug_stack(pi, buf);
        } else {
            fill_indent(pi, indent, sizeof(indent));
            printf("%s%s\n", indent, buf);
        }
    }
    if (0 == pi->h) { // top level object
        pi->h = pi->helpers;
        if (0 != (id = get_id_from_attrs(pi, attrs))) {
            pi->circ_array = circ_array_new();
        }
    } else {
        pi->h++;
    }
    if ('\0' != ename[1]) {
        raise_error("Invalid element name", pi->str, pi->s);
    }
    h = pi->h;
    h->type = *ename;
    h->var = get_var_sym_from_attrs(attrs);
    switch (h->type) {
    case NilClassCode:
        h->obj = Qnil;
        break;
    case TrueClassCode:
        h->obj = Qtrue;
        break;
    case FalseClassCode:
        h->obj = Qfalse;
        break;
    case StringCode:
        // h->obj will be replaced by add_text if it is called
        h->obj = empty_string;
        if (0 != pi->circ_array) {
            pi->id = get_id_from_attrs(pi, attrs);
            circ_array_set(pi->circ_array, h->obj, pi->id);
        }
        break;
    case FixnumCode:
    case FloatCode:
    case SymbolCode:
    case Symbol64Code:
    case RegexpCode:
    case BignumCode:
    case ComplexCode:
    case TimeCode:
    case RationalCode: // sub elements read next
        // value will be read in the following add_text
        h->obj = Qundef;
        break;
    case String64Code:
        h->obj = Qundef;
        if (0 != pi->circ_array) {
            pi->id = get_id_from_attrs(pi, attrs);
        }
        break;
    case ArrayCode:
        h->obj = rb_ary_new();
        if (0 != pi->circ_array) {
            circ_array_set(pi->circ_array, h->obj, get_id_from_attrs(pi, attrs));
        }
        break;
    case HashCode:
        h->obj = rb_hash_new();
        if (0 != pi->circ_array) {
            circ_array_set(pi->circ_array, h->obj, get_id_from_attrs(pi, attrs));
        }
        break;
    case RangeCode:
        h->obj = rb_range_new(zero_fixnum, zero_fixnum, Qfalse);
        break;
    case RawCode:
        if (hasChildren) {
            h->obj = parse(pi->s, ox_gen_callbacks, &pi->s, pi->trace, pi->effort);
            if (0 != pi->circ_array) {
                circ_array_set(pi->circ_array, h->obj, get_id_from_attrs(pi, attrs));
            }
        } else {
            h->obj = Qnil;
        }
        break;
    case ObjectCode:
        h->obj = get_obj_from_attrs(attrs, pi);
        if (0 != pi->circ_array && Qnil != h->obj) {
            circ_array_set(pi->circ_array, h->obj, get_id_from_attrs(pi, attrs));
        }
        break;
    case StructCode:
#ifdef NO_RSTRUCT
        raise_error("Ruby structs not supported with this verion of Ruby", pi->str, pi->s);
#else
        h->obj = get_struct_from_attrs(attrs);
        if (0 != pi->circ_array) {
            circ_array_set(pi->circ_array, h->obj, get_id_from_attrs(pi, attrs));
        }
#endif
        break;
    case ClassCode:
        h->obj = get_class_from_attrs(attrs, pi);
        break;
    case RefCode:
        h->obj = Qundef;
        if (0 != pi->circ_array) {
            h->obj = circ_array_get(pi->circ_array, get_id_from_attrs(pi, attrs));
        }
        if (Qundef == h->obj) {
            raise_error("Invalid circular reference", pi->str, pi->s);
        }
        break;
    default:
        raise_error("Invalid element name", pi->str, pi->s);
        break;
    }
    if (DEBUG <= pi->trace) {
        debug_stack(pi, "   -----------");
    }
}

static void
end_element(PInfo pi, const char *ename) {
    if (TRACE <= pi->trace) {
        char    indent[128];
        
        if (DEBUG <= pi->trace) {
            char    buf[1024];

            snprintf(buf, sizeof(buf) - 1, "</%s>", ename);
            debug_stack(pi, buf);
        } else {
            fill_indent(pi, indent, sizeof(indent));
            printf("%s</%s>\n", indent, ename);
        }
    }
    if (0 != pi->h && pi->helpers <= pi->h) {
        Helper  h = pi->h;

        if (empty_string == h->obj) {
            // special catch for empty strings
            h->obj = rb_str_new2("");
        }
        pi->obj = h->obj;
        pi->h--;
        if (pi->helpers <= pi->h) {
            switch (pi->h->type) {
            case ArrayCode:
                rb_ary_push(pi->h->obj, h->obj);
                break;
            case ObjectCode:
                if (Qnil != pi->h->obj) {
                    rb_ivar_set(pi->h->obj, h->var, h->obj);
                }
                break;
            case StructCode:
#ifdef NO_RSTRUCT
                raise_error("Ruby structs not supported with this verion of Ruby", pi->str, pi->s);
#else
                rb_struct_aset(pi->h->obj, h->var, h->obj);
#endif
                break;
            case HashCode:
                h->type = KeyCode;
                pi->h++;
                break;
            case RangeCode:
#ifdef NO_RSTRUCT
                raise_error("Ruby structs not supported with this verion of Ruby", pi->str, pi->s);
#else
                if (beg_id == h->var) {
                    RSTRUCT_PTR(pi->h->obj)[0] = h->obj;
                } else if (end_id == h->var) {
                    RSTRUCT_PTR(pi->h->obj)[1] = h->obj;
                } else if (excl_id == h->var) {
                    RSTRUCT_PTR(pi->h->obj)[2] = h->obj;
                } else {
                    raise_error("Invalid range attribute", pi->str, pi->s);
                }
#endif
                break;
            case KeyCode:
                rb_hash_aset((pi->h - 1)->obj, pi->h->obj, h->obj);
                pi->h--;
                break;
            case ComplexCode:
#ifdef T_COMPLEX
                if (Qundef == pi->h->obj) {
                    pi->h->obj = h->obj;
                } else {
                    pi->h->obj = rb_complex_new(pi->h->obj, h->obj);
                }
#else
                raise_error("Complex Objects not implemented in Ruby 1.8.7", pi->str, pi->s);
#endif
                break;
            case RationalCode:
#ifdef T_RATIONAL
                if (Qundef == pi->h->obj) {
                    pi->h->obj = h->obj;
                } else {
                    pi->h->obj = rb_rational_new(pi->h->obj, h->obj);
                }
#else
                raise_error("Rational Objects not implemented in Ruby 1.8.7", pi->str, pi->s);
#endif
                break;
            default:
                raise_error("Corrupt parse stack, container is wrong type", pi->str, pi->s);
                break;
            }
        }
    }
    if (0 != pi->circ_array && pi->helpers > pi->h) {
        circ_array_free(pi->circ_array);
        pi->circ_array = 0;
    }
    if (DEBUG <= pi->trace) {
        debug_stack(pi, "   ----------");
    }
}

static VALUE
parse_double_time(const char *text, VALUE clas) {
    VALUE       args[2];
    long        v = 0;
    long        v2 = 0;
    const char  *dot = 0;
    char        c;
    
    for (; '.' != *text; text++) {
        c = *text;
        if (c < '0' || '9' < c) {
            return Qnil;
        }
        v = 10 * v + (long)(c - '0');
    }
    dot = text++;
    for (; '\0' != *text && text - dot <= 6; text++) {
        c = *text;
        if (c < '0' || '9' < c) {
            return Qnil;
        }
        v2 = 10 * v2 + (long)(c - '0');
    }
    for (; text - dot <= 6; text++) {
        v2 *= 10;
    }
    args[0] = INT2FIX(v);
    args[1] = INT2FIX(v2);

    return rb_funcall2(clas, at_id, 2, args);
}

typedef struct _Tp {
    int         cnt;
    char        end;
    char        alt;
} *Tp;

static VALUE
parse_xsd_time(const char *text, VALUE clas) {
    VALUE       args[2];
    long        cargs[10];
    long        *cp = cargs;
    long        v;
    int         i;
    char        c;
    struct _Tp  tpa[10] = { { 4, '-', '-' },
                           { 2, '-', '-' },
                           { 2, 'T', 'T' },
                           { 2, ':', ':' },
                           { 2, ':', ':' },
                           { 2, '.', '.' },
                           { 9, '+', '-' },
                           { 2, ':', ':' },
                           { 2, '\0', '\0' },
                           { 0, '\0', '\0' } };
    Tp          tp = tpa;
    struct tm   tm;

    for (; 0 != tp->cnt; tp++) {
        for (i = tp->cnt, v = 0; 0 < i ; text++, i--) {
            c = *text;
            if (c < '0' || '9' < c) {
                if (tp->end == c || tp->alt == c) {
                    break;
                }
                return Qnil;
            }
            v = 10 * v + (long)(c - '0');
        }
        c = *text++;
        if (tp->end != c && tp->alt != c) {
            return Qnil;
        }
        *cp++ = v;
    }
    tm.tm_year = (int)cargs[0] - 1900;
    tm.tm_mon = (int)cargs[1] - 1;
    tm.tm_mday = (int)cargs[2];
    tm.tm_hour = (int)cargs[3];
    tm.tm_min = (int)cargs[4];
    tm.tm_sec = (int)cargs[5];

    args[0] = INT2FIX(mktime(&tm));
    args[1] = INT2FIX(cargs[6]);

    return rb_funcall2(clas, at_id, 2, args);
}

// debug functions
static void
fill_indent(PInfo pi, char *buf, size_t size) {
    if (0 != pi->h) {
        size_t  cnt = pi->h - pi->helpers + 1;

        if (size < cnt + 1) {
            cnt = size - 1;
        }
        memset(buf, ' ', cnt);
        buf += cnt;
    }
    *buf = '\0';
}

static void
debug_stack(PInfo pi, const char *comment) {
    char        indent[128];
    Helper      h;

    fill_indent(pi, indent, sizeof(indent));
    printf("%s%s\n", indent, comment);
    if (0 != pi->h) {
        for (h = pi->helpers; h <= pi->h; h++) {
            const char  *clas = "---";
            const char  *key = "---";

            if (Qundef != h->obj) {
                VALUE   c =  rb_obj_class(h->obj);

                clas = rb_class2name(c);
            }
            if (Qundef != h->var) {
                if (HashCode == h->type) {
                    VALUE       v;
                    
                    v = rb_funcall2(h->var, rb_intern("to_s"), 0, 0);
                    key = StringValuePtr(v);
                } else if (ObjectCode == (h - 1)->type || RangeCode == (h - 1)->type || StructCode == (h - 1)->type) {
                    key = rb_id2name(h->var);
                } else {
                    printf("%s*** corrupt stack ***\n", indent);
                }
            }
            printf("%s   [%c] %s : %s\n", indent, h->type, clas, key);
        }
    }
}
