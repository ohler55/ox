/* sax.c
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
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "ruby.h"
#include "ox.h"

typedef struct _SaxDrive {
    char        base_buf[0x00010000];
    //char        base_buf[0x00000010];
    char        *buf;
    char        *buf_end;
    char        *cur;
    char        *read_end;      // one past last character read
    char        *str;           // start of current string being read
    int         line;
    int         col;
    VALUE       handler;
    int         (*read_func)(struct _SaxDrive *dr);
    int         convert_special;
    union {
        int     fd;
        VALUE   io;
    };
    int         has_instruct;
    int         has_attr;
    int         has_doctype;
    int         has_comment;
    int         has_cdata;
    int         has_text;
    int         has_start_element;
    int         has_end_element;
    int         has_error;
#ifdef HAVE_RUBY_ENCODING_H
    rb_encoding *encoding;
#endif
} *SaxDrive;

static void     sax_drive_init(SaxDrive dr, VALUE handler, VALUE io, int convert);
static void     sax_drive_cleanup(SaxDrive dr);
static int      sax_drive_read(SaxDrive dr);
static void     sax_drive_error(SaxDrive dr, const char *msg, int critical);

static int      read_children(SaxDrive dr, int first);
static int      read_instruction(SaxDrive dr);
static int      read_doctype(SaxDrive dr);
static int      read_cdata(SaxDrive dr);
static int      read_comment(SaxDrive dr);
static int      read_element(SaxDrive dr);
static int      read_text(SaxDrive dr);
static int      read_attrs(SaxDrive dr, char c, char termc, char term2, int is_xml);
static char     read_name_token(SaxDrive dr);
static int      read_quoted_value(SaxDrive dr);
static int      collapse_special(char *str);

static VALUE    io_cb(VALUE rdr);
static VALUE    partial_io_cb(VALUE rdr);
static int      read_from_io(SaxDrive dr);
static int      read_from_fd(SaxDrive dr);
static int      read_from_io_partial(SaxDrive dr);

static inline char
sax_drive_get(SaxDrive dr) {
    if (dr->read_end <= dr->cur) {
        if (0 != sax_drive_read(dr)) {
            return 0;
        }
    }
    if ('\n' == *dr->cur) {
        dr->line++;
        dr->col = 0;
    }
    dr->col++;
    
    return *dr->cur++;
}

/* Starts by reading a character so it is safe to use with an empty or
 * compacted buffer.
 */
inline static char
next_non_white(SaxDrive dr) {
    char        c;

    while ('\0' != (c = sax_drive_get(dr))) {
	switch(c) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	    break;
	default:
	    return c;
	}
    }
    return '\0';
}

inline static int
is_white(char c) {
    switch(c) {
    case ' ':
    case '\t':
    case '\f':
    case '\n':
    case '\r':
        return 1;
    default:
        break;
    }
    return 0;
}

inline static VALUE
str2sym(const char *str) {
    VALUE       *slot;
    VALUE       sym;

    if (Qundef == (sym = ox_cache_get(symbol_cache, str, &slot))) {
        sym = ID2SYM(rb_intern(str));
        *slot = sym;
    }
    return sym;
}


void
ox_sax_parse(VALUE handler, VALUE io, int convert) {
    struct _SaxDrive    dr;
    
    sax_drive_init(&dr, handler, io, convert);
#if 0
    printf("*** sax_parse with these flags\n");
    printf("    has_instruct = %s\n", dr.has_instruct ? "true" : "false");
    printf("    has_attr = %s\n", dr.has_attr ? "true" : "false");
    printf("    has_doctype = %s\n", dr.has_doctype ? "true" : "false");
    printf("    has_comment = %s\n", dr.has_comment ? "true" : "false");
    printf("    has_cdata = %s\n", dr.has_cdata ? "true" : "false");
    printf("    has_text = %s\n", dr.has_text ? "true" : "false");
    printf("    has_start_element = %s\n", dr.has_start_element ? "true" : "false");
    printf("    has_end_element = %s\n", dr.has_end_element ? "true" : "false");
    printf("    has_error = %s\n", dr.has_error ? "true" : "false");
#endif
    read_children(&dr, 1);
    sax_drive_cleanup(&dr);
}

