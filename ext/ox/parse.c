/* parse.c
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "attr.h"
#include "err.h"
#include "helper.h"
#include "intern.h"
#include "ox.h"
#include "ruby.h"
#include "special.h"

#define MAX_ELEMENT_DEPTH 1000

static void  mark_pi_cb(void *ptr);
static void  read_instruction(PInfo pi);
static void  read_doctype(PInfo pi);
static void  read_comment(PInfo pi);
static char *read_element(PInfo pi, int depth);
static void  read_text(PInfo pi);
/*static void	  read_reduced_text(PInfo pi); */
static void  read_cdata(PInfo pi);
static char *read_name_token(PInfo pi);
static char *read_quoted_value(PInfo pi);
static char *read_hex_uint64(char *b, uint64_t *up);
static char *read_10_uint64(char *b, uint64_t *up);
static char *read_coded_chars(PInfo pi, char *text);
static void  next_non_white(PInfo pi);
static int   collapse_special(PInfo pi, char *str);

static const rb_data_type_t ox_wrap_type = {
    "Object",
    {
        mark_pi_cb,
        NULL,
        NULL,
    },
    0,
    0,
};

/* This XML parser is a single pass, destructive, callback parser. It is a
 * single pass parse since it only make one pass over the characters in the
 * XML document string. It is destructive because it re-uses the content of
 * the string for values in the callback and places \0 characters at various
 * places to mark the end of tokens and strings. It is a callback parser like
 * a SAX parser because it uses callback when document elements are
 * encountered.
 *
 * Parsing is very tolerant. Lack of headers and even mispelled element
 * endings are passed over without raising an error. A best attempt is made in
 * all cases to parse the string.
 */

static char xml_valid_lower_chars[34] = "xxxxxxxxxooxxoxxxxxxxxxxxxxxxxxxo";

inline static int is_white(char c) {
    switch (c) {
    case ' ':
    case '\t':
    case '\f':
    case '\n':
    case '\r': return 1;
    default: return 0;
    }
}

inline static void next_non_white(PInfo pi) {
    for (; 1; pi->s++) {
        switch (*pi->s) {
        case ' ':
        case '\t':
        case '\f':
        case '\n':
        case '\r': break;
        default: return;
        }
    }
}

inline static void next_white(PInfo pi) {
    for (; 1; pi->s++) {
        switch (*pi->s) {
        case ' ':
        case '\t':
        case '\f':
        case '\n':
        case '\r':
        case '\0': return;
        default: break;
        }
    }
}

static void fix_newlines(char *buf) {
#if HAVE_INDEX
    if (NULL != index(buf, '\r')) {
#endif
        char *s = buf;
        char *d = buf;

        for (; '\0' != *s; s++) {
            if ('\r' == *s) {
                if ('\n' == *(s + 1)) {
                    continue;
                }
                *d = '\n';
            } else if (d < s) {
                *d = *s;
            }
            d++;
        }
        *d = '\0';
#if HAVE_INDEX
    }
#endif
}

static void mark_pi_cb(void *ptr) {
    if (NULL != ptr) {
        HelperStack stack = &((PInfo)ptr)->helpers;
        Helper      h;

        for (h = stack->head; h < stack->tail; h++) {
            if (NoCode != h->type) {
                rb_gc_mark(h->obj);
            }
        }
    }
}

// State shared between the parse body and its ensure block. Bundled so the
// whole parse can run under rb_ensure and clean up even when a callback raises.
typedef struct _parseCtx {
    PInfo  pi;
    char **endp;
    Err    err;
    VALUE  wrap;
    int    block_given;
} *ParseCtx;

static VALUE ox_parse_body(VALUE ctxv) {
    ParseCtx ctx         = (ParseCtx)ctxv;
    PInfo    pi          = ctx->pi;
    char   **endp        = ctx->endp;
    Err      err         = ctx->err;
    int      block_given = ctx->block_given;
    int      body_read   = 0;

    while (1) {
        next_non_white(pi);  // skip white space
        if ('\0' == *pi->s) {
            break;
        }
        if (body_read && 0 != endp) {
            *endp = pi->s;
            break;
        }
        if ('<' != *pi->s) {  // all top level entities start with <
            set_error(err, "invalid format, expected <", pi->str, pi->s);
            return Qnil;
        }
        pi->s++;  // past <
        switch (*pi->s) {
        case '?':  // processing instruction
            pi->s++;
            read_instruction(pi);
            break;
        case '!':  // comment or doctype
            pi->s++;
            if ('\0' == *pi->s) {
                set_error(err, "invalid format, DOCTYPE or comment not terminated", pi->str, pi->s);
                return Qnil;
            } else if ('-' == *pi->s) {
                pi->s++;  // skip -
                if ('-' != *pi->s) {
                    set_error(err, "invalid format, bad comment format", pi->str, pi->s);
                    return Qnil;
                } else {
                    pi->s++;  // skip second -
                    read_comment(pi);
                }
            } else if ((TolerantEffort == pi->options->effort) ? 0 == strncasecmp("DOCTYPE", pi->s, 7)
                                                               : 0 == strncmp("DOCTYPE", pi->s, 7)) {
                pi->s += 7;
                read_doctype(pi);
            } else {
                set_error(err, "invalid format, DOCTYPE or comment expected", pi->str, pi->s);
                return Qnil;
            }
            break;
        case '\0': set_error(err, "invalid format, document not terminated", pi->str, pi->s); return Qnil;
        default:
            read_element(pi, 0);
            body_read = 1;
            break;
        }
        if (err_has(&pi->err)) {
            *err = pi->err;
            return Qnil;
        }
        if (block_given && Qnil != pi->obj && Qundef != pi->obj) {
            if (NULL != pi->pcb->finish) {
                pi->pcb->finish(pi);
            }
            rb_yield(pi->obj);
        }
    }
    if (NULL != pi->pcb->finish) {
        pi->pcb->finish(pi);
    }
    return pi->obj;
}

