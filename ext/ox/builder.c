/* builder.c
 * Copyright (c) 2011, 2016 Peter Ohler
 * All rights reserved.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "err.h"
#include "ox.h"
#include "ruby.h"
#include "ruby/encoding.h"
#include "ruby/version.h"
#include "xml_check.h"
#include "xml_str.h"

#define MAX_DEPTH 128

static void builder_free(void *ptr);

static const rb_data_type_t ox_builder_type = {
    "Ox/builder",
    {
        NULL,
        builder_free,
        NULL,
    },
    0,
    0,
};

typedef struct _element {
    char *name;
    char  buf[64];
    long  len;
    bool  has_child;
    bool  non_text_child;
} *Element;

typedef struct _builder {
    struct _buf     buf;
    int             indent;
    char            encoding[64];
    int             depth;
    FILE           *file;
    struct _element stack[MAX_DEPTH];
    long            line;
    long            col;
    long            pos;
} *Builder;

static VALUE      builder_class   = Qundef;
static const char indent_spaces[] = "\n                                                                                "
                                    "                                                ";  // 128 spaces

// The : character is equivalent to 10. Used for replacement characters up to
// 10 characters long such as '&#x10FFFF;'. From
// https://www.w3.org/TR/2006/REC-xml11-20060816
#if 0
static const char	xml_friendly_chars[257] = "\
:::::::::11::1::::::::::::::::::\
11611156111111111111111111114141\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111";
#endif

// From 2.3 of the XML 1.1 spec. All over 0x20 except <&", > also. Builder
// uses double quotes for attributes.
static const char xml_attr_chars[257] = "\
:::::::::11::1::::::::::::::::::\
11611151111111111111111111114141\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111";

// From 3.1 of the XML 1.1 spec. All over 0x20 except <&, > also.
static const char xml_element_chars[257] = "\
:::::::::11::1::::::::::::::::::\
11111151111111111111111111114141\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111";

static void append_indent(Builder b) {
    if (0 >= b->indent) {
        return;
    }
    // pos and not the buffer: a file builder empties the buffer on every flush,
    // which would read as the start of the document again.
    if (0 < b->pos) {
        int cnt = (b->indent * (b->depth + 1)) + 1;

        if (sizeof(indent_spaces) <= (size_t)cnt) {
            cnt = sizeof(indent_spaces) - 1;
        }
        buf_append_string(&b->buf, indent_spaces, cnt);
        b->line++;
        b->col = cnt - 1;
        b->pos += cnt;
    }
}

// Returns the escaped size append_string() needs, and raises first if the value
// holds a character XML has no way to write. append_string() raises on the same
// byte, but only once the delimiters around the value are already in the buffer
// and, for an element, once the element is on the stack, and nothing takes those
// back. So the writers ask here before they write anything.
//
// Only a value whose escaped form is longer can hold one, since an invalid byte
// counts 10, so the common case is the one walk append_string() always did.
inline static size_t check_string(const char *str, size_t size, const char *table) {
    size_t xsize = xml_str_len((const unsigned char *)str, size, table);

    if (xsize != size) {
        const unsigned char *bad = xml_first_invalid((const unsigned char *)str, size);

        if (NULL != bad) {
            rb_raise(ox_syntax_error_class, "'\\#x%02x' is not a valid XML character.", *bad);
        }
    }
    return xsize;
}

// One of the instruct() attributes, which are written from the Hash rather than
// passed in. A value of the wrong type is left to the type check that already
// refuses it further down.
inline static void check_instruct_value(VALUE attrs, VALUE key) {
    volatile VALUE v = rb_hash_lookup(attrs, key);

    if (Qnil != v && rb_cString == rb_obj_class(v)) {
        check_unescaped(StringValuePtr(v), (size_t)RSTRING_LEN(v));
    }
}

// Resolves a Symbol or String name to bytes and checks it, for the writers that
// take either. sym holds the Symbol's name String for as long as *strp is used.
inline static long check_name(VALUE v, volatile VALUE *sym, const char **strp, size_t *xsizep, const char *msg) {
    long len;

    switch (rb_type(v)) {
    case T_STRING:
        *strp = StringValuePtr(v);
        len   = RSTRING_LEN(v);
        break;
    case T_SYMBOL:
        *sym  = rb_sym2str(v);
        *strp = RSTRING_PTR(*sym);
        len   = RSTRING_LEN(*sym);
        break;
    default: rb_raise(ox_arg_error_class, "%s", msg); break;
    }
    *xsizep = check_string(*strp, (size_t)len, xml_element_chars);

    return len;
}

static void
append_string(Builder b, const char *str, size_t size, const char *table, size_t xsize, bool strip_invalid_chars) {
    if (size == xsize) {
        const char *s   = str;
        const char *end = str + size;

        buf_append_string(&b->buf, str, size);
        b->col += size;
        s = strchr(s, '\n');
        while (NULL != s) {
            b->line++;
            b->col = end - s;
            s      = strchr(s + 1, '\n');
        }
        b->pos += size;
    } else {
        char        buf[256];
        char       *bp   = buf;
        char       *bend = buf + sizeof(buf) - 1;
        const char *send = str + size;

        while (str < send) {
            /* Copy runs of pass-through bytes a word at a time into the staging
             * buffer. A clean word has no byte below 0x20, so it can hold no
             * newline and no '\0': col and pos advance by the whole word and the
             * line is unchanged. On a hit the store is kept up to the first
             * flagged byte and the rest is redone by the loops below.
             */
            while (str + 8 <= send) {
                uint64_t v;
                uint64_t mask;

                memcpy(&v, str, 8);
                mask = xml_bytes_of_interest(v);
                if (bend < bp + 8) {
                    buf_append_string(&b->buf, buf, bp - buf);
                    bp = buf;
                }
                memcpy(bp, str, 8);
                if (0 != mask) {
                    int n = xml_first_of_interest((const unsigned char *)str, mask);

                    bp += n;
                    str += n;
                    b->col += n;
                    b->pos += n;
                    break;
                }
                bp += 8;
                str += 8;
                b->col += 8;
                b->pos += 8;
            }
            /* Pass-through bytes the word loop left: the tail shorter than a
             * word. These are all >= 0x20 and not escape characters, so no
             * newline check is needed.
             */
            while (str < send) {
                unsigned char c = (unsigned char)*str;

                if (c < 0x20 || '"' == c || '\'' == c || '&' == c || '<' == c || '>' == c) {
                    break;
                }
                if (bend <= bp) {
                    buf_append_string(&b->buf, buf, bp - buf);
                    bp = buf;
                }
                *bp++ = *str++;
                b->col++;
                b->pos++;
            }
            /* One byte the predicate flagged: an escape, an invalid character,
             * or a byte the table keeps unchanged ('"' and '\'' in the element
             * table, and 0x09/0x0a/0x0d in every table).
             */
            if (str < send) {
                int fcnt = table[(unsigned char)*str];

                if ('1' == fcnt) {
                    if (bend <= bp) {
                        buf_append_string(&b->buf, buf, bp - buf);
                        bp = buf;
                    }
                    if ('\n' == *str) {
                        b->line++;
                        b->col = 1;
                    } else {
                        b->col++;
                    }
                    b->pos++;
                    *bp++ = *str++;
                } else {
                    size_t written = (size_t)(fcnt - '0');

                    if (buf < bp) {
                        buf_append_string(&b->buf, buf, bp - buf);
                        bp = buf;
                    }
                    switch (*str) {
                    case '"': buf_append_string(&b->buf, "&quot;", 6); break;
                    case '&': buf_append_string(&b->buf, "&amp;", 5); break;
                    case '\'': buf_append_string(&b->buf, "&apos;", 6); break;
                    case '<': buf_append_string(&b->buf, "&lt;", 4); break;
                    case '>': buf_append_string(&b->buf, "&gt;", 4); break;
                    default:
                        // Must be one of the invalid characters. The table holds
                        // 10 for those, the longest escape, but nothing is
                        // written for them here.
                        if (!strip_invalid_chars) {
                            rb_raise(ox_syntax_error_class, "'\\#x%02x' is not a valid XML character.", *str);
                        }
                        written = 0;
                        break;
                    }
                    b->pos += written;
                    b->col += written;
                    str++;
                }
            }
        }
        if (buf < bp) {
            buf_append_string(&b->buf, buf, bp - buf);
            bp = buf;
        }
    }
}

