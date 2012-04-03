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
#include <strings.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>

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
    VALUE	value_obj;
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
    int         has_value;
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

static VALUE	rescue_cb(VALUE rdr, VALUE err);
static VALUE    io_cb(VALUE rdr);
static VALUE    partial_io_cb(VALUE rdr);
static int      read_from_io(SaxDrive dr);
static int      read_from_fd(SaxDrive dr);
static int      read_from_io_partial(SaxDrive dr);

static VALUE	sax_value_class;


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

/* Starts by reading a character so it is safe to use with an empty or
 * compacted buffer.
 */
inline static char
next_white(SaxDrive dr) {
    char        c;

    while ('\0' != (c = sax_drive_get(dr))) {
	switch(c) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	case '\0':
	    return c;
	default:
	    break;
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
str2sym(const char *str, SaxDrive dr) {
    VALUE       *slot;
    VALUE       sym;

    if (Qundef == (sym = ox_cache_get(ox_symbol_cache, str, &slot))) {
#ifdef HAVE_RUBY_ENCODING_H
        if (0 != dr->encoding) {
	    VALUE	rstr = rb_str_new2(str);

            rb_enc_associate(rstr, dr->encoding);
	    sym = rb_funcall(rstr, ox_to_sym_id, 0);
        } else {
	    sym = ID2SYM(rb_intern(str));
	}
#else
        sym = ID2SYM(rb_intern(str));
#endif
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
    printf("    has_value = %s\n", dr.has_value ? "true" : "false");
    printf("    has_start_element = %s\n", dr.has_start_element ? "true" : "false");
    printf("    has_end_element = %s\n", dr.has_end_element ? "true" : "false");
    printf("    has_error = %s\n", dr.has_error ? "true" : "false");
#endif
    read_children(&dr, 1);
    sax_drive_cleanup(&dr);
}

static void
sax_drive_init(SaxDrive dr, VALUE handler, VALUE io, int convert) {
    if (rb_respond_to(io, ox_readpartial_id)) {
        VALUE   rfd;

        if (rb_respond_to(io, ox_fileno_id) && Qnil != (rfd = rb_funcall(io, ox_fileno_id, 0))) {
            dr->read_func = read_from_fd;
            dr->fd = FIX2INT(rfd);
        } else {
            dr->read_func = read_from_io_partial;
            dr->io = io;
        }
    } else if (rb_respond_to(io, ox_read_id)) {
        VALUE   rfd;

        if (rb_respond_to(io, ox_fileno_id) && Qnil != (rfd = rb_funcall(io, ox_fileno_id, 0))) {
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
    dr->value_obj = rb_data_object_alloc(sax_value_class, dr, 0, 0);
    rb_gc_register_address(&dr->value_obj);
    dr->convert_special = convert;
    dr->has_instruct = rb_respond_to(handler, ox_instruct_id);
    dr->has_attr = rb_respond_to(handler, ox_attr_id);
    dr->has_doctype = rb_respond_to(handler, ox_doctype_id);
    dr->has_comment = rb_respond_to(handler, ox_comment_id);
    dr->has_cdata = rb_respond_to(handler, ox_cdata_id);
    dr->has_text = rb_respond_to(handler, ox_text_id);
    dr->has_value = rb_respond_to(handler, ox_value_id);
    dr->has_start_element = rb_respond_to(handler, ox_start_element_id);
    dr->has_end_element = rb_respond_to(handler, ox_end_element_id);
    dr->has_error = rb_respond_to(handler, ox_error_id);
#ifdef HAVE_RUBY_ENCODING_H
    if ('\0' == *ox_default_options.encoding) {
        dr->encoding = 0;
    } else {
        dr->encoding = rb_enc_find(ox_default_options.encoding);
    }
#endif
}

static void
sax_drive_cleanup(SaxDrive dr) {
    rb_gc_unregister_address(&dr->value_obj);
    if (dr->base_buf != dr->buf) {
        xfree(dr->buf);
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
                dr->buf = ALLOC_N(char, size * 2);
                memcpy(dr->buf, old, size);
            } else {
		REALLOC_N(dr->buf, char, size * 2);
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
        rb_funcall2(dr->handler, ox_error_id, 3, args);
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
        rb_funcall2(dr->handler, ox_instruct_id, 1, args);
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
        rb_funcall2(dr->handler, ox_doctype_id, 1, args);
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

    dr->cur--; // back up to the start in case the cdata is empty
    dr->str = dr->cur; // mark the start
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
        rb_funcall2(dr->handler, ox_cdata_id, 1, args);
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
        rb_funcall2(dr->handler, ox_comment_id, 1, args);
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
    name = str2sym(dr->str, dr);
    if (dr->has_start_element) {
        VALUE       args[1];

        args[0] = name;
        rb_funcall2(dr->handler, ox_start_element_id, 1, args);
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
            rb_funcall2(dr->handler, ox_end_element_id, 1, args);
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
            rb_funcall2(dr->handler, ox_end_element_id, 1, args);
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
    if (dr->has_value) {
        VALUE   args[1];

	*args = dr->value_obj;
        rb_funcall2(dr->handler, ox_value_id, 1, args);
    } else if (dr->has_text) {
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
        rb_funcall2(dr->handler, ox_text_id, 1, args);
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
            name = str2sym(dr->str, dr);
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
            rb_funcall2(dr->handler, ox_attr_id, 2, args);
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
    if ('"' == c || '\'' == c) {
	char	term = c;

        dr->str = dr->cur;
        while (term != (c = sax_drive_get(dr))) {
            if ('\0' == c) {
                sax_drive_error(dr, "invalid format, quoted value not terminated", 1);
                return -1;
            }
        }
    } else {
        dr->str = dr->cur - 1;
        if ('\0' == (c = next_white(dr))) {
	    sax_drive_error(dr, "invalid format, attibute value not in quotes", 1);
	}
    }        
    *(dr->cur - 1) = '\0'; // terminate value
    return 0;
}

static VALUE
rescue_cb(VALUE rdr, VALUE err) {
    if (rb_obj_class(err) != rb_eEOFError) {
	SaxDrive	dr = (SaxDrive)rdr;

        sax_drive_cleanup(dr);
        rb_raise(err, "at line %d, column %d\n", dr->line, dr->col);
    }
    return Qfalse;
}

static VALUE
partial_io_cb(VALUE rdr) {
    SaxDrive    dr = (SaxDrive)rdr;
    VALUE       args[1];
    VALUE       rstr;
    char        *str;
    size_t      cnt;

    args[0] = ULONG2NUM(dr->buf_end - dr->cur);
    rstr = rb_funcall2(dr->io, ox_readpartial_id, 1, args);
    str = StringValuePtr(rstr);
    cnt = strlen(str);
    //printf("*** read %lu bytes, str: '%s'\n", cnt, str);
    strcpy(dr->cur, str);
    dr->read_end = dr->cur + cnt;

    return Qtrue;
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
    rstr = rb_funcall2(dr->io, ox_read_id, 1, args);
    str = StringValuePtr(rstr);
    cnt = strlen(str);
    //printf("*** read %lu bytes, str: '%s'\n", cnt, str);
    strcpy(dr->cur, str);
    dr->read_end = dr->cur + cnt;

    return Qtrue;
}

static int
read_from_io_partial(SaxDrive dr) {
    return (Qfalse == rb_rescue(partial_io_cb, (VALUE)dr, rescue_cb, (VALUE)dr));
}

static int
read_from_io(SaxDrive dr) {
    return (Qfalse == rb_rescue(io_cb, (VALUE)dr, rescue_cb, (VALUE)dr));
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
		if ('x' == *s || 'X' == *s) {
		    s++;
		    c = (int)strtol(s, &end, 16);
		} else {
		    c = (int)strtol(s, &end, 10);
		}
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

static VALUE
parse_double_time(const char *text) {
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
#if HAS_NANO_TIME
    return rb_time_nano_new(v, v2);
#else
    return rb_time_new(v, v2 / 1000);
#endif
}

typedef struct _Tp {
    int         cnt;
    char        end;
    char        alt;
} *Tp;

static VALUE
parse_xsd_time(const char *text) {
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
#if HAS_NANO_TIME
    return rb_time_nano_new(mktime(&tm), cargs[6]);
#else
    return rb_time_new(mktime(&tm), cargs[6] / 1000);
#endif
}

static VALUE
sax_value_as_s(VALUE self) {
    SaxDrive	dr = DATA_PTR(self);
    VALUE	rs = rb_str_new2(dr->str);

#ifdef HAVE_RUBY_ENCODING_H
    if (0 != dr->encoding) {
	rb_enc_associate(rs, dr->encoding);
    }
#endif
    return rs;
}

static VALUE
sax_value_as_sym(VALUE self) {
    SaxDrive	dr = DATA_PTR(self);

    return str2sym(dr->str, dr);
}

static VALUE
sax_value_as_f(VALUE self) {
    return rb_float_new(strtod(((SaxDrive)DATA_PTR(self))->str, 0));
}

static VALUE
sax_value_as_i(VALUE self) {
    SaxDrive	dr = DATA_PTR(self);
    const char	*s = dr->str;
    long	n = 0;
    int		neg = 0;

    if ('-' == *s) {
	neg = 1;
	s++;
    } else if ('+' == *s) {
	s++;
    }
    for (; '\0' != *s; s++) {
	if ('0' <= *s && *s <= '9') {
	    n = n * 10 + (*s - '0');
	} else {
	    rb_raise(rb_eArgError, "Not a valid Fixnum.\n");
	}
    }
    if (neg) {
	n = -n;
    }
    return LONG2NUM(n);
}

static VALUE
sax_value_as_time(VALUE self) {
    SaxDrive	dr = DATA_PTR(self);
    const char	*str = dr->str;
    VALUE       t;

    if (Qnil == (t = parse_double_time(str)) &&
	Qnil == (t = parse_xsd_time(str))) {
        VALUE       args[1];

        //printf("**** time parse\n");
        *args = rb_str_new2(str);
        t = rb_funcall2(ox_time_class, ox_parse_id, 1, args);
    }
    return t;
}

static VALUE
sax_value_as_bool(VALUE self) {
    return (0 == strcasecmp("true", ((SaxDrive)DATA_PTR(self))->str)) ? Qtrue : Qfalse;
}

static VALUE
sax_value_empty(VALUE self) {
    return ('\0' == *((SaxDrive)DATA_PTR(self))->str) ? Qtrue : Qfalse;
}

void
ox_sax_define() {
    VALUE	sax_module = rb_const_get_at(Ox, rb_intern("Sax"));

    sax_value_class = rb_define_class_under(sax_module, "Value", rb_cObject);

    rb_define_method(sax_value_class, "as_s", sax_value_as_s, 0);
    rb_define_method(sax_value_class, "as_sym", sax_value_as_sym, 0);
    rb_define_method(sax_value_class, "as_i", sax_value_as_i, 0);
    rb_define_method(sax_value_class, "as_f", sax_value_as_f, 0);
    rb_define_method(sax_value_class, "as_time", sax_value_as_time, 0);
    rb_define_method(sax_value_class, "as_bool", sax_value_as_bool, 0);
    rb_define_method(sax_value_class, "empty?", sax_value_empty, 0);
}