static VALUE ox_parse_ensure(VALUE ctxv) {
    ParseCtx ctx = (ParseCtx)ctxv;

    // pi is a stack value wrapped for GC marking. Detach the wrapper before
    // this frame is gone so a later conservative GC can not mark a dead helper
    // stack, and free the helper stack whether the parse returned normally or a
    // callback raised out of it.
    if (Qnil != ctx->wrap) {
        DATA_PTR(ctx->wrap) = NULL;
    }
    helper_stack_cleanup(&ctx->pi->helpers);
    // end_element() only frees the circular reference table on a complete
    // parse, so release it here for an error or a callback raising out.
    if (0 != ctx->pi->circ_array) {
        ox_circ_array_free(ctx->pi->circ_array);
        ctx->pi->circ_array = 0;
    }
    // Same for hash mode's mark list, freed by finish() only if the loop ends.
    xfree(ctx->pi->marked);
    ctx->pi->marked    = NULL;
    ctx->pi->mark_size = 0;
    ctx->pi->mark_cnt  = 0;
    return Qnil;
}

VALUE
ox_parse(char *xml, size_t len, ParseCallbacks pcb, char **endp, Options options, Err err) {
    struct _pInfo    pi;
    struct _parseCtx ctx;

    if (0 == xml) {
        set_error(err, "Invalid arg, xml string can not be null", xml, 0);
        return Qnil;
    }
    if (DEBUG <= options->trace) {
        printf("Parsing xml:\n%s\n", xml);
    }
    // initialize parse info
    helper_stack_init(&pi.helpers);
    err_init(&pi.err);
    pi.str        = xml;
    pi.end        = pi.str + len;
    pi.s          = xml;
    pi.pcb        = pcb;
    pi.obj        = Qnil;
    pi.circ_array = 0;
    pi.options    = options;
    pi.marked     = NULL;
    pi.mark_size  = 0;
    pi.mark_cnt   = 0;

    ctx.pi          = &pi;
    ctx.endp        = endp;
    ctx.err         = err;
    ctx.block_given = rb_block_given_p();
    // Protect against GC. The wrapper marks the helper stack while parsing;
    // ox_parse_ensure detaches it on every exit, including a callback raise.
    ctx.wrap = TypedData_Wrap_Struct(rb_cObject, &ox_wrap_type, &pi);

    return rb_ensure(ox_parse_body, (VALUE)&ctx, ox_parse_ensure, (VALUE)&ctx);
}

// Entered after the "<?" sequence. Ready to read the rest.
static void read_instruction(PInfo pi) {
    char              content[256];
    char             *content_ptr = content;
    struct _attrStack attrs;
    char             *attr_name;
    char             *attr_value;
    char             *target;
    char             *end;
    char              c;
    char             *cend;
    size_t            size;
    bool              attrs_ok = true;

    *content = '\0';
    attr_stack_init(&attrs);
    if (0 == (target = read_name_token(pi))) {
        goto CLEANUP;
    }
    end = pi->s;
    for (; true; pi->s++) {
        switch (*pi->s) {
        case '?':
            if ('>' == *(pi->s + 1)) {
                pi->s++;
                goto DONE;
            }
            break;
        case '\0': set_error(&pi->err, "processing instruction not terminated", pi->str, pi->s); goto CLEANUP;
        default: break;
        }
    }
DONE:
    cend  = pi->s;
    size  = cend - end - 1;
    pi->s = end;
    if (size < sizeof(content)) {
        content_ptr = content;
    } else {
        content_ptr = ALLOC_N(char, size + 1);
    }
    memcpy(content_ptr, end, size);
    content_ptr[size] = '\0';

    next_non_white(pi);
    c    = *pi->s;
    *end = '\0';  // terminate name
    if ('?' != c) {
        while ('?' != c) {
            pi->last = 0;
            if ('\0' == *pi->s) {
                set_error(&pi->err, "invalid format, processing instruction not terminated", pi->str, pi->s);
                goto CLEANUP;
            }
            next_non_white(pi);
            if (0 == (attr_name = read_name_token(pi))) {
                goto CLEANUP;
            }
            end = pi->s;
            next_non_white(pi);
            if ('\0' == *pi->s) {
                set_error(&pi->err, "invalid format, processing instruction not terminated", pi->str, pi->s);
                goto CLEANUP;
            }
            if ('=' != *pi->s++) {
                attrs_ok = false;
                break;
            }
            *end = '\0';  // terminate name
            // read value
            next_non_white(pi);
            if (0 == (attr_value = read_quoted_value(pi))) {
                goto CLEANUP;
            }
            attr_stack_push(&attrs, attr_name, attr_value);
            next_non_white(pi);
            if ('\0' == pi->last) {
                c = *pi->s;
            } else {
                c = pi->last;
            }
        }
        if ('?' == *pi->s) {
            pi->s++;
        }
    } else {
        pi->s++;
    }
    if (attrs_ok) {
        if ('>' != *pi->s++) {
            set_error(&pi->err, "invalid format, processing instruction not terminated", pi->str, pi->s);
            goto CLEANUP;
        }
    } else {
        pi->s = cend + 1;
    }
    if (0 != pi->pcb->instruct) {
        if (attrs_ok) {
            pi->pcb->instruct(pi, target, attrs.head, 0);
        } else {
            pi->pcb->instruct(pi, target, attrs.head, content_ptr);
        }
    } else {
        for (Attr a = attrs.head; a < attrs.tail; a++) {
            if (0 == strcasecmp(a->name, "encoding")) {
                strncpy(pi->options->encoding, a->value, sizeof(pi->options->encoding) - 1);
                pi->options->encoding[sizeof(pi->options->encoding) - 1] = '\0';
                pi->options->rb_enc                                      = rb_enc_find(a->value);
                break;
            }
        }
    }
    // Single exit, so no error return can leak the heap content buffer or the
    // heap-grown attribute stack.
CLEANUP:
    attr_stack_cleanup(&attrs);
    if (content_ptr != content) {
        xfree(content_ptr);
    }
}