static void append_sym_str(Builder b, VALUE v) {
    volatile VALUE sym = Qnil;
    const char    *s;
    size_t         xsize = 0;
    long           len   = check_name(v, &sym, &s, &xsize, "expected a Symbol or String");

    append_string(b, s, len, xml_element_chars, xsize, false);
}

static void i_am_a_child(Builder b, bool is_text) {
    if (0 <= b->depth) {
        Element e = &b->stack[b->depth];

        if (!e->has_child) {
            e->has_child = true;
            buf_append(&b->buf, '>');
            b->col++;
            b->pos++;
        }
        if (!is_text) {
            e->non_text_child = true;
        }
    }
}

static int append_attr(VALUE key, VALUE value, VALUE bv) {
    Builder        b   = (Builder)bv;
    volatile VALUE sym = Qnil;
    const char    *ks;
    long           klen;
    size_t         kx;
    size_t         vsize;
    size_t         vx;

    // Both halves before the space: an attribute is either written whole or not
    // at all, and the element it belongs to is left as it was.
    Check_Type(value, T_STRING);
    klen = check_name(key, &sym, &ks, &kx, "expected a Symbol or String");
    check_name_chars(ks, (size_t)klen, true);
    vsize = (size_t)RSTRING_LEN(value);
    vx    = check_string(StringValuePtr(value), vsize, xml_attr_chars);

    buf_append(&b->buf, ' ');
    b->col++;
    b->pos++;
    append_string(b, ks, klen, xml_element_chars, kx, false);
    buf_append_string(&b->buf, "=\"", 2);
    b->col += 2;
    b->pos += 2;
    append_string(b, StringValuePtr(value), vsize, xml_attr_chars, vx, false);
    buf_append(&b->buf, '"');
    b->col++;
    b->pos++;

    return ST_CONTINUE;
}