static void
sax_drive_init(SaxDrive dr, VALUE handler, VALUE io, int convert) {
    if (rb_respond_to(io, readpartial_id)) {
        VALUE   rfd;

        if (rb_respond_to(io, rb_intern("fileno")) && Qnil != (rfd = rb_funcall(io, rb_intern("fileno"), 0))) {
            dr->read_func = read_from_fd;
            dr->fd = FIX2INT(rfd);
        } else {
            dr->read_func = read_from_io_partial;
            dr->io = io;
        }
    } else if (rb_respond_to(io, read_id)) {
        VALUE   rfd;

        if (rb_respond_to(io, rb_intern("fileno")) && Qnil != (rfd = rb_funcall(io, rb_intern("fileno"), 0))) {
            dr->read_func = read_from_fd;
            dr->fd = FIX2INT(rfd);
        } else {
            dr->read_func = read_from_io;
            dr->io = io;
        }
    } else {
        rb_raise(rb_eArgError, "sax_parser io argument must respond to readpartial() or read().\n");
    }
    dr->buf = dr->base_buf;
    *dr->buf = '\0';
    dr->buf_end = dr->buf + sizeof(dr->base_buf) - 1; // 1 less to make debugging easier
    dr->cur = dr->buf;
    dr->read_end = dr->buf;
    dr->str = 0;
    dr->line = 1;
    dr->col = 0;
    dr->handler = handler;
    dr->convert_special = convert;
    dr->has_instruct = rb_respond_to(handler, instruct_id);
    dr->has_attr = rb_respond_to(handler, attr_id);
    dr->has_doctype = rb_respond_to(handler, doctype_id);
    dr->has_comment = rb_respond_to(handler, comment_id);
    dr->has_cdata = rb_respond_to(handler, cdata_id);
    dr->has_text = rb_respond_to(handler, text_id);
    dr->has_start_element = rb_respond_to(handler, start_element_id);
    dr->has_end_element = rb_respond_to(handler, end_element_id);
    dr->has_error = rb_respond_to(handler, error_id);
#ifdef HAVE_RUBY_ENCODING_H
    if ('\0' == *default_options.encoding) {
        dr->encoding = 0;
    } else {
        dr->encoding = rb_enc_find(default_options.encoding);
    }
#endif
}

static void
sax_drive_cleanup(SaxDrive dr) {
    if (dr->base_buf != dr->buf) {
        free(dr->buf);
    }
}

static int
sax_drive_read(SaxDrive dr) {
    int         err;
    size_t      shift = 0;
    
    if (dr->buf < dr->cur) {
        if (0 == dr->str) {
            shift = dr->cur - dr->buf;
        } else {
            shift = dr->str - dr->buf;
        }
        //printf("\n*** shift: %lu\n", shift);
        if (0 == shift) { // no space left so allocate more
            char        *old = dr->buf;
            size_t      size = dr->buf_end - dr->buf;
        
            if (dr->buf == dr->base_buf) {
                if (0 == (dr->buf = (char*)malloc(size * 2))) {
                    rb_raise(rb_eNoMemError, "Could not allocate memory for large element.\n");
                }
                memcpy(dr->buf, old, size);
            } else {
                if (0 == (dr->buf = (char*)realloc(dr->buf, size * 2))) {
                    rb_raise(rb_eNoMemError, "Could not allocate memory for large element.\n");
                }
            }
            dr->buf_end = dr->buf + size * 2;
            dr->cur = dr->buf + (dr->cur - old);
            dr->read_end = dr->buf + (dr->read_end - old);
            if (0 != dr->str) {
                dr->str = dr->buf + (dr->str - old);
            }
        } else {
            memmove(dr->buf, dr->buf + shift, dr->read_end - (dr->buf + shift));
            dr->cur -= shift;
            dr->read_end -= shift;
            if (0 != dr->str) {
                dr->str -= shift;
            }
        }
    }
    err = dr->read_func(dr);
    *dr->read_end = '\0';

    return err;
}