static void read_delimited(PInfo pi, char end) {
    char c;

    if ('"' == end || '\'' == end) {
        for (c = *pi->s++; end != c; c = *pi->s++) {
            if ('\0' == c) {
                pi->s--;
                set_error(&pi->err, "invalid format, doctype not terminated", pi->str, pi->s);
                return;
            }
        }
    } else {
        while (1) {
            c = *pi->s++;
            if (end == c) {
                return;
            }
            if (MAX_PROLOG < (pi->s - pi->str)) {
                pi->s--;
                set_error(&pi->err, "prolog (doctype) too long", pi->str, pi->s);
                return;
            }
            switch (c) {
            case '\0':
                pi->s--;
                set_error(&pi->err, "invalid format, doctype not terminated", pi->str, pi->s);
                return;
            case '"': read_delimited(pi, c); break;
            case '\'': read_delimited(pi, c); break;
            case '[': read_delimited(pi, ']'); break;
            case '<': read_delimited(pi, '>'); break;
            default: break;
            }
        }
    }
}

// Entered after the "<!DOCTYPE" sequence plus the first character after
// that. Ready to read the rest.
static void read_doctype(PInfo pi) {
    char *doctype;

    next_non_white(pi);
    doctype = pi->s;
    read_delimited(pi, '>');
    if (err_has(&pi->err)) {
        return;
    }
    pi->s--;
    *pi->s = '\0';
    pi->s++;
    if (0 != pi->pcb->add_doctype) {
        fix_newlines(doctype);
        pi->pcb->add_doctype(pi, doctype);
    }
}

// Entered after "<!--". Returns error code.
static void read_comment(PInfo pi) {
    char *end;
    char *s;
    char *comment;
    int   done = 0;

    next_non_white(pi);
    comment = pi->s;
    end     = strstr(pi->s, "-->");
    if (0 == end) {
        set_error(&pi->err, "invalid format, comment not terminated", pi->str, pi->s);
        return;
    }
    for (s = end - 1; pi->s < s && !done; s--) {
        switch (*s) {
        case ' ':
        case '\t':
        case '\f':
        case '\n':
        case '\r': break;
        default:
            *(s + 1) = '\0';
            done     = 1;
            break;
        }
    }
    *end  = '\0';  // in case the comment was blank
    pi->s = end + 3;
    if (0 != pi->pcb->add_comment) {
        fix_newlines(comment);
        pi->pcb->add_comment(pi, comment);
    }
}