static void init(Builder b, int fd, int indent, long initial_size) {
    buf_init(&b->buf, fd, initial_size);
    b->indent    = indent;
    *b->encoding = '\0';
    b->depth     = -1;
    b->line      = 1;
    b->col       = 1;
    b->pos       = 0;
}

static void builder_free(void *ptr) {
    Builder b;
    Element e;
    int     d;

    if (0 == ptr) {
        return;
    }
    b = (Builder)ptr;
    buf_cleanup(&b->buf);
    for (e = b->stack, d = b->depth; 0 <= d; d--, e++) {
        if (e->name != e->buf) {
            free(e->name);
        }
    }
    xfree(ptr);
}

static void pop(Builder b) {
    Element e;

    if (0 > b->depth) {
        rb_raise(ox_arg_error_class, "closed too many elements");
    }
    e = &b->stack[b->depth];
    b->depth--;
    if (e->has_child) {
        if (e->non_text_child) {
            append_indent(b);
        }
        buf_append_string(&b->buf, "</", 2);
        append_string(b,
                      e->name,
                      e->len,
                      xml_element_chars,
                      xml_str_len((const unsigned char *)e->name, e->len, xml_element_chars),
                      false);
        buf_append(&b->buf, '>');
        // append_string() already counted the name, so this is only the "</"
        // and the ">".
        b->col += 3;
        b->pos += 3;
        if (e->buf != e->name) {
            free(e->name);
            e->name = 0;
        }
    } else {
        buf_append_string(&b->buf, "/>", 2);
        b->col += 2;
        b->pos += 2;
    }
}

static void bclose(Builder b) {
    while (0 <= b->depth) {
        pop(b);
    }
    if (0 <= b->indent) {
        buf_append(&b->buf, '\n');
    }
    b->line++;
    b->col = 1;
    b->pos++;
    buf_finish(&b->buf);
    if (NULL != b->file) {
        fclose(b->file);
    }
}

static VALUE to_s(Builder b) {
    volatile VALUE rstr;
    size_t         len;

    if (0 != b->buf.fd) {
        rb_raise(ox_arg_error_class, "can not create a String with a stream or file builder.");
    }
    len          = buf_len(&b->buf);
    *b->buf.tail = '\0';  // for debugging
    rstr         = rb_str_new(b->buf.head, len);
    // The closing newline goes on the String and not the buffer. Appended to the
    // buffer it stays there, so a to_s taken before the document is finished
    // leaves a newline in the middle of it, and in the middle of a value if that
    // is where the builder was.
    if (0 <= b->indent && (0 == len || '\n' != b->buf.head[len - 1])) {
        rb_str_cat(rstr, "\n", 1);
    }

    if ('\0' != *b->encoding) {
        rb_enc_associate(rstr, rb_enc_find(b->encoding));
    }
    return rstr;
}