static void
sax_drive_error(SaxDrive dr, const char *msg, int critical) {
    if (dr->has_error) {
        VALUE   args[3];

        args[0] = rb_str_new2(msg);
        args[1] = INT2FIX(dr->line);
        args[2] = INT2FIX(dr->col);
        rb_funcall2(dr->handler, error_id, 3, args);
    } else if (critical) {
        sax_drive_cleanup(dr);
        rb_raise(rb_eSyntaxError, "%s at line %d, column %d\n", msg, dr->line, dr->col);
    }
}

static int
read_children(SaxDrive dr, int first) {
    int         err = 0;
    int         element_read = !first;
    int         doctype_read = !first;
    char        c;
    
    while (!err) {
        dr->str = dr->cur; // protect the start
        if ('\0' == (c = next_non_white(dr))) {
            if (!first) {
                sax_drive_error(dr, "invalid format, element not terminated", 1);
                err = 1;
            }
            break; // normal completion if first
        }
	if ('<' != c) {
            if (first) { // all top level entities start with <
                sax_drive_error(dr, "invalid format, expected <", 1);
                break; // unrecoverable
            }
            if (0 != (err = read_text(dr))) { // finished when < is reached
                break;
            }
	}
        dr->str = dr->cur; // protect the start for elements
        c = sax_drive_get(dr);
	switch (c) {
	case '?': // instructions (xml or otherwise)
            if (!first || element_read || doctype_read) {
                sax_drive_error(dr, "invalid format, instruction must come before elements", 0);
            }
	    err = read_instruction(dr);
	    break;
	case '!': // comment or doctype
            dr->str = dr->cur;
            c = sax_drive_get(dr);
	    if ('\0' == c) {
                sax_drive_error(dr, "invalid format, DOCTYPE or comment not terminated", 1);
                err = 1;
	    } else if ('-' == c) {
                c = sax_drive_get(dr); // skip first - and get next character
		if ('-' != c) {
                    sax_drive_error(dr, "invalid format, bad comment format", 1);
                    err = 1;
		} else {
                    c = sax_drive_get(dr); // skip second -
		    err = read_comment(dr);
		}
            } else {
                int     i;

                for (i = 7; 0 < i; i--) {
                    sax_drive_get(dr);
                }
                if (0 == strncmp("DOCTYPE", dr->str, 7)) {
                    if (element_read || !first) {
                        sax_drive_error(dr, "invalid format, DOCTYPE can not come after an element", 0);
                    }
                    doctype_read = 1;
                    err = read_doctype(dr);
                } else if (0 == strncmp("[CDATA[", dr->str, 7)) {
                    err = read_cdata(dr);
                } else {
                    sax_drive_error(dr, "invalid format, DOCTYPE or comment expected", 1);
                    err = 1;
                }
	    }
	    break;
	case '/': // element end
            return ('\0' == read_name_token(dr));
            break;
	case '\0':
            sax_drive_error(dr, "invalid format, document not terminated", 1);
            err = 1;
            break;
	default:
            dr->cur--; // safe since no read occurred after getting last character
            if (first && element_read) {
                sax_drive_error(dr, "invalid format, multiple top level elements", 0);
            }
	    err = read_element(dr);
            element_read = 1;
	    break;
	}
    }
    return err;
}

/* Entered after the "<?" sequence. Ready to read the rest.
 */
static int
read_instruction(SaxDrive dr) {
    char        c;

    if ('\0' == (c = read_name_token(dr))) {
        return -1;
    }
    if (dr->has_instruct) {
        VALUE       args[1];

        args[0] = rb_str_new2(dr->str);
        rb_funcall2(dr->handler, instruct_id, 1, args);
    }
    if (0 != read_attrs(dr, c, '?', '?', (0 == strcmp("xml", dr->str)))) {
        return -1;
    }
    c = next_non_white(dr);
    if ('>' != c) {
        sax_drive_error(dr, "invalid format, instruction not terminated", 1);
        return -1;
    }
    dr->str = 0;

    return 0;
}