// Entered after the '<' and the first character after that. Returns stat
// code.
static char *read_element(PInfo pi, int depth) {
    struct _attrStack attrs;
    const char       *attr_name;
    const char       *attr_value;
    char             *name;
    char             *ename;
    char             *end;
    char              c;
    long              elen;
    int               hasChildren = 0;
    int               done        = 0;

    if (MAX_ELEMENT_DEPTH < depth) {
        set_error(&pi->err, "element nested too deeply, limit is 1000", pi->str, pi->s);
        return 0;
    }
    attr_stack_init(&attrs);
    if (0 == (ename = read_name_token(pi))) {
        return 0;
    }
    end  = pi->s;
    elen = end - ename;
    next_non_white(pi);
    c    = *pi->s;
    *end = '\0';
    if ('/' == c) {
        // empty element, no attributes and no children
        pi->s++;
        if ('>' != *pi->s) {
            attr_stack_cleanup(&attrs);
            set_error(&pi->err, "invalid format, element not closed", pi->str, pi->s);
            return 0;
        }
        pi->s++; /* past > */
        pi->pcb->add_element(pi, ename, attrs.head, hasChildren);
        pi->pcb->end_element(pi, ename);

        attr_stack_cleanup(&attrs);
        return 0;
    }
    /* read attribute names until the close (/ or >) is reached */
    while (!done) {
        if ('\0' == c) {
            if (pi->end <= pi->s) {
                break;
            }
            next_non_white(pi);
            c = *pi->s;
        }
        pi->last = 0;
        switch (c) {
        case '\0':
            attr_stack_cleanup(&attrs);
            set_error(&pi->err, "invalid format, document not terminated", pi->str, pi->s);
            return 0;
        case '/':
            /* Element with just attributes. */
            pi->s++;
            if ('>' != *pi->s) {
                attr_stack_cleanup(&attrs);
                set_error(&pi->err, "invalid format, element not closed", pi->str, pi->s);
                return 0;
            }
            pi->s++;
            pi->pcb->add_element(pi, ename, attrs.head, hasChildren);
            pi->pcb->end_element(pi, ename);
            attr_stack_cleanup(&attrs);

            return 0;
        case '>':
            /* has either children or a value */
            pi->s++;
            hasChildren = 1;
            done        = 1;
            pi->pcb->add_element(pi, ename, attrs.head, hasChildren);

            break;
        default:
            /* Attribute name so it's an element and the attribute will be */
            /* added to it. */
            if (0 == (attr_name = read_name_token(pi))) {
                attr_stack_cleanup(&attrs);
                return 0;
            }
            end = pi->s;
            next_non_white(pi);
            if ('=' != *pi->s++) {
                if (TolerantEffort == pi->options->effort) {
                    pi->s--;
                    pi->last   = *pi->s;
                    *end       = '\0'; /* terminate name */
                    attr_value = "";
                    attr_stack_push(&attrs, attr_name, attr_value);
                    break;
                } else {
                    attr_stack_cleanup(&attrs);
                    pi->s--;
                    set_error(&pi->err, "invalid format, no attribute value", pi->str, pi->s);
                    return 0;
                }
            }
            *end = '\0'; /* terminate name */
            /* read value */
            next_non_white(pi);
            if (0 == (attr_value = read_quoted_value(pi))) {
                attr_stack_cleanup(&attrs);
                return 0;
            }
            if (pi->options->convert_special && 0 != strchr(attr_value, '&')) {
                if (0 != collapse_special(pi, (char *)attr_value) || err_has(&pi->err)) {
                    attr_stack_cleanup(&attrs);
                    return 0;
                }
            }
            attr_stack_push(&attrs, attr_name, attr_value);
            break;
        }
        if ('\0' == pi->last) {
            c = '\0';
        } else {
            c        = pi->last;
            pi->last = '\0';
        }
    }
    if (hasChildren) {
        char *start;
        int   first = 1;

        done = 0;
        /* read children */
        while (!done) {
            start = pi->s;
            next_non_white(pi);
            if (OffSkip == pi->options->skip && start < pi->s && '<' == *pi->s) {
                c      = *pi->s;
                *pi->s = '\0';
                pi->pcb->add_text(pi, start, (size_t)(pi->s - start), 1);
                *pi->s = c;
            }
            c = *pi->s++;
            if ('\0' == c) {
                attr_stack_cleanup(&attrs);
                set_error(&pi->err, "invalid format, document not terminated", pi->str, pi->s - 1);
                return 0;
            }
            if ('<' == c) {
                char *slash;

                switch (*pi->s) {
                case '!': /* better be a comment or CDATA */
                    pi->s++;
                    if ('-' == *pi->s && '-' == *(pi->s + 1)) {
                        pi->s += 2;
                        read_comment(pi);
                    } else if ((TolerantEffort == pi->options->effort) ? 0 == strncasecmp("[CDATA[", pi->s, 7)
                                                                       : 0 == strncmp("[CDATA[", pi->s, 7)) {
                        pi->s += 7;
                        read_cdata(pi);
                    } else {
                        attr_stack_cleanup(&attrs);
                        set_error(&pi->err, "invalid format, invalid comment or CDATA format", pi->str, pi->s);
                        return 0;
                    }
                    break;
                case '?': /* processing instruction */
                    pi->s++;
                    read_instruction(pi);
                    break;
                case '/':
                    slash = pi->s;
                    pi->s++;
                    if (0 == (name = read_name_token(pi))) {
                        attr_stack_cleanup(&attrs);
                        return 0;
                    }
                    end = pi->s;
                    next_non_white(pi);
                    c    = *pi->s;
                    *end = '\0';
                    if (0 !=
                        ((TolerantEffort == pi->options->effort) ? strcasecmp(name, ename) : strcmp(name, ename))) {
                        attr_stack_cleanup(&attrs);
                        if (TolerantEffort == pi->options->effort) {
                            pi->pcb->end_element(pi, ename);
                            return name;
                        } else {
                            set_error(&pi->err, "invalid format, elements overlap", pi->str, pi->s);
                            return 0;
                        }
                    }
                    if ('>' != c) {
                        attr_stack_cleanup(&attrs);
                        set_error(&pi->err, "invalid format, element not closed", pi->str, pi->s);
                        return 0;
                    }
                    if (first && start != slash - 1) {
                        // Some white space between start and here so add as
                        // text after checking skip.
                        *(slash - 1) = '\0';
                        switch (pi->options->skip) {
                        case CrSkip: {
                            char *s = start;
                            char *e = start;

                            for (; '\0' != *e; e++) {
                                if ('\r' != *e) {
                                    *s++ = *e;
                                }
                            }
                            *s = '\0';
                            break;
                        }
                        case SpcSkip: *start = '\0'; break;
                        case NoSkip:
                        case OffSkip:
                        default: break;
                        }
                        if ('\0' != *start) {
                            // The skip handling above moves the terminator, so
                            // the length is whatever survived it.
                            pi->pcb->add_text(pi, start, strlen(start), 1);
                        }
                    }
                    pi->s++;
                    pi->pcb->end_element(pi, ename);
                    attr_stack_cleanup(&attrs);
                    return 0;
                case '\0':
                    attr_stack_cleanup(&attrs);
                    if (TolerantEffort == pi->options->effort) {
                        return 0;
                    } else {
                        set_error(&pi->err, "invalid format, document not terminated", pi->str, pi->s);
                        return 0;
                    }
                default:
                    first = 0;
                    /* a child element */
                    // Child closed with mismatched name.
                    if (0 != (name = read_element(pi, depth + 1))) {
                        attr_stack_cleanup(&attrs);

                        if (0 ==
                            ((TolerantEffort == pi->options->effort) ? strcasecmp(name, ename) : strcmp(name, ename))) {
                            pi->s++;
                            pi->pcb->end_element(pi, ename);
                            return 0;
                        } else {  // not the correct element yet
                            pi->pcb->end_element(pi, ename);
                            return name;
                        }
                    } else if (err_has(&pi->err)) {
                        attr_stack_cleanup(&attrs);
                        return 0;
                    }
                    break;
                }
            } else { /* read as TEXT */
                char prev = *(start - 1);

                pi->s = start;
                if ('>' != prev && (' ' <= prev || is_white(prev))) {
                    pi->s--;
                }
                read_text(pi);
                /*read_reduced_text(pi); */

                if (err_has(&pi->err)) {
                    attr_stack_cleanup(&attrs);
                    return 0;
                }

                /* to exit read_text with no errors the next character must be < */
                if ('/' == *(pi->s + 1) &&
                    0 == ((TolerantEffort == pi->options->effort) ? strncasecmp(ename, pi->s + 2, elen)
                                                                  : strncmp(ename, pi->s + 2, elen)) &&
                    '>' == *(pi->s + elen + 2)) {
                    /* close tag after text so treat as a value */
                    pi->s += elen + 3;
                    pi->pcb->end_element(pi, ename);
                    attr_stack_cleanup(&attrs);
                    return 0;
                }
            }
        }
    }
    attr_stack_cleanup(&attrs);
    return 0;
}