/* call-seq: new(options)
 *
 * Creates a new Builder that will write to a string that can be retrieved with
 * the to_s() method. If a block is given it is executed with a single parameter
 * which is the builder instance. The return value is then the generated string.
 *
 * - +options+ - (Hash) formating options
 *   - +:indent+ (Fixnum) indentaion level, negative values excludes terminating newline
 *   - +:size+ (Fixnum) the initial size of the string buffer
 */
static VALUE builder_new(int argc, VALUE *argv, VALUE self) {
    Builder b        = ALLOC(struct _builder);
    int     indent   = ox_default_options.indent;
    long    buf_size = 0;

    if (1 == argc) {
        volatile VALUE v;

        rb_check_type(*argv, T_HASH);
        if (Qnil != (v = rb_hash_lookup(*argv, ox_indent_sym))) {
            if (rb_cInteger != rb_obj_class(v)) {
                rb_raise(ox_parse_error_class, ":indent must be a fixnum.\n");
            }
            indent = NUM2INT(v);
        }
        if (Qnil != (v = rb_hash_lookup(*argv, ox_size_sym))) {
            if (rb_cInteger != rb_obj_class(v)) {
                rb_raise(ox_parse_error_class, ":size must be a fixnum.\n");
            }
            buf_size = NUM2LONG(v);
        }
    }
    b->file = NULL;
    init(b, 0, indent, buf_size);

    if (rb_block_given_p()) {
        volatile VALUE rb = TypedData_Wrap_Struct(builder_class, &ox_builder_type, b);

        rb_yield(rb);
        bclose(b);

        return to_s(b);
    } else {
        return TypedData_Wrap_Struct(builder_class, &ox_builder_type, b);
    }
}

/* call-seq: file(filename, options)
 *
 * Creates a new Builder that will write to a file.
 *
 * - +filename+ (String) filename to write to
 * - +options+ - (Hash) formating options
 *   - +:indent+ (Fixnum) indentaion level, negative values excludes terminating newline
 *   - +:size+ (Fixnum) the initial size of the string buffer
 */
static VALUE builder_file(int argc, VALUE *argv, VALUE self) {
    Builder b        = ALLOC(struct _builder);
    int     indent   = ox_default_options.indent;
    long    buf_size = 0;
    FILE   *f;

    if (1 > argc) {
        rb_raise(ox_arg_error_class, "missing filename");
    }
    Check_Type(*argv, T_STRING);
    if (NULL == (f = fopen(StringValuePtr(*argv), "wb"))) {
        xfree(b);
        rb_raise(rb_eIOError, "%s\n", strerror(errno));
    }
    if (2 == argc) {
        volatile VALUE v;

        rb_check_type(argv[1], T_HASH);
        if (Qnil != (v = rb_hash_lookup(argv[1], ox_indent_sym))) {
            if (rb_cInteger != rb_obj_class(v)) {
                rb_raise(ox_parse_error_class, ":indent must be a fixnum.\n");
            }
            indent = NUM2INT(v);
        }
        if (Qnil != (v = rb_hash_lookup(argv[1], ox_size_sym))) {
            if (rb_cInteger != rb_obj_class(v)) {
                rb_raise(ox_parse_error_class, ":size must be a fixnum.\n");
            }
            buf_size = NUM2LONG(v);
        }
    }
    b->file = f;
    init(b, fileno(f), indent, buf_size);

    if (rb_block_given_p()) {
        volatile VALUE rb = TypedData_Wrap_Struct(builder_class, &ox_builder_type, b);
        rb_yield(rb);
        bclose(b);
        return Qnil;
    } else {
        return TypedData_Wrap_Struct(builder_class, &ox_builder_type, b);
    }
}

/* call-seq: io(io, options)
 *
 * Creates a new Builder that will write to an IO instance.
 *
 * - +io+ (String) IO to write to
 * - +options+ - (Hash) formating options
 *   - +:indent+ (Fixnum) indentaion level, negative values excludes terminating newline
 *   - +:size+ (Fixnum) the initial size of the string buffer
 */