/* Entered after the "<!DOCTYPE" sequence. Ready to read the rest.
 */
static int
read_doctype(SaxDrive dr) {
    char        c;

    dr->str = dr->cur - 1; // mark the start
    while ('>' != (c = sax_drive_get(dr))) {
        if ('\0' == c) {
            sax_drive_error(dr, "invalid format, doctype terminated unexpectedly", 1);
            return -1;
        }
    }
    *(dr->cur - 1) = '\0';
    if (dr->has_doctype) {
        VALUE       args[1];

        args[0] = rb_str_new2(dr->str);
        rb_funcall2(dr->handler, doctype_id, 1, args);
    }
    dr->str = 0;

    return 0;
}

/* Entered after the "<![CDATA[" sequence. Ready to read the rest.
 */
static int
read_cdata(SaxDrive dr) {
    char        c;
    int         end = 0;

    dr->str = dr->cur - 1; // mark the start
    while (1) {
        c = sax_drive_get(dr);
        if (']' == c) {
            end++;
        } else if ('>' == c) {
            if (2 <= end) {
                *(dr->cur - 3) = '\0';
                break;
            }
            end = 0;
        } else if ('\0' == c) {
            sax_drive_error(dr, "invalid format, cdata terminated unexpectedly", 1);
            return -1;
        } else {
            end = 0;
        }
    }
    if (dr->has_cdata) {
        VALUE       args[1];

        args[0] = rb_str_new2(dr->str);
#ifdef HAVE_RUBY_ENCODING_H
        if (0 != dr->encoding) {
            rb_enc_associate(args[0], dr->encoding);
        }
#endif
        rb_funcall2(dr->handler, cdata_id, 1, args);
    }
    dr->str = 0;

    return 0;
}

/* Entered after the "<!--" sequence. Ready to read the rest.
 */
static int
read_comment(SaxDrive dr) {
    char        c;
    int         end = 0;

    dr->str = dr->cur - 1; // mark the start
    while (1) {
        c = sax_drive_get(dr);
        if ('-' == c) {
            if (end) {
                *(dr->cur - 2) = '\0';
                break;
            } else {
                end = 1;
            }
        } else if ('\0' == c) {
            sax_drive_error(dr, "invalid format, comment terminated unexpectedly", 1);
            return -1;
        } else {
            end = 0;
        }
    }
    c = sax_drive_get(dr);
    if ('>' != c) {
        sax_drive_error(dr, "invalid format, comment terminated unexpectedly", 1);
    }
    if (dr->has_comment) {
        VALUE       args[1];

        args[0] = rb_str_new2(dr->str);
#ifdef HAVE_RUBY_ENCODING_H
        if (0 != dr->encoding) {
            rb_enc_associate(args[0], dr->encoding);
        }
#endif
        rb_funcall2(dr->handler, comment_id, 1, args);
    }
    dr->str = 0;

    return 0;
}

/* Entered after the '<' and the first character after that. Returns status
 * code.
 */
static int
read_element(SaxDrive dr) {
    VALUE       name = Qnil;
    char        c;
    int         closed;

    if ('\0' == (c = read_name_token(dr))) {
        return -1;
    }
    name = str2sym(dr->str);
    if (dr->has_start_element) {
        VALUE       args[1];

        args[0] = name;
        rb_funcall2(dr->handler, start_element_id, 1, args);
    }
    if ('/' == c) {
        closed = 1;
    } else if ('>' == c) {
        closed = 0;
    } else {
        if (0 != read_attrs(dr, c, '/', '>', 0)) {
            return -1;
        }
        closed = ('/' == *(dr->cur - 1));
    }
    if (closed) {
        c = next_non_white(dr);
        if ('>' != c) {
            sax_drive_error(dr, "invalid format, element not closed", 1);
            return -1;
        }
    }
    if (closed) {
        if (dr->has_end_element) {
            VALUE       args[1];

            args[0] = name;
            rb_funcall2(dr->handler, end_element_id, 1, args);
        }
    } else {
        if (0 != read_children(dr, 0)) {
            return -1;
        }
        if (0 != strcmp(dr->str, rb_id2name(SYM2ID(name)))) {
            sax_drive_error(dr, "invalid format, element start and end names do not match", 1);
            return -1;
        }
        if (0 != dr->has_end_element) {
            VALUE       args[1];

            args[0] = name;
            rb_funcall2(dr->handler, end_element_id, 1, args);
        }
    }
    dr->str = 0;

    return 0;
}