#define TEXT_ONES 0x0101010101010101ULL
#define TEXT_HIGH 0x8080808080808080ULL

/* Sets the high bit of every byte of a word that read_text can not copy
 * through as is, that is a byte less than or equal to 0x20, a '&', or a '<'.
 * Bytes with the high bit set are never flagged because ~v clears them, which
 * matches the byte loop where a signed char with the high bit set is negative
 * and so falls to the plain copy.
 *
 * A borrow out of one byte can also flag the byte above it, but a borrow is
 * only produced by a byte that matched, so the lowest flagged byte is always a
 * real match and there are never false negatives.
 */
inline static uint64_t text_bytes_of_interest(uint64_t v) {
    uint64_t a = v ^ (TEXT_ONES * (uint64_t)'&');
    uint64_t l = v ^ (TEXT_ONES * (uint64_t)'<');

    return (((v - TEXT_ONES * 0x21) & ~v) | ((a - TEXT_ONES) & ~a) | ((l - TEXT_ONES) & ~l)) & TEXT_HIGH;
}

/* Offset of the lowest flagged byte, which is the first byte of the word that
 * has to go through the switch below. Only ever called with a non-zero mask.
 */
inline static int text_first_of_interest(const char *s, uint64_t mask) {
#if defined(__GNUC__) || defined(__clang__)
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    (void)s;
    return (int)(__builtin_clzll(mask) >> 3);
#else
    (void)s;
    return (int)(__builtin_ctzll(mask) >> 3);
#endif
#else
    int i;

    (void)mask;
    for (i = 0; i < 8; i++) {
        unsigned char u = (unsigned char)s[i];

        if (u <= 0x20 || '&' == u || '<' == u) {
            break;
        }
    }
    return i;
#endif
}