static VALUE builder_io(int argc, VALUE *argv, VALUE self) {
    Builder        b        = ALLOC(struct _builder);
    int            indent   = ox_default_options.indent;
    long           buf_size = 0;
    int            fd;
    volatile VALUE v;

    if (1 > argc) {
        rb_raise(ox_arg_error_class, "missing IO object");
    }
    if (!rb_respond_to(*argv, ox_fileno_id) || Qnil == (v = rb_funcall(*argv, ox_fileno_id, 0)) ||
        0 == (fd = FIX2INT(v))) {
        rb_raise(rb_eIOError, "expected an IO that has a fileno.");
    }
    if (2 == argc) {
        volatile VALUE v;

        rb_check_type(argv[1], T_HASH);
        if (Qnil != (v = rb_hash_lookup(argv[1], ox_indent_sym))) {
            if (rb_cInteger != rb_obj_class(v)) {
                rb_raise(ox_parse_error_class, ":indent must be a fixnum.\n");
            }
            indent = NUM2INT(v);
        }
        if (Qnil != (v = rb_hash_lookup(argv[1], ox_size_sym))) {
            if (rb_cInteger != rb_obj_class(v)) {
                rb_raise(ox_parse_error_class, ":size must be a fixnum.\n");
            }
            buf_size = NUM2LONG(v);
        }
    }
    b->file = NULL;
    init(b, fd, indent, buf_size);

    if (rb_block_given_p()) {
        volatile VALUE rb = TypedData_Wrap_Struct(builder_class, &ox_builder_type, b);
        rb_yield(rb);
        bclose(b);
        return Qnil;
    } else {
        return TypedData_Wrap_Struct(builder_class, &ox_builder_type, b);
    }
}

/* call-seq: instruct(decl,options)
 *
 * Adds the top level <?xml?> element.
 *
 * - +decl+ - (String) 'xml' expected
 * - +options+ - (Hash) version or encoding
 */
static VALUE builder_instruct(int argc, VALUE *argv, VALUE self) {
    Builder b;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    if (0 < argc) {
        volatile VALUE nsym = Qnil;
        const char    *nstr;
        size_t         nx;

        check_name(*argv, &nsym, &nstr, &nx, "expected a Symbol or String");
        if (1 < argc && rb_cHash == rb_obj_class(argv[1])) {
            // The "<?" and the target are out by the time the values are
            // written, so a raise down there would leave them in the buffer.
            check_instruct_value(argv[1], ox_version_sym);
            check_instruct_value(argv[1], ox_encoding_sym);
            check_instruct_value(argv[1], ox_standalone_sym);
        }
    }
    i_am_a_child(b, false);
    append_indent(b);
    if (0 == argc) {
        buf_append_string(&b->buf, "<?xml?>", 7);
        b->col += 7;
        b->pos += 7;
    } else {
        volatile VALUE v;

        buf_append_string(&b->buf, "<?", 2);
        b->col += 2;
        b->pos += 2;
        append_sym_str(b, *argv);
        if (1 < argc && rb_cHash == rb_obj_class(argv[1])) {
            int len;

            if (Qnil != (v = rb_hash_lookup(argv[1], ox_version_sym))) {
                if (rb_cString != rb_obj_class(v)) {
                    rb_raise(ox_parse_error_class, ":version must be a Symbol.\n");
                }
                len = (int)RSTRING_LEN(v);
                buf_append_string(&b->buf, " version=\"", 10);
                buf_append_string(&b->buf, StringValuePtr(v), len);
                buf_append(&b->buf, '"');
                b->col += len + 11;
                b->pos += len + 11;
            }
            if (Qnil != (v = rb_hash_lookup(argv[1], ox_encoding_sym))) {
                if (rb_cString != rb_obj_class(v)) {
                    rb_raise(ox_parse_error_class, ":encoding must be a Symbol.\n");
                }
                len = (int)RSTRING_LEN(v);
                buf_append_string(&b->buf, " encoding=\"", 11);
                buf_append_string(&b->buf, StringValuePtr(v), len);
                buf_append(&b->buf, '"');
                b->col += len + 12;
                b->pos += len + 12;
                strncpy(b->encoding, StringValuePtr(v), sizeof(b->encoding));
                b->encoding[sizeof(b->encoding) - 1] = '\0';
            }
            if (Qnil != (v = rb_hash_lookup(argv[1], ox_standalone_sym))) {
                if (rb_cString != rb_obj_class(v)) {
                    rb_raise(ox_parse_error_class, ":standalone must be a Symbol.\n");
                }
                len = (int)RSTRING_LEN(v);
                buf_append_string(&b->buf, " standalone=\"", 13);
                buf_append_string(&b->buf, StringValuePtr(v), len);
                buf_append(&b->buf, '"');
                b->col += len + 14;
                b->pos += len + 14;
            }
        }
        buf_append_string(&b->buf, "?>", 2);
        b->col += 2;
        b->pos += 2;
    }
    return Qnil;
}