static int
read_text(SaxDrive dr) {
    char        c;

    dr->str = dr->cur - 1; // mark the start
    while ('<' != (c = sax_drive_get(dr))) {
        if ('\0' == c) {
            sax_drive_error(dr, "invalid format, text terminated unexpectedly", 1);
            return -1;
        }
    }
    *(dr->cur - 1) = '\0';
    if (dr->has_text) {
        VALUE   args[1];
        
        if (dr->convert_special) {
            if (0 != collapse_special(dr->str) && 0 != strchr(dr->str, '&')) {
                sax_drive_error(dr, "invalid format, special character does not end with a semicolon", 0);
            }
        }
        args[0] = rb_str_new2(dr->str);
#ifdef HAVE_RUBY_ENCODING_H
        if (0 != dr->encoding) {
            rb_enc_associate(args[0], dr->encoding);
        }
#endif
        rb_funcall2(dr->handler, text_id, 1, args);
    }
    return 0;
}

static int
read_attrs(SaxDrive dr, char c, char termc, char term2, int is_xml) {
    VALUE       name = Qnil;
    int         is_encoding = 0;
    
    dr->str = dr->cur; // lock it down
    if (is_white(c)) {
        c = next_non_white(dr);
    }
    while (termc != c && term2 != c) {
        dr->cur--;
        if ('\0' == c) {
            sax_drive_error(dr, "invalid format, processing instruction not terminated", 1);
            return -1;
        }
        if ('\0' == (c = read_name_token(dr))) {
            return -1;
        }
        if (is_xml && 0 == strcmp("encoding", dr->str)) {
            is_encoding = 1;
        }
        if (dr->has_attr) {
            name = str2sym(dr->str);
        }
        if (is_white(c)) {
            c = next_non_white(dr);
        }
        if ('=' != c) {
            sax_drive_error(dr, "invalid format, no attribute value", 1);
            return -1;
        }
        if (0 != read_quoted_value(dr)) {
            return -1;
        }
#ifdef HAVE_RUBY_ENCODING_H
        if (is_encoding) {
            dr->encoding = rb_enc_find(dr->str);
        }
#endif
        if (dr->has_attr) {
            VALUE       args[2];

            args[0] = name;
            if (0 != collapse_special(dr->str) && 0 != strchr(dr->str, '&')) {
                sax_drive_error(dr, "invalid format, special character does not end with a semicolon", 0);
            }
            args[1] = rb_str_new2(dr->str);
#ifdef HAVE_RUBY_ENCODING_H
            if (0 != dr->encoding) {
                rb_enc_associate(args[1], dr->encoding);
            }
#endif
            rb_funcall2(dr->handler, attr_id, 2, args);
        }
        c = next_non_white(dr);
    }
    return 0;
}

static char
read_name_token(SaxDrive dr) {
    char        c;

    dr->str = dr->cur; // make sure the start doesn't get compacted out
    c = sax_drive_get(dr);
    if (is_white(c)) {
        c = next_non_white(dr);
        dr->str = dr->cur - 1;
    }
    while (1) {
	switch (c) {
	case ' ':
	case '\t':
	case '\f':
	case '?':
	case '=':
	case '/':
	case '>':
	case '\n':
	case '\r':
            *(dr->cur - 1) = '\0';
	    return c;
	case '\0':
            // documents never terminate after a name token
            sax_drive_error(dr, "invalid format, document not terminated", 1);
            return '\0';
	default:
	    break;
	}
        c = sax_drive_get(dr);
    }
    return '\0';
}