static void read_text(PInfo pi) {
    char   buf[MAX_TEXT_LEN];
    char  *b         = buf;
    char  *alloc_buf = 0;
    char  *end       = b + sizeof(buf) - 2;
    char  *text;
    size_t len;
    char   c;
    int    done = 0;
    /* fix_newlines only rewrites '\r'. SpcSkip folds every '\r' to a space, so
     * the only way one reaches buf under it is an entity like &#13;. Any other
     * skip mode can keep a '\r', so assume one might be present there and run
     * fix_newlines as before. SpcSkip text with no such entity, the common
     * case, then skips the fix_newlines scan and second pass entirely.
     */
    int maybe_cr = (SpcSkip != pi->options->skip);

    while (!done) {
        /* A byte above 0x20 that is neither '&' nor '<' is copied through
         * unchanged for every skip mode and every effort, so runs of them can
         * be moved a word at a time instead of one switch dispatch per byte.
         * pi->end addresses the terminating '\0' so the loads never leave the
         * document, which keeps the loop clean under ASan.
         */
        while (pi->s + 8 <= pi->end && b + 8 <= end) {
            uint64_t v;
            uint64_t mask;

            memcpy(&v, pi->s, 8);
            mask = text_bytes_of_interest(v);
            /* b + 8 <= end so the whole word can be stored even when only part
             * of it is kept. The bytes past the kept prefix are overwritten by
             * the next copy or cut off by the terminating '\0'.
             */
            memcpy(b, pi->s, 8);
            if (0 != mask) {
                int n = text_first_of_interest(pi->s, mask);

                b += n;
                pi->s += n;
                break;
            }
            b += 8;
            pi->s += 8;
        }
        /* Only reached when a bound stopped the word loop, or for the byte the
         * word loop stopped on.
         */
        while (pi->s < pi->end && b < end) {
            unsigned char u = (unsigned char)*pi->s;

            if (u <= 0x20 || '&' == u || '<' == u) {
                break;
            }
            *b++ = *pi->s++;
        }
        c = *pi->s++;
        switch (c) {
        case '<':
            done = 1;
            pi->s--;
            break;
        case '\0':
            pi->s--;
            set_error(&pi->err, "invalid format, document not terminated", pi->str, pi->s);
            goto CLEANUP;
        default:
            if (end <= (b + (('&' == c) ? 7 : 0))) { /* extra 8 for special just in case it is sequence of bytes */
                unsigned long size;

                if (0 == alloc_buf) {
                    size      = sizeof(buf) * 2;
                    alloc_buf = ALLOC_N(char, size);
                    memcpy(alloc_buf, buf, b - buf);
                    b = alloc_buf + (b - buf);
                } else {
                    unsigned long pos = b - alloc_buf;

                    size = (end - alloc_buf) * 2;
                    REALLOC_N(alloc_buf, char, size);
                    b = alloc_buf + pos;
                }
                end = alloc_buf + size - 2;
            }
            if ('&' == c) {
                if (0 == (b = read_coded_chars(pi, b))) {
                    goto CLEANUP;
                }
                /* An entity such as &#13; can decode to '\r'; be conservative. */
                maybe_cr = 1;
            } else {
                if (0 <= c && c <= 0x20) {
                    if (StrictEffort == pi->options->effort && 'x' == xml_valid_lower_chars[(unsigned char)c]) {
                        set_error(&pi->err, "invalid character", pi->str, pi->s);
                        goto CLEANUP;
                    }
                    switch (pi->options->skip) {
                    case CrSkip:
                        if (buf != b && '\n' == c && '\r' == *(b - 1)) {
                            *(b - 1) = '\n';
                        } else {
                            *b++ = c;
                        }
                        break;
                    case SpcSkip:
                        if (is_white(c)) {
                            if (buf == b || ' ' != *(b - 1)) {
                                *b++ = ' ';
                            }
                        } else {
                            *b++ = c;
                        }
                        break;
                    case NoSkip:
                    case OffSkip:
                    default: *b++ = c; break;
                    }
                } else {
                    *b++ = c;
                }
            }
            break;
        }
    }
    *b   = '\0';
    text = (0 != alloc_buf) ? alloc_buf : buf;
    len  = (size_t)(b - text);
    if (maybe_cr) {
        // fix_newlines can fold "\r\n" to "\n" and shorten the text, so the
        // length is only known after it runs. When it was skipped the text is
        // exactly b - text, so add_text avoids a strlen on the common path.
        fix_newlines(text);
        len = strlen(text);
    }
    pi->pcb->add_text(pi, text, len, ('/' == *(pi->s + 1)));
    // Single exit, so no error return can leak the heap-grown text buffer.
CLEANUP:
    if (0 != alloc_buf) {
        xfree(alloc_buf);
    }
}

#if 0
static void
read_reduced_text(PInfo pi) {
    char	buf[MAX_TEXT_LEN];
    char	*b = buf;
    char	*alloc_buf = 0;
    char	*end = b + sizeof(buf) - 2;
    char	c;
    int		spc = 0;
    int		done = 0;

    while (!done) {
	c = *pi->s++;
	switch(c) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	    spc = 1;
	    break;
	case '<':
	    done = 1;
	    pi->s--;
	    break;
	case '\0':
	    set_error(&pi->err, "invalid format, document not terminated", pi->str, pi->s);
	    return;
	default:
	    if (end <= (b + spc + (('&' == c) ? 7 : 0))) { /* extra 8 for special just in case it is sequence of bytes */
		unsigned long	size;

		if (0 == alloc_buf) {
		    size = sizeof(buf) * 2;
		    alloc_buf = ALLOC_N(char, size);
		    memcpy(alloc_buf, buf, b - buf);
		    b = alloc_buf + (b - buf);
		} else {
		    unsigned long	pos = b - alloc_buf;

		    size = (end - alloc_buf) * 2;
		    REALLOC(alloc_buf, char, size);
		    b = alloc_buf + pos;
		}
		end = alloc_buf + size - 2;
	    }
	    if (spc) {
		*b++ = ' ';
	    }
	    spc = 0;
	    if ('&' == c) {
		if (0 == (b = read_coded_chars(pi, b))) {
		    return;
		}
	    } else {
		*b++ = c;
	    }
	    break;
	}
    }
    *b = '\0';
    if (0 != alloc_buf) {
	fix_newlines(alloc_buf);
	pi->pcb->add_text(pi, alloc_buf, ('/' == *(pi->s + 1)));
	xfree(alloc_buf);
    } else {
	fix_newlines(buf);
	pi->pcb->add_text(pi, buf, ('/' == *(pi->s + 1)));
    }
}
#endif