/* call-seq: element(name,attributes)
 *
 * Adds an element with the name and attributes provided. If a block is given
 * then on closing of the block a pop() is called.
 *
 * - +name+ - (String) name of the element
 * - +attributes+ - (Hash) of the element
 */
static VALUE builder_element(int argc, VALUE *argv, VALUE self) {
    Builder        b;
    Element        e;
    volatile VALUE sym   = Qnil;
    size_t         xsize = 0;
    const char    *name;
    long           len;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);

    if (1 > argc) {
        rb_raise(ox_arg_error_class, "missing element name");
    }
    // Everything that can raise has to run before the first write, or the raise
    // leaves an element started that no later call can finish or take back. It
    // also has to run before b->depth++, or the stack slot is never filled in.
    len = check_name(*argv, &sym, &name, &xsize, "expected a Symbol or String for an element name");
    check_name_chars(name, (size_t)len, false);
    if (MAX_DEPTH <= b->depth + 1) {
        rb_raise(ox_arg_error_class, "XML too deeply nested");
    }
    i_am_a_child(b, false);
    append_indent(b);
    b->depth++;
    e = &b->stack[b->depth];
    if (sizeof(e->buf) <= (size_t)len) {
        e->name = strdup(name);
        *e->buf = '\0';
    } else {
        strcpy(e->buf, name);
        e->name = e->buf;
    }
    e->len            = len;
    e->has_child      = false;
    e->non_text_child = false;

    buf_append(&b->buf, '<');
    b->col++;
    b->pos++;
    append_string(b, e->name, len, xml_element_chars, xsize, false);
    if (1 < argc && T_HASH == rb_type(argv[1])) {
        rb_hash_foreach(argv[1], append_attr, (VALUE)b);
    }
    // Do not close with > or /> yet. That is done with i_am_a_child() or pop().
    if (rb_block_given_p()) {
        rb_yield(self);
        pop(b);
    }
    return Qnil;
}

/* call-seq: void_element(name,attributes)
 *
 * Adds an void element with the name and attributes provided.
 *
 * - +name+ - (String) name of the element
 * - +attributes+ - (Hash) of the element
 */
static VALUE builder_void_element(int argc, VALUE *argv, VALUE self) {
    Builder        b;
    volatile VALUE sym   = Qnil;
    size_t         xsize = 0;
    const char    *name;
    long           len;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);

    if (1 > argc) {
        rb_raise(ox_arg_error_class, "missing element name");
    }
    len = check_name(*argv, &sym, &name, &xsize, "expected a Symbol or String for an element name");
    check_name_chars(name, (size_t)len, false);
    i_am_a_child(b, false);
    append_indent(b);
    buf_append(&b->buf, '<');
    b->col++;
    b->pos++;
    append_string(b, name, len, xml_element_chars, xsize, false);
    if (1 < argc && T_HASH == rb_type(argv[1])) {
        rb_hash_foreach(argv[1], append_attr, (VALUE)b);
    }
    buf_append_string(&b->buf, ">", 1);
    b->col++;
    ;
    b->pos++;

    return Qnil;
}

/* call-seq: comment(text)
 *
 * Adds a comment element to the XML string being formed.
 * - +text+ - (String) contents of the comment
 */
static VALUE builder_comment(VALUE self, VALUE text) {
    Builder b;
    size_t  size;
    size_t  xsize;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    rb_check_type(text, T_STRING);
    size  = (size_t)RSTRING_LEN(text);
    xsize = check_string(StringValuePtr(text), size, xml_element_chars);
    i_am_a_child(b, false);
    append_indent(b);
    buf_append_string(&b->buf, "<!--", 4);
    b->col += 4;
    b->pos += 4;
    append_string(b, StringValuePtr(text), size, xml_element_chars, xsize, false);
    buf_append_string(&b->buf, "-->", 3);
    b->col += 3;
    b->pos += 3;

    return Qnil;
}

