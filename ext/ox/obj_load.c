/* obj_load.c
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "base64.h"
#include "intern.h"
#include "ox.h"
#include "ruby.h"
#include "ruby/encoding.h"
#include "time_conv.h"

// No Struct has this many members, so a larger index is always out of range.
#define MAX_STRUCT_INDEX (1 << 24)

static void instruct(PInfo pi, const char *target, Attr attrs, const char *content);
static void add_text(PInfo pi, char *text, size_t len, int closed);
static void add_element(PInfo pi, const char *ename, Attr attrs, int hasChildren);
static void end_element(PInfo pi, const char *ename);

static VALUE parse_time(const char *text, VALUE clas);
static VALUE parse_xsd_time(const char *text, VALUE clas);
static VALUE parse_double_time(const char *text, VALUE clas);
static VALUE parse_regexp(const char *text, PInfo pi);

static ID            get_var_sym_from_attrs(Attr a, void *encoding, bool *indexp);
static VALUE         get_obj_from_attrs(Attr a, PInfo pi, VALUE base_class);
static VALUE         get_class_from_attrs(Attr a, PInfo pi, VALUE base_class);
static VALUE         classname2class(const char *name, PInfo pi, VALUE base_class);
static unsigned long get_id_from_attrs(PInfo pi, Attr a);
static CircArray     circ_array_new(void);
static void          circ_array_set(CircArray ca, VALUE obj, unsigned long id);
static VALUE         circ_array_get(CircArray ca, unsigned long id);

static void debug_stack(PInfo pi, const char *comment);
static void fill_indent(PInfo pi, char *buf, size_t size);

struct _parseCallbacks _ox_obj_callbacks = {
    instruct,  // instruct,
    0,         // add_doctype,
    0,         // add_comment,
    0,         // add_cdata,
    add_text,
    add_element,
    end_element,
    NULL,
};

ParseCallbacks ox_obj_callbacks = &_ox_obj_callbacks;

extern ParseCallbacks ox_gen_callbacks;

inline static VALUE resolve_classname(VALUE mod, const char *class_name, Effort effort, VALUE base_class) {
    VALUE clas;
    ID    ci = rb_intern(class_name);

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
            clas = rb_define_class_under(mod, class_name, base_class);
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

inline static VALUE classname2obj(const char *name, PInfo pi, VALUE base_class) {
    VALUE clas = classname2class(name, pi, base_class);

    if (Qundef == clas) {
        return Qnil;
    } else {
        return rb_obj_alloc(clas);
    }
}

inline static VALUE structname2obj(const char *name) {
    VALUE       ost;
    const char *s = name;

    for (; 1; s++) {
        if ('\0' == *s) {
            s = name;
            break;
        } else if (':' == *s) {
            s += 2;
            break;
        }
    }
    ost = rb_const_get(ox_struct_class, rb_intern(s));
    return rb_struct_alloc_noinit(ost);
}

inline static VALUE parse_ulong(const char *s, PInfo pi) {
    unsigned long n = 0;

    for (; '\0' != *s; s++) {
        if ('0' <= *s && *s <= '9') {
            n = n * 10 + (*s - '0');
        } else {
            set_error(&pi->err, "Invalid number for a julian day", pi->str, pi->s);
            return Qundef;
        }
    }
    return ULONG2NUM(n);
}

// 2010-07-09T10:47:45.895826162+09:00
inline static VALUE parse_time(const char *text, VALUE clas) {
    VALUE t;

    if (Qnil == (t = parse_double_time(text, clas)) && Qnil == (t = parse_xsd_time(text, clas))) {
        VALUE args[1];

        *args = rb_str_new2(text);
        t     = rb_funcall2(ox_time_class, ox_parse_id, 1, args);
    }
    return t;
}

static VALUE classname2class(const char *name, PInfo pi, VALUE base_class) {
    VALUE *slot;
    VALUE  clas;

    if (Qundef == (clas = slot_cache_get(ox_class_cache, name, &slot, 0))) {
        char        class_name[1024];
        char       *s;
        char       *class_name_end = class_name + sizeof(class_name) - 1;
        const char *n              = name;

        clas = rb_cObject;
        for (s = class_name; '\0' != *n; n++) {
            if (':' == *n) {
                *s = '\0';
                n++;
                if (':' != *n) {
                    set_error(&pi->err, "Invalid classname, expected another ':'", pi->str, pi->s);
                    return Qundef;
                }
                if (Qundef == (clas = resolve_classname(clas, class_name, pi->options->effort, base_class))) {
                    return Qundef;
                }
                s = class_name;
            } else {
                if (class_name_end <= s) {
                    // Guard the fixed-size stack buffer: an over-long class name
                    // (from the attacker-controlled `c` attribute in object mode)
                    // would otherwise overflow class_name[].
                    set_error(&pi->err, "Invalid classname, too long", pi->str, pi->s);
                    return Qundef;
                }
                *s++ = *n;
            }
        }
        *s = '\0';
        if (Qundef != (clas = resolve_classname(clas, class_name, pi->options->effort, base_class))) {
            *slot = clas;
            rb_gc_register_address(slot);
        }
    }
    return clas;
}

// Returns a Struct member index as a Fixnum or an instance variable ID. An ID
// is odd, so it satisfies FIXNUM_P too; indexp says which one it is.
static ID get_var_sym_from_attrs(Attr a, void *encoding, bool *indexp) {
    *indexp = false;
    for (; 0 != a->name; a++) {
        if ('a' == *a->name && '\0' == *(a->name + 1)) {
            const char *val = a->value;

            if ('0' <= *val && *val <= '9') {
                // atoi() wrapped past INT_MAX into a negative index, which Ruby
                // counts from the end. Stopping at INT_MAX is no good either:
                // FIXNUM_MAX is 2**30 - 1 where long is 32 bits, and INT2NUM of
                // INT_MAX came back as -1 there.
                int i = 0;

                for (; '0' <= *val && *val <= '9'; val++) {
                    i = i * 10 + (*val - '0');
                    if (MAX_STRUCT_INDEX < i) {
                        i = MAX_STRUCT_INDEX;
                        break;
                    }
                }
                *indexp = true;
                return (ID)INT2NUM(i);
            }
            return ox_id_intern(val, strlen(val));
        }
    }
    return 0;
}

static VALUE get_obj_from_attrs(Attr a, PInfo pi, VALUE base_class) {
    for (; 0 != a->name; a++) {
        if ('c' == *a->name && '\0' == *(a->name + 1)) {
            return classname2obj(a->value, pi, base_class);
        }
    }
    return Qundef;
}

static VALUE get_struct_from_attrs(Attr a) {
    for (; 0 != a->name; a++) {
        if ('c' == *a->name && '\0' == *(a->name + 1)) {
            return structname2obj(a->value);
        }
    }
    return Qundef;
}

static VALUE get_class_from_attrs(Attr a, PInfo pi, VALUE base_class) {
    for (; 0 != a->name; a++) {
        if ('c' == *a->name && '\0' == *(a->name + 1)) {
            return classname2class(a->value, pi, base_class);
        }
    }
    return Qundef;
}

// The id indexes the circular reference table, which is grown to fit it, so an
// unbounded id is an unbounded heap write. Cap it at the document length: every
// referenced object needs at least one element, so no valid id can exceed it.
static unsigned long get_id_from_attrs(PInfo pi, Attr a) {
    for (; 0 != a->name; a++) {
        if ('i' == *a->name && '\0' == *(a->name + 1)) {
            unsigned long       id    = 0;
            const unsigned long limit = (unsigned long)(pi->end - pi->str);
            const char         *text  = a->value;
            char                c;

            for (; '\0' != *text; text++) {
                c = *text;
                if ('0' <= c && c <= '9') {
                    // Checked before the multiply so id can not overflow.
                    if (limit / 10 < id || limit < id * 10 + (unsigned long)(c - '0')) {
                        set_error(&pi->err, "circular reference id out of range", pi->str, pi->s);
                        return 0;
                    }
                    id = id * 10 + (c - '0');
                } else {
                    set_error(&pi->err, "bad number format", pi->str, pi->s);
                    return 0;
                }
            }
            return id;
        }
    }
    return 0;
}

static CircArray circ_array_new(void) {
    CircArray ca;

    ca       = ALLOC(struct _circArray);
    ca->objs = ca->obj_array;
    ca->size = sizeof(ca->obj_array) / sizeof(VALUE);
    ca->cnt  = 0;

    return ca;
}

// Also called from ox_parse_ensure() when a parse ends before end_element().
void ox_circ_array_free(CircArray ca) {
    if (ca->objs != ca->obj_array) {
        xfree(ca->objs);
    }
    xfree(ca);
}

static void circ_array_set(CircArray ca, VALUE obj, unsigned long id) {
    if (0 < id) {
        unsigned long i;

        if (ca->size < id) {
            unsigned long cnt = id + 512;

            if (ca->objs == ca->obj_array) {
                ca->objs = ALLOC_N(VALUE, cnt);
                memcpy(ca->objs, ca->obj_array, sizeof(VALUE) * ca->cnt);
            } else {
                REALLOC_N(ca->objs, VALUE, cnt);
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

static VALUE circ_array_get(CircArray ca, unsigned long id) {
    VALUE obj = Qundef;

    // id is 0 when the `i` attribute is missing or invalid. Without the lower
    // bound that reads objs[-1] and hands the word to Ruby as an object.
    if (0 < id && id <= ca->cnt) {
        obj = ca->objs[id - 1];
    }
    return obj;
}

static VALUE parse_regexp(const char *text, PInfo pi) {
    const char *te;
    const char *s;
    char       *b;
    VALUE       src;
    size_t      len     = strlen(text);
    int         options = 0;

    // The text is Regexp#inspect output, so the shortest it can be is "//".
    // Below that te would be built from text - 1, which is not a pointer the
    // string owns.
    if (len < 2 || '/' != *text) {
        set_error(&pi->err, "Invalid regexp format", pi->str, pi->s);
        return Qundef;
    }
    te = text + len - 1;
#ifdef ONIG_OPTION_IGNORECASE
    for (; text < te && '/' != *te; te--) {
        switch (*te) {
        case 'i': options |= ONIG_OPTION_IGNORECASE; break;
        case 'm': options |= ONIG_OPTION_MULTILINE; break;
        case 'x': options |= ONIG_OPTION_EXTEND; break;
        default: break;
        }
    }
#endif
    // The scan above stops on the opening / when there is no closing one, and
    // without ONIG_OPTION_IGNORECASE it never runs at all. Either way the
    // length below goes negative if this is not checked.
    if (te <= text || '/' != *te) {
        set_error(&pi->err, "Invalid regexp format", pi->str, pi->s);
        return Qundef;
    }
    // rb_reg_new() takes bytes, so it can not be told the document's encoding
    // and leaves the source ASCII-8BIT. Going through the String also gets
    // Ruby's own rule, which promotes an ASCII only source to US-ASCII the way
    // a literal is.
    src = rb_str_new(0, te - text - 1);
    b   = RSTRING_PTR(src);
    // The dumper writes Regexp#inspect, which escapes a / in the source as \/,
    // so take that back off. A backslash pair is stepped over whole or the /
    // in "\\/" would be read as the escaped one.
    s = text + 1;
    while (s < te) {
        if ('\\' == *s && s + 1 < te) {
            if ('/' == s[1]) {
                *b++ = '/';
            } else {
                *b++ = *s;
                *b++ = s[1];
            }
            s += 2;
        } else {
            *b++ = *s++;
        }
    }
    rb_str_set_len(src, b - RSTRING_PTR(src));
    if (0 != pi->options->rb_enc) {
        rb_enc_associate(src, pi->options->rb_enc);
    }
    return rb_reg_new_str(src, options);
}

static void instruct(PInfo pi, const char *target, Attr attrs, const char *content) {
    if (0 == strcmp("xml", target)) {
        for (; 0 != attrs->name; attrs++) {
            if (0 == strcmp("encoding", attrs->name)) {
                pi->options->rb_enc = rb_enc_find(attrs->value);
            }
        }
    }
}

static void add_text(PInfo pi, char *text, size_t len, int closed) {
    Helper h = helper_stack_peek(&pi->helpers);

    if (!closed) {
        set_error(&pi->err, "Text not closed", pi->str, pi->s);
        return;
    }
    if (0 == h) {
        set_error(&pi->err, "Unexpected text", pi->str, pi->s);
        return;
    }
    if (DEBUG <= pi->options->trace) {
        char indent[128];

        fill_indent(pi, indent, sizeof(indent));
        printf("%s '%s' to type %c\n", indent, text, h->type);
    }
    switch (h->type) {
    case NoCode:
    case StringCode:
        h->obj = rb_str_new(text, len);
        if (0 != pi->options->rb_enc) {
            rb_enc_associate(h->obj, pi->options->rb_enc);
        }
        if (0 != pi->circ_array) {
            circ_array_set(pi->circ_array, h->obj, (unsigned long)pi->id);
        }
        break;
    case FixnumCode: {
        long n = 0;
        char c;
        int  neg = 0;

        if ('-' == *text) {
            neg = 1;
            text++;
        }
        for (; '\0' != *text; text++) {
            c = *text;
            if ('0' <= c && c <= '9') {
                n = n * 10 + (c - '0');
            } else {
                set_error(&pi->err, "bad number format", pi->str, pi->s);
                return;
            }
        }
        if (neg) {
            n = -n;
        }
        h->obj = LONG2NUM(n);
        break;
    }
    case FloatCode: h->obj = rb_float_new(strtod(text, 0)); break;
    case SymbolCode: h->obj = ox_sym_intern(text, strlen(text), NULL); break;
    case DateCode: {
        VALUE args[1];

        if (Qundef == (*args = parse_ulong(text, pi))) {
            return;
        }
        h->obj = rb_funcall2(ox_date_class, ox_jd_id, 1, args);
        break;
    }
    case TimeCode: h->obj = parse_time(text, ox_time_class); break;
    case String64Code: {
        unsigned long str_size = b64_orig_size(text);
        // Decode straight into the String. The size comes from the document, so
        // an ALLOCA_N of it is an unbounded stack allocation, and the heap the
        // String already needs costs nothing extra.
        VALUE v = rb_str_new(0, (long)str_size);

        from_base64(text, (uchar *)RSTRING_PTR(v), str_size + 1);
        if (0 != pi->options->rb_enc) {
            rb_enc_associate(v, pi->options->rb_enc);
        }
        if (0 != pi->circ_array) {
            // h->obj is still Qundef; add_element() parked the id in pi->id.
            circ_array_set(pi->circ_array, v, pi->id);
        }
        h->obj = v;
        break;
    }
    case Symbol64Code: {
        unsigned long str_size = b64_orig_size(text);
        VALUE         v        = rb_str_new(0, (long)str_size);
        char         *str      = RSTRING_PTR(v);

        from_base64(text, (uchar *)str, str_size + 1);
        h->obj = ox_sym_intern(str, strlen(str), NULL);
        RB_GC_GUARD(v);
        break;
    }
    case RegexpCode: {
        VALUE re;

        if ('/' == *text) {
            re = parse_regexp(text, pi);
        } else {
            unsigned long str_size = b64_orig_size(text);
            VALUE         v        = rb_str_new(0, (long)str_size);
            char         *str      = RSTRING_PTR(v);

            from_base64(text, (uchar *)str, str_size + 1);
            re = parse_regexp(str, pi);
            RB_GC_GUARD(v);
        }
        if (Qundef == re) {
            return;
        }
        h->obj = re;
        break;
    }
    case BignumCode: h->obj = rb_cstr_to_inum(text, 10, 1); break;
    case BigDecimalCode: h->obj = rb_funcall(rb_cObject, ox_bigdecimal_id, 1, rb_str_new2(text)); break;
    default: {
        // The rest are containers add_element() built. Replacing one with Qnil
        // left end_element() reading the nil back as an Array or a Struct.
        // skip_off delivers the indentation between children here.
        size_t i;

        for (i = 0; i < len; i++) {
            if (' ' != text[i] && '\t' != text[i] && '\n' != text[i] && '\r' != text[i]) {
                set_error(&pi->err, "Unexpected text", pi->str, pi->s);
                return;
            }
        }
        break;
    }
    }
}

static void add_element(PInfo pi, const char *ename, Attr attrs, int hasChildren) {
    Attr          a;
    Helper        h;
    unsigned long id;
    bool          index;
    ID            var;

    if (TRACE <= pi->options->trace) {
        char  buf[1024];
        char  indent[128];
        char *s   = buf;
        char *end = buf + sizeof(buf) - 2;

        s += snprintf(s, end - s, " <%s%s", (hasChildren) ? "" : "/", ename);
        for (a = attrs; 0 != a->name; a++) {
            s += snprintf(s, end - s, " %s=%s", a->name, a->value);
        }
        *s++ = '>';
        *s++ = '\0';
        if (DEBUG <= pi->options->trace) {
            printf("===== add element stack(%d) =====\n", helper_stack_depth(&pi->helpers));
            debug_stack(pi, buf);
        } else {
            fill_indent(pi, indent, sizeof(indent));
            printf("%s%s\n", indent, buf);
        }
    }
    if (helper_stack_empty(&pi->helpers)) {  // top level object
        if (0 != (id = get_id_from_attrs(pi, attrs))) {
            pi->circ_array = circ_array_new();
        }
    }
    if ('\0' != ename[1]) {
        set_error(&pi->err, "Invalid element name", pi->str, pi->s);
        return;
    }
    var      = get_var_sym_from_attrs(attrs, (void *)pi->options->rb_enc, &index);
    h        = helper_stack_push(&pi->helpers, var, Qundef, *ename);
    h->index = index;
    switch (h->type) {
    case NilClassCode: h->obj = Qnil; break;
    case TrueClassCode: h->obj = Qtrue; break;
    case FalseClassCode: h->obj = Qfalse; break;
    case StringCode:
        // h->obj will be replaced by add_text if it is called
        h->obj = ox_empty_string;
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
    case BigDecimalCode:
    case ComplexCode:
    case DateCode:
    case TimeCode:
    case RationalCode:  // sub elements read next
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
    case RangeCode: h->obj = rb_ary_new_from_args(3, Qnil, Qnil, Qfalse); break;
    case RawCode:
        if (hasChildren) {
            h->obj = ox_parse(pi->s, pi->end - pi->s, ox_gen_callbacks, &pi->s, pi->options, &pi->err);
            if (0 != pi->circ_array) {
                circ_array_set(pi->circ_array, h->obj, get_id_from_attrs(pi, attrs));
            }
        } else {
            h->obj = Qnil;
        }
        break;
    case ExceptionCode:
        if (Qundef == (h->obj = get_obj_from_attrs(attrs, pi, rb_eException))) {
            return;
        }
        if (0 != pi->circ_array && Qnil != h->obj) {
            circ_array_set(pi->circ_array, h->obj, get_id_from_attrs(pi, attrs));
        }
        break;
    case ObjectCode:
        if (Qundef == (h->obj = get_obj_from_attrs(attrs, pi, ox_bag_clas))) {
            return;
        }
        if (0 != pi->circ_array && Qnil != h->obj) {
            circ_array_set(pi->circ_array, h->obj, get_id_from_attrs(pi, attrs));
        }
        break;
    case StructCode:
        // Returns Qundef with no c attribute and, unlike the helpers above, does
        // not set the error itself. The Qundef reached rb_struct_aset().
        if (Qundef == (h->obj = get_struct_from_attrs(attrs))) {
            set_error(&pi->err, "Invalid element for object mode", pi->str, pi->s);
            return;
        }
        if (0 != pi->circ_array) {
            circ_array_set(pi->circ_array, h->obj, get_id_from_attrs(pi, attrs));
        }
        break;
    case ClassCode:
        if (Qundef == (h->obj = get_class_from_attrs(attrs, pi, ox_bag_clas))) {
            return;
        }
        break;
    case RefCode:
        h->obj = Qundef;
        if (0 != pi->circ_array) {
            h->obj = circ_array_get(pi->circ_array, get_id_from_attrs(pi, attrs));
        }
        if (Qundef == h->obj) {
            set_error(&pi->err, "Invalid circular reference", pi->str, pi->s);
            return;
        }
        break;
    default:
        set_error(&pi->err, "Invalid element name", pi->str, pi->s);
        return;
        break;
    }
    if (DEBUG <= pi->options->trace) {
        debug_stack(pi, "   -----------");
    }
}

static void end_element(PInfo pi, const char *ename) {
    if (TRACE <= pi->options->trace) {
        char indent[128];

        if (DEBUG <= pi->options->trace) {
            char buf[1024];

            printf("===== end element stack(%d) =====\n", helper_stack_depth(&pi->helpers));
            snprintf(buf, sizeof(buf) - 1, "</%s>", ename);
            debug_stack(pi, buf);
        } else {
            fill_indent(pi, indent, sizeof(indent));
            printf("%s</%s>\n", indent, ename);
        }
    }
    if (!helper_stack_empty(&pi->helpers)) {
        Helper h  = helper_stack_pop(&pi->helpers);
        Helper ph = helper_stack_peek(&pi->helpers);

        if (ox_empty_string == h->obj) {
            // special catch for empty strings
            h->obj = rb_str_new2("");
        } else if (Qundef == h->obj) {
            set_error(&pi->err, "Invalid element for object mode", pi->str, pi->s);
            return;
        } else if (RangeCode == h->type) {  // Expect an array of 3 elements.
            const VALUE *ap = RARRAY_PTR(h->obj);

            h->obj = rb_range_new(*ap, *(ap + 1), Qtrue == *(ap + 2));
        }
        pi->obj = h->obj;
        if (0 != ph) {
            switch (ph->type) {
            case ArrayCode: rb_ary_push(ph->obj, h->obj); break;
            case ExceptionCode:
            case ObjectCode:
                if (Qnil != ph->obj) {
                    // An index is a valid ID too, so a numeric a attribute set an
                    // instance variable named whatever was interned in that slot.
                    if (0 == h->var || h->index || NULL == rb_id2name(h->var)) {
                        set_error(&pi->err, "Invalid element for object mode", pi->str, pi->s);
                        return;
                    }
                    if (RUBY_T_OBJECT != rb_type(ph->obj)) {
                        set_error(&pi->err, "Corrupt object encoding", pi->str, pi->s);
                        return;
                    }
                    rb_ivar_set(ph->obj, h->var, h->obj);
                }
                break;
            case StructCode:
                // An ID landed in rb_struct_aset() as a Fixnum, so a name picked
                // whichever member that number happened to be.
                if (0 == h->var || !h->index) {
                    set_error(&pi->err, "Invalid element for object mode", pi->str, pi->s);
                    return;
                }
                rb_struct_aset(ph->obj, h->var, h->obj);
                break;
            case HashCode:
                // put back h
                helper_stack_push(&pi->helpers, h->var, h->obj, KeyCode)->index = h->index;
                break;
            case RangeCode:
                // An index can equal one of these IDs, so turn it away first.
                if (h->index) {
                    set_error(&pi->err, "Invalid range attribute", pi->str, pi->s);
                    return;
                }
                if (ox_beg_id == h->var) {
                    rb_ary_store(ph->obj, 0, h->obj);
                } else if (ox_end_id == h->var) {
                    rb_ary_store(ph->obj, 1, h->obj);
                } else if (ox_excl_id == h->var) {
                    rb_ary_store(ph->obj, 2, h->obj);
                } else {
                    set_error(&pi->err, "Invalid range attribute", pi->str, pi->s);
                    return;
                }
                break;
            case KeyCode: {
                Helper gh;

                helper_stack_pop(&pi->helpers);
                if (NULL == (gh = helper_stack_peek(&pi->helpers)) || Qundef == ph->obj || Qundef == h->obj) {
                    set_error(&pi->err, "Corrupt parse stack, container is wrong type", pi->str, pi->s);
                    return;
                }
                rb_hash_aset(gh->obj, ph->obj, h->obj);
            } break;
            case ComplexCode:
                if (Qundef == ph->obj) {
                    ph->obj = h->obj;
                } else {
                    ph->obj = rb_complex_new(ph->obj, h->obj);
                }
                break;
            case RationalCode: {
                if (Qundef == h->obj || RUBY_T_FIXNUM != rb_type(h->obj)) {
                    set_error(&pi->err, "Invalid object format", pi->str, pi->s);
                    return;
                }
                if (Qundef == ph->obj) {
                    ph->obj = h->obj;
                } else {
                    if (Qundef == ph->obj || RUBY_T_FIXNUM != rb_type(ph->obj)) {
                        set_error(&pi->err, "Corrupt parse stack, container is wrong type", pi->str, pi->s);
                        return;
                    }
#ifdef RUBINIUS_RUBY
                    ph->obj = rb_Rational(ph->obj, h->obj);
#else
                    ph->obj = rb_rational_new(ph->obj, h->obj);
#endif
                }
                break;
            }
            default:
                set_error(&pi->err, "Corrupt parse stack, container is wrong type", pi->str, pi->s);
                return;
                break;
            }
        }
    }
    if (0 != pi->circ_array && helper_stack_empty(&pi->helpers)) {
        ox_circ_array_free(pi->circ_array);
        pi->circ_array = 0;
    }
    if (DEBUG <= pi->options->trace) {
        debug_stack(pi, "   ----------");
    }
}

static VALUE parse_double_time(const char *text, VALUE clas) {
    time_t      v   = 0;
    long        v2  = 0;
    const char *dot = 0;
    char        c;
    bool        neg = false;

    // A time before the epoch is written as a sign followed by the magnitude.
    if ('-' == *text) {
        neg = true;
        text++;
    }
    for (; '.' != *text; text++) {
        c = *text;
        if (c < '0' || '9' < c) {
            return Qnil;
        }
        v = 10 * v + (time_t)(c - '0');
    }
    dot = text++;
    for (; '\0' != *text && text - dot <= 6; text++) {
        c = *text;
        if (c < '0' || '9' < c) {
            return Qnil;
        }
        v2 = 10 * v2 + (long)(c - '0');
    }
    for (; text - dot <= 9; text++) {
        v2 *= 10;
    }
    if (neg) {
        // rb_time_nano_new() wants a non negative nanosecond count, so the
        // fraction goes back into the second count the way dump_time_thin()
        // took it out.
        if (0 == v2) {
            v = -v;
        } else {
            v  = -v - 1;
            v2 = 1000000000L - v2;
        }
    }
    return rb_time_nano_new(v, v2);
}

typedef struct _tp {
    int  cnt;
    char end;
    char alt;
} *Tp;

static VALUE parse_xsd_time(const char *text, VALUE clas) {
    long       cargs[10];
    long      *cp = cargs;
    long       v;
    int        i;
    char       c;
    struct _tp tpa[10] = {{4, '-', '-'},
                          {2, '-', '-'},
                          {2, 'T', 'T'},
                          {2, ':', ':'},
                          {2, ':', ':'},
                          {2, '.', '.'},
                          {9, '+', '-'},
                          {2, ':', ':'},
                          {2, '\0', '\0'},
                          {0, '\0', '\0'}};
    Tp         tp      = tpa;
    long       nsec    = 0;
    long       offset;
    bool       neg = false;

    for (; 0 != tp->cnt; tp++) {
        for (i = tp->cnt, v = 0; 0 < i; text++, i--) {
            c = *text;
            if (c < '0' || '9' < c) {
                if (tp->end == c || tp->alt == c) {
                    break;
                }
                return Qnil;
            }
            v = 10 * v + (long)(c - '0');
        }
        // The fraction is the only field terminated by the offset sign, and the
        // sign is the only place the offset's direction is written down.
        if ('+' == tp->end) {
            nsec = v;
            for (; 0 < i; i--) {
                nsec *= 10;
            }
            neg = ('-' == *text);
        }
        c = *text++;
        if (tp->end != c && tp->alt != c) {
            return Qnil;
        }
        *cp++ = v;
    }
    offset = cargs[7] * 3600 + cargs[8] * 60;
    if (neg) {
        offset = -offset;
    }
    // mktime() would read the wall clock as local time and throw the offset
    // away, and returns -1 for everything before the epoch on Windows.
    return rb_time_nano_new(
        (time_t)(ox_epoch_from_civil(cargs[0], cargs[1], cargs[2], cargs[3], cargs[4], cargs[5]) - offset),
        nsec);
}

// debug functions
static void fill_indent(PInfo pi, char *buf, size_t size) {
    size_t cnt;

    if (0 < (cnt = helper_stack_depth(&pi->helpers))) {
        cnt *= 2;
        if (size < cnt + 1) {
            cnt = size - 1;
        }
        memset(buf, ' ', cnt);
        buf += cnt;
    }
    *buf = '\0';
}

static void debug_stack(PInfo pi, const char *comment) {
    char   indent[128];
    Helper h;

    fill_indent(pi, indent, sizeof(indent));
    printf("%s%s\n", indent, comment);
    if (!helper_stack_empty(&pi->helpers)) {
        for (h = pi->helpers.head; h < pi->helpers.tail; h++) {
            const char *clas = "---";
            const char *key  = "---";

            if (Qundef != h->obj) {
                VALUE c = rb_obj_class(h->obj);

                clas = rb_class2name(c);
            }
            if (0 != h->var) {
                if (h->index) {
                    VALUE v;

                    v   = rb_String(h->var);
                    key = StringValuePtr(v);
                } else if (ObjectCode == (h - 1)->type || ExceptionCode == (h - 1)->type ||
                           RangeCode == (h - 1)->type || StructCode == (h - 1)->type) {
                    key = rb_id2name(h->var);
                } else {
                    printf("%s*** corrupt stack ***\n", indent);
                }
            }
            printf("%s [%c] %s : %s\n", indent, h->type, clas, key);
        }
    }
}