static char *read_name_token(PInfo pi) {
    char *start;

    next_non_white(pi);
    start = pi->s;
    for (; 1; pi->s++) {
        switch (*pi->s) {
        case ' ':
        case '\t':
        case '\f':
        case '?':
        case '=':
        case '/':
        case '>':
        case '\n':
        case '\r': return start;
        case '\0':
            /* documents never terminate after a name token */
            set_error(&pi->err, "invalid format, document not terminated", pi->str, pi->s);
            return 0;
            break; /* to avoid warnings */
        case ':':
            if ('\0' == *pi->options->strip_ns) {
                break;
            } else if ('*' == *pi->options->strip_ns && '\0' == pi->options->strip_ns[1]) {
                start = pi->s + 1;
            } else if (0 == strncmp(pi->options->strip_ns, start, pi->s - start)) {
                start = pi->s + 1;
            }
            break;
        default: break;
        }
    }
    return start;
}

static void read_cdata(PInfo pi) {
    char *start;
    char *end;

    start = pi->s;
    end   = strstr(pi->s, "]]>");
    if (end == 0) {
        set_error(&pi->err, "invalid format, CDATA not terminated", pi->str, pi->s);
        return;
    }
    *end  = '\0';
    pi->s = end + 3;
    if (0 != pi->pcb->add_cdata) {
        fix_newlines(start);
        pi->pcb->add_cdata(pi, start, end - start);
    }
}

/* Assume the value starts immediately and goes until the quote character is
 * reached again. Do not read the character after the terminating quote.
 */
static char *read_quoted_value(PInfo pi) {
    char *value = 0;

    if ('"' == *pi->s || '\'' == *pi->s) {
        char term = *pi->s;

        pi->s++; /* skip quote character */
        value = pi->s;
        for (; *pi->s != term; pi->s++) {
            if ('\0' == *pi->s) {
                set_error(&pi->err, "invalid format, document not terminated", pi->str, pi->s);
                return 0;
            }
        }
        *pi->s = '\0'; /* terminate value */
        pi->s++;       /* move past quote */
    } else if (StrictEffort == pi->options->effort) {
        set_error(&pi->err, "invalid format, expected a quote character", pi->str, pi->s);
        return 0;
    } else if (TolerantEffort == pi->options->effort) {
        value = pi->s;
        for (; 1; pi->s++) {
            switch (*pi->s) {
            case '\0': set_error(&pi->err, "invalid format, document not terminated", pi->str, pi->s); return 0;
            case ' ':
            case '/':
            case '>':
            case '?':  // for instructions
            case '\t':
            case '\n':
            case '\r':
                pi->last = *pi->s;
                *pi->s   = '\0'; /* terminate value */
                pi->s++;
                return value;
            default: break;
            }
        }
    } else {
        value = pi->s;
        next_white(pi);
        if ('\0' == *pi->s) {
            set_error(&pi->err, "invalid format, document not terminated", pi->str, pi->s);
            return 0;
        }
        *pi->s++ = '\0'; /* terminate value */
    }
    return value;
}

static char *read_hex_uint64(char *b, uint64_t *up) {
    uint64_t u = 0;
    char     c;

    for (; ';' != *b; b++) {
        c = *b;
        if ('0' <= c && c <= '9') {
            u = (u << 4) | (uint64_t)(c - '0');
        } else if ('a' <= c && c <= 'f') {
            u = (u << 4) | (uint64_t)(c - 'a' + 10);
        } else if ('A' <= c && c <= 'F') {
            u = (u << 4) | (uint64_t)(c - 'A' + 10);
        } else {
            return 0;
        }
    }
    *up = u;

    return b;
}

static char *read_10_uint64(char *b, uint64_t *up) {
    uint64_t u = 0;
    char     c;

    for (; ';' != *b; b++) {
        c = *b;
        if ('0' <= c && c <= '9') {
            u = (u * 10) + (uint64_t)(c - '0');
        } else {
            return 0;
        }
    }
    *up = u;

    return b;
}