static int
read_quoted_value(SaxDrive dr) {
    char        c;

    dr->str = dr->cur;
    c = sax_drive_get(dr);
    if (is_white(c)) {
        c = next_non_white(dr);
    }
    if ('"' != c) {
        sax_drive_error(dr, "invalid format, attibute value not in quotes", 1);
        return -1;
    }
    dr->str = dr->cur;
    while ('"' != (c = sax_drive_get(dr))) {
        if ('\0' == c) {
            sax_drive_error(dr, "invalid format, quoted value not terminated", 1);
            return -1;
        }
    }
    *(dr->cur - 1) = '\0'; // terminate value

    return 0;
}

static int
read_from_io_partial(SaxDrive dr) {
    int ex = 0;

    rb_protect(partial_io_cb, (VALUE)dr, &ex);
    // printf("*** io_cb exception = %d\n", ex);
    // An error code of 6 is always returned not matter what kind of Exception is raised.
    return ex;
}

static VALUE
partial_io_cb(VALUE rdr) {
    SaxDrive    dr = (SaxDrive)rdr;
    VALUE       args[1];
    VALUE       rstr;
    char        *str;
    size_t      cnt;

    args[0] = ULONG2NUM(dr->buf_end - dr->cur);
    rstr = rb_funcall2(dr->io, readpartial_id, 1, args);
    str = StringValuePtr(rstr);
    cnt = strlen(str);
    //printf("*** read %lu bytes, str: '%s'\n", cnt, str);
    strcpy(dr->cur, str);
    dr->read_end = dr->cur + cnt;

    return Qnil;
}

static int
read_from_io(SaxDrive dr) {
    int ex = 0;

    rb_protect(io_cb, (VALUE)dr, &ex);
    // printf("*** io_cb exception = %d\n", ex);
    // An error code of 6 is always returned not matter what kind of Exception is raised.
    return ex;
}

static VALUE
io_cb(VALUE rdr) {
    SaxDrive    dr = (SaxDrive)rdr;
    VALUE       args[1];
    VALUE       rstr;
    char        *str;
    size_t      cnt;

    args[0] = ULONG2NUM(dr->buf_end - dr->cur);
    //args[0] = SIZET2NUM(dr->buf_end - dr->cur);
    rstr = rb_funcall2(dr->io, read_id, 1, args);
    str = StringValuePtr(rstr);
    cnt = strlen(str);
    //printf("*** read %lu bytes, str: '%s'\n", cnt, str);
    strcpy(dr->cur, str);
    dr->read_end = dr->cur + cnt;

    return Qnil;
}

static int
read_from_fd(SaxDrive dr) {
    ssize_t     cnt;
    size_t      max = dr->buf_end - dr->cur;

    cnt = read(dr->fd, dr->cur, max);
    if (cnt < 0) {
        sax_drive_error(dr, "failed to read from file", 1);
        return -1;
    } else if (0 != cnt) {
        dr->read_end = dr->cur + cnt;
    }
    return 0;
}


static int
collapse_special(char *str) {
    char        *s = str;
    char        *b = str;

    while ('\0' != *s) {
        if ('&' == *s) {
            int         c;
            char        *end;
            
            s++;
            if ('#' == *s) {
                s++;
                c = (int)strtol(s, &end, 10);
                if (';' != *end) {
                    return EDOM;
                }
                s = end + 1;
            } else if (0 == strncasecmp(s, "lt;", 3)) {
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
            } else {
                c = '?';
                while (';' != *s++) {
                    if ('\0' == *s) {
                        return EDOM;
                    }
                }
                s++;
            }
            *b++ = (char)c;
        } else {
            *b++ = *s++;
        }
    }
    *b = '\0';

    return 0;
}