/* call-seq: doctype(text)
 *
 * Adds a DOCTYPE element to the XML string being formed.
 * - +text+ - (String) contents of the doctype
 */
static VALUE builder_doctype(VALUE self, VALUE text) {
    Builder b;
    size_t  size;
    size_t  xsize;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    rb_check_type(text, T_STRING);
    size  = (size_t)RSTRING_LEN(text);
    xsize = check_string(StringValuePtr(text), size, xml_element_chars);
    i_am_a_child(b, false);
    append_indent(b);
    buf_append_string(&b->buf, "<!DOCTYPE ", 10);
    b->col += 10;
    b->pos += 10;
    append_string(b, StringValuePtr(text), size, xml_element_chars, xsize, false);
    buf_append(&b->buf, '>');
    b->col++;
    b->pos++;

    return Qnil;
}

/* call-seq: text(text)
 *
 * Adds a text element to the XML string being formed.
 * - +text+ - (String) contents of the text field
 * - +strip_invalid_chars+ - [true|false] strips any characters invalid for XML, defaults to false
 */
static VALUE builder_text(int argc, VALUE *argv, VALUE self) {
    Builder        b;
    volatile VALUE v;
    volatile VALUE strip_invalid_chars;
    size_t         size;
    size_t         xsize;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);

    if ((0 == argc) || (argc > 2)) {
        rb_raise(rb_eArgError, "wrong number of arguments (given %d, expected 1..2)", argc);
    }
    v = argv[0];
    if (2 == argc) {
        strip_invalid_chars = argv[1];
    } else {
        strip_invalid_chars = Qfalse;
    }

    v    = rb_String(v);
    size = (size_t)RSTRING_LEN(v);
    // Stripping is allowed to drop the byte, so it is the one writer that does
    // not have to refuse the value up front.
    xsize = RTEST(strip_invalid_chars) ? xml_str_len((const unsigned char *)StringValuePtr(v), size, xml_element_chars)
                                       : check_string(StringValuePtr(v), size, xml_element_chars);
    i_am_a_child(b, true);
    append_string(b, StringValuePtr(v), size, xml_element_chars, xsize, RTEST(strip_invalid_chars));

    return Qnil;
}

/* call-seq: cdata(data)
 *
 * Adds a CDATA element to the XML string being formed.
 * - +data+ - (String) contents of the CDATA element
 */
static VALUE builder_cdata(VALUE self, VALUE data) {
    Builder        b;
    volatile VALUE v = data;
    const char    *str;
    const char    *s;
    const char    *end;
    const char    *from;
    const char    *split;
    size_t         extra = 0;
    int            len;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);

    v    = rb_String(v);
    str  = StringValuePtr(v);
    len  = (int)RSTRING_LEN(v);
    s    = str;
    end  = str + len;
    from = str;
    check_unescaped(str, (size_t)len);
    i_am_a_child(b, false);
    append_indent(b);
    buf_append_string(&b->buf, "<![CDATA[", 9);
    b->col += 9;
    b->pos += 9;
    // See xml_cdata_end(). The ']]' stays in this section and the '>' opens the
    // next one, so from lands on the '>' each time round.
    while (NULL != (split = xml_cdata_end(from, end))) {
        buf_append_string(&b->buf, from, (split + 2) - from);
        buf_append_string(&b->buf, "]]><![CDATA[", CDATA_SPLIT_EXTRA);
        extra += CDATA_SPLIT_EXTRA;
        from = split + 2;
    }
    buf_append_string(&b->buf, from, end - from);
    b->col += len + extra;
    s = strchr(s, '\n');
    while (NULL != s) {
        b->line++;
        b->col = end - s;
        s      = strchr(s + 1, '\n');
    }
    b->pos += len + extra;
    buf_append_string(&b->buf, "]]>", 3);
    b->col += 3;
    b->pos += 3;

    return Qnil;
}

/* call-seq: raw(text)
 *
 * Adds the provided string directly to the XML without formatting or modifications.
 *
 * - +text+ - (String) contents to be added
 */