static char *read_coded_chars(PInfo pi, char *text) {
    char *b, buf[32];
    char *end = buf + sizeof(buf) - 1;
    char *s;
    long  blen = 0;

    for (b = buf, s = pi->s; b < end; b++, s++) {
        *b = *s;
        if ('\0' == *s) {
            set_error(&pi->err, "Not terminated coded char.", pi->str, pi->s);
            return NULL;
        }
        if (';' == *s) {
            *(b + 1) = '\0';
            blen     = b - buf;
            s++;
            break;
        }
    }
    if (b >= end) {
        // No terminating ; found in the first 31 bytes after an &.
        *text++ = '&';
    } else if ('#' == *buf) {
        uint64_t u = 0;

        b = buf + 1;
        if ('x' == *b || 'X' == *b) {
            b = read_hex_uint64(b + 1, &u);
        } else {
            b = read_10_uint64(b, &u);
        }
        if (0 == b) {
            *text++ = '&';
        } else {
            if (u <= 0x000000000000007FULL) {
                *text++ = (char)u;
            } else if (ox_utf8_encoding == pi->options->rb_enc) {
                text = ox_ucs_to_utf8_chars(text, u);
            } else if (0 == pi->options->rb_enc) {
                pi->options->rb_enc = ox_utf8_encoding;
                text                = ox_ucs_to_utf8_chars(text, u);
            } else if (TolerantEffort == pi->options->effort) {
                *text++ = '&';
                return text;
            } else if (u <= 0x00000000000000FFULL) {
                *text++ = (char)u;
            } else {
                /*set_error(&pi->err, "Invalid encoding, need UTF-8 or UTF-16 encoding to parse &#nnnn; character
                 * sequences.", pi->str, pi->s); */
                set_error(&pi->err,
                          "Invalid encoding, need UTF-8 encoding to parse &#nnnn; character sequences.",
                          pi->str,
                          pi->s);
                return NULL;
            }
            pi->s = s;
        }
    } else {
        char *t2;

        buf[blen] = '\0';
        if (NULL == (t2 = ox_entity_lookup(text, buf))) {
            *text++ = '&';
        } else {
            text  = t2;
            pi->s = s;
        }
    }
    return text;
}

static int collapse_special(PInfo pi, char *str) {
    char *s = str;
    char *b = str;

    while ('\0' != *s) {
        if ('&' == *s) {
            int   c;
            char *end;

            s++;
            if ('#' == *s) {
                uint64_t u = 0;
                char     x;

                s++;
                if ('x' == *s || 'X' == *s) {
                    x = *s;
                    s++;
                    end = read_hex_uint64(s, &u);
                } else {
                    x   = '\0';
                    end = read_10_uint64(s, &u);
                }
                if (0 == end) {
                    if (TolerantEffort == pi->options->effort) {
                        *b++ = '&';
                        *b++ = '#';
                        if ('\0' != x) {
                            *b++ = x;
                        }
                        continue;
                    }
                    return EDOM;
                }
                if (u <= 0x000000000000007FULL) {
                    *b++ = (char)u;
                } else if (ox_utf8_encoding == pi->options->rb_enc) {
                    b = ox_ucs_to_utf8_chars(b, u);
                    /* TBD support UTF-16 */
                } else if (0 == pi->options->rb_enc) {
                    pi->options->rb_enc = ox_utf8_encoding;
                    b                   = ox_ucs_to_utf8_chars(b, u);
                } else {
                    /* set_error(&pi->err, "Invalid encoding, need UTF-8 or UTF-16 encoding to parse &#nnnn; character
                     * sequences.", pi->str, pi->s);*/
                    set_error(&pi->err,
                              "Invalid encoding, need UTF-8 encoding to parse &#nnnn; character sequences.",
                              pi->str,
                              pi->s);
                    return 0;
                }
                s = end + 1;
            } else {
                if (0 == strncasecmp(s, "lt;", 3)) {
                    c = '<';
                    s += 3;
                } else if (0 == strncasecmp(s, "gt;", 3)) {
                    c = '>';
                    s += 3;
                } else if (0 == strncasecmp(s, "amp;", 4)) {
                    c = '&';
                    s += 4;
                } else if (0 == strncasecmp(s, "quot;", 5)) {
                    c = '"';
                    s += 5;
                } else if (0 == strncasecmp(s, "apos;", 5)) {
                    c = '\'';
                    s += 5;
                } else if (TolerantEffort == pi->options->effort) {
                    *b++ = '&';
                    continue;
                } else {
                    char  key[16];
                    char *k    = key;
                    char *kend = key + sizeof(key) - 1;

                    *k++ = *s;
                    while (';' != *s++) {
                        if ('\0' == *s) {
                            set_error(&pi->err,
                                      "Invalid format, special character does not end with a semicolon",
                                      pi->str,
                                      pi->s);
                            return EDOM;
                        }
                        if (kend <= k) {
                            k = key + 1;  // because k-- follows
                            break;
                        }
                        *k++ = *s;
                    }
                    k--;
                    *k = '\0';
                    if ('\0' == *key || NULL == (b = ox_entity_lookup(b, key))) {
                        set_error(&pi->err, "Invalid format, invalid special character sequence", pi->str, pi->s);
                        c = '?';
                        return 0;
                    }
                    continue;
                }
                *b++ = (char)c;
            }
        } else {
            *b++ = *s++;
        }
    }
    *b = '\0';

    return 0;
}