static VALUE builder_raw(VALUE self, VALUE text) {
    Builder        b;
    volatile VALUE v = text;
    const char    *str;
    const char    *s;
    const char    *end;
    int            len;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    v   = rb_String(v);
    str = StringValuePtr(v);
    len = (int)RSTRING_LEN(v);
    s   = str;
    end = str + len;
    i_am_a_child(b, true);
    buf_append_string(&b->buf, str, len);
    b->col += len;
    s = strchr(s, '\n');
    while (NULL != s) {
        b->line++;
        b->col = end - s;
        s      = strchr(s + 1, '\n');
    }
    b->pos += len;

    return Qnil;
}

/* call-seq: to_s()
 *
 * Returns the JSON document string in what ever state the construction is at.
 */
static VALUE builder_to_s(VALUE self) {
    Builder b;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    return to_s(b);
}

/* call-seq: line()
 *
 * Returns the current line in the output. The first line is line 1.
 */
static VALUE builder_line(VALUE self) {
    Builder b;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    return LONG2NUM(b->line);
}

/* call-seq: column()
 *
 * Returns the current column in the output. The first character in a line is at
 * column 1.
 */
static VALUE builder_column(VALUE self) {
    Builder b;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    return LONG2NUM(b->col);
}

/* call-seq: indent()
 *
 * Returns the indentation level
 */
static VALUE builder_get_indent(VALUE self) {
    Builder b;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    return INT2NUM(b->indent);
}

/* call-seq: indent=(indent)
 *
 * Sets the indentation level
 *
 * - +indent+ (Fixnum) indentaion level, negative values excludes terminating newline
 */
static VALUE builder_set_indent(VALUE self, VALUE indent) {
    Builder b;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);

    if (rb_cInteger != rb_obj_class(indent)) {
        rb_raise(ox_parse_error_class, "indent must be a fixnum.\n");
    }

    b->indent = NUM2INT(indent);
    return Qnil;
}

/* call-seq: pos()
 *
 * Returns the number of bytes written.
 */
static VALUE builder_pos(VALUE self) {
    Builder b;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    return LONG2NUM(b->pos);
}

/* call-seq: pop()
 *
 * Closes the current element.
 */
static VALUE builder_pop(VALUE self) {
    Builder b;
    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    pop(b);

    return Qnil;
}

/* call-seq: close()
 *
 * Closes the all elements and the document.
 */
static VALUE builder_close(VALUE self) {
    Builder b;

    TypedData_Get_Struct(self, struct _builder, &ox_builder_type, b);
    bclose(b);

    return Qnil;
}

/*
 * Document-class: Ox::Builder
 *
 * An XML builder.
 */
void ox_init_builder(VALUE ox) {
#if 0
    // Just for rdoc.
    ox = rb_define_module("Ox");
#endif
    builder_class = rb_define_class_under(ox, "Builder", rb_cObject);
#if RUBY_API_VERSION_CODE >= 30200
    rb_undef_alloc_func(builder_class);
#endif
    rb_define_module_function(builder_class, "new", builder_new, -1);
    rb_define_module_function(builder_class, "file", builder_file, -1);
    rb_define_module_function(builder_class, "io", builder_io, -1);
    rb_define_method(builder_class, "instruct", builder_instruct, -1);
    rb_define_method(builder_class, "comment", builder_comment, 1);
    rb_define_method(builder_class, "doctype", builder_doctype, 1);
    rb_define_method(builder_class, "element", builder_element, -1);
    rb_define_method(builder_class, "void_element", builder_void_element, -1);
    rb_define_method(builder_class, "text", builder_text, -1);
    rb_define_method(builder_class, "cdata", builder_cdata, 1);
    rb_define_method(builder_class, "raw", builder_raw, 1);
    rb_define_method(builder_class, "pop", builder_pop, 0);
    rb_define_method(builder_class, "close", builder_close, 0);
    rb_define_method(builder_class, "to_s", builder_to_s, 0);
    rb_define_method(builder_class, "line", builder_line, 0);
    rb_define_method(builder_class, "column", builder_column, 0);
    rb_define_method(builder_class, "pos", builder_pos, 0);
    rb_define_method(builder_class, "indent", builder_get_indent, 0);
    rb_define_method(builder_class, "indent=", builder_set_indent, 1);
}
