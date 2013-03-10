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
#if NEEDS_UIO
#include <sys/uio.h>    
#endif
#include <unistd.h>
#include <time.h>

#include "ruby.h"
#include "ox.h"

typedef struct _SaxDrive {
    char        base_buf[0x00010000];
    char        *buf;
    char        *buf_end;
    char        *cur;
    char        *read_end;      /* one past last character read */
    char        *str;           /* start of current string being read */
    int         line;
    int         col;
    VALUE       handler;
    VALUE	value_obj;
    int         (*read_func)(struct _SaxDrive *dr);
    int         convert_special;
    union {
        int     	fd;
        VALUE   	io;
	const char	*in_str;
    };
    int         has_instruct;
    int         has_end_instruct;
    int         has_attr;
    int         has_attr_value;
    int         has_doctype;
    int         has_comment;
    int         has_cdata;
    int         has_text;
    int         has_value;
    int         has_start_element;
    int         has_end_element;
    int         has_error;
    int		has_line;
    int		has_column;
#if HAS_ENCODING_SUPPORT
    rb_encoding *encoding;
#elif HAS_PRIVATE_ENCODING
    VALUE	encoding;
#endif
} *SaxDrive;

#ifdef NEEDS_STPCPY
char *stpncpy(char *dest, const char *src, size_t n);
#endif
static void		sax_drive_init(SaxDrive dr, VALUE handler, VALUE io, int convert);
static void		sax_drive_cleanup(SaxDrive dr);
static int		sax_drive_read(SaxDrive dr);
static void		sax_drive_error(SaxDrive dr, const char *msg, int critical);

static int		read_children(SaxDrive dr, int first);
static int		read_instruction(SaxDrive dr);
static int		read_doctype(SaxDrive dr);
static int		read_cdata(SaxDrive dr);
static int		read_comment(SaxDrive dr);
static int		read_element(SaxDrive dr);
static int		read_text(SaxDrive dr);
static const char*	read_attrs(SaxDrive dr, char c, char termc, char term2, int is_xml);
static char		read_name_token(SaxDrive dr);
static int		read_quoted_value(SaxDrive dr);
static int		collapse_special(char *str);

static VALUE		rescue_cb(VALUE rdr, VALUE err);
static VALUE		io_cb(VALUE rdr);
static VALUE		partial_io_cb(VALUE rdr);
static int		read_from_io(SaxDrive dr);
#ifndef JRUBY_RUBY
static int		read_from_fd(SaxDrive dr);
#endif
static int		read_from_io_partial(SaxDrive dr);
static int		read_from_str(SaxDrive dr);

static VALUE		sax_value_class;

/* This is only for CentOS 5.4 with Ruby 1.9.3-p0 and for OS X 10.6 and Solaris 10. */
#ifdef NEEDS_STPCPY
char *stpncpy(char *dest, const char *src, size_t n) {
    size_t	cnt = strlen(src) + 1;
    
    if (n < cnt) {
	cnt = n;
    }
    strncpy(dest, src, cnt);

    return dest + cnt - 1;
}
#endif

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

static inline void
backup(SaxDrive dr) {
    dr->cur--;
    dr->col--;	// should reverse wrap but not worth it
}

static inline void
reset_reader(SaxDrive dr, char *cur, int line, int col) {
    dr->cur = cur;
    dr->line = line;
    dr->col = col;
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
str2sym(const char *str, SaxDrive dr, char **strp) {
    VALUE       *slot;
    VALUE       sym;

    if (Qundef == (sym = ox_cache_get(ox_symbol_cache, str, &slot, strp))) {
#if HAS_ENCODING_SUPPORT
        if (0 != dr->encoding) {
	    VALUE	rstr = rb_str_new2(str);

            rb_enc_associate(rstr, dr->encoding);
	    sym = rb_funcall(rstr, ox_to_sym_id, 0);
        } else {
	    sym = ID2SYM(rb_intern(str));
	}
#elif HAS_PRIVATE_ENCODING
        if (Qnil != dr->encoding) {
	    VALUE	rstr = rb_str_new2(str);

	    rb_funcall(rstr, ox_force_encoding_id, 1, dr->encoding);
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
    printf("    has_end_instruct = %s\n", dr.has_end_instruct ? "true" : "false");
    printf("    has_attr = %s\n", dr.has_attr ? "true" : "false");
    printf("    has_attr_value = %s\n", dr.has_attr_value ? "true" : "false");
    printf("    has_doctype = %s\n", dr.has_doctype ? "true" : "false");
    printf("    has_comment = %s\n", dr.has_comment ? "true" : "false");
    printf("    has_cdata = %s\n", dr.has_cdata ? "true" : "false");
    printf("    has_text = %s\n", dr.has_text ? "true" : "false");
    printf("    has_value = %s\n", dr.has_value ? "true" : "false");
    printf("    has_start_element = %s\n", dr.has_start_element ? "true" : "false");
    printf("    has_end_element = %s\n", dr.has_end_element ? "true" : "false");
    printf("    has_error = %s\n", dr.has_error ? "true" : "false");
    printf("    has_line = %s\n", dr.has_line ? "true" : "false");
    printf("    has_column = %s\n", dr.has_column ? "true" : "false");
#endif
    read_children(&dr, 1);
    sax_drive_cleanup(&dr);
}

inline static int
respond_to(VALUE obj, ID method) {
#ifdef JRUBY_RUBY
    /* There is a bug in JRuby where rb_respond_to() returns true (1) even if
     * a method is private. */
    {
	VALUE	args[1];

	*args = ID2SYM(method);
	return (Qtrue == rb_funcall2(obj, rb_intern("respond_to?"), 1, args));
    }
#else
    return rb_respond_to(obj, method);
#endif
}

static void
sax_drive_init(SaxDrive dr, VALUE handler, VALUE io, int convert) {
    if (ox_stringio_class == rb_obj_class(io)) {
	VALUE	s = rb_funcall2(io, ox_string_id, 0, 0);

	dr->read_func = read_from_str;
	dr->in_str = StringValuePtr(s);
    } else if (rb_respond_to(io, ox_readpartial_id)) {
#ifdef JRUBY_RUBY
	dr->read_func = read_from_io_partial;
	dr->io = io;
#else
        VALUE   rfd;

        if (rb_respond_to(io, ox_fileno_id) && Qnil != (rfd = rb_funcall(io, ox_fileno_id, 0))) {
            dr->read_func = read_from_fd;
            dr->fd = FIX2INT(rfd);
        } else {
            dr->read_func = read_from_io_partial;
            dr->io = io;
        }
#endif
    } else if (rb_respond_to(io, ox_read_id)) {
#ifdef JRUBY_RUBY
	dr->read_func = read_from_io;
	dr->io = io;
#else
        VALUE   rfd;

        if (rb_respond_to(io, ox_fileno_id) && Qnil != (rfd = rb_funcall(io, ox_fileno_id, 0))) {
            dr->read_func = read_from_fd;
            dr->fd = FIX2INT(rfd);
        } else {
            dr->read_func = read_from_io;
            dr->io = io;
        }
#endif
    } else {
        rb_raise(ox_arg_error_class, "sax_parser io argument must respond to readpartial() or read().\n");
    }
    dr->buf = dr->base_buf;
    *dr->buf = '\0';
    dr->buf_end = dr->buf + sizeof(dr->base_buf) - 1; /* 1 less to make debugging easier */
    dr->cur = dr->buf;
    dr->read_end = dr->buf;
    dr->str = 0;
    dr->line = 1;
    dr->col = 0;
    dr->handler = handler;
    dr->value_obj = rb_data_object_alloc(sax_value_class, dr, 0, 0);
    rb_gc_register_address(&dr->value_obj);
    dr->convert_special = convert;
    dr->has_instruct = respond_to(handler, ox_instruct_id);
    dr->has_end_instruct = respond_to(handler, ox_end_instruct_id);
    dr->has_attr = respond_to(handler, ox_attr_id);
    dr->has_attr_value = respond_to(handler, ox_attr_value_id);
    dr->has_doctype = respond_to(handler, ox_doctype_id);
    dr->has_comment = respond_to(handler, ox_comment_id);
    dr->has_cdata = respond_to(handler, ox_cdata_id);
    dr->has_text = respond_to(handler, ox_text_id);
    dr->has_value = respond_to(handler, ox_value_id);
    dr->has_start_element = respond_to(handler, ox_start_element_id);
    dr->has_end_element = respond_to(handler, ox_end_element_id);
    dr->has_error = respond_to(handler, ox_error_id);
    dr->has_line = (Qtrue == rb_ivar_defined(handler, ox_at_line_id));
    dr->has_column = (Qtrue == rb_ivar_defined(handler, ox_at_column_id));
#if HAS_ENCODING_SUPPORT
    if ('\0' == *ox_default_options.encoding) {
	VALUE	encoding;

	if (rb_respond_to(io, ox_external_encoding_id) && Qnil != (encoding = rb_funcall(io, ox_external_encoding_id, 0))) {
	    dr->encoding = rb_enc_from_index(rb_enc_get_index(encoding));
	} else {
	    dr->encoding = 0;
	}
    } else {
        dr->encoding = rb_enc_find(ox_default_options.encoding);
    }
#elif HAS_PRIVATE_ENCODING
    if ('\0' == *ox_default_options.encoding) {
	VALUE	encoding;

	if (rb_respond_to(io, ox_external_encoding_id) && Qnil != (encoding = rb_funcall(io, ox_external_encoding_id, 0))) {
	    dr->encoding = encoding;
	} else {
	    dr->encoding = Qnil;
	}
    } else {
        dr->encoding = rb_str_new2(ox_default_options.encoding);
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
        /*printf("\n*** shift: %lu\n", shift); */
        if (0 == shift) { /* no space left so allocate more */
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
	if (dr->has_line) {
	    rb_ivar_set(dr->handler, ox_at_line_id, args[1]);
	}
	if (dr->has_column) {
	    rb_ivar_set(dr->handler, ox_at_column_id, args[2]);
	}
        rb_funcall2(dr->handler, ox_error_id, 3, args);
    } else if (critical) {
        sax_drive_cleanup(dr);
        rb_raise(ox_parse_error_class, "%s at line %d, column %d\n", msg, dr->line, dr->col);
    }
}

static int
read_children(SaxDrive dr, int first) {
    int         err = 0;
    int         element_read = !first;
    char        c;
    int		line;
    int		col;
    
    while (!err) {
        dr->str = dr->cur; /* protect the start */
	c = sax_drive_get(dr);
	if (first) {
	    if (0xEF == (uint8_t)c) { /* only UTF8 is supported */
		if (0xBB == (uint8_t)sax_drive_get(dr) && 0xBF == (uint8_t)sax_drive_get(dr)) {
#if HAS_ENCODING_SUPPORT
		    dr->encoding = ox_utf8_encoding;
#elif HAS_PRIVATE_ENCODING
		    dr->encoding = ox_utf8_encoding;
#endif
		    c = sax_drive_get(dr);
		} else {
		    sax_drive_error(dr, "invalid format, invalid BOM or a binary file.", 1);
		}
	    }
	}
        if ('\0' == c || (is_white(c) && '\0' == (c = next_non_white(dr)))) {
            if (!first) {
                sax_drive_error(dr, "invalid format, element not terminated", 1);
                err = 1;
            }
            break; /* normal completion if first */
        }
	if ('<' != c) {
            if (first) { /* all top level entities start with < */
                sax_drive_error(dr, "invalid format, expected <", 1);
                break; /* unrecoverable */
            }
            if (0 != (err = read_text(dr))) { /* finished when < is reached */
                break;
            }
	}
        dr->str = dr->cur; /* protect the start for elements */
        c = sax_drive_get(dr);
	switch (c) {
	case '?': /* instructions (xml or otherwise) */
	    err = read_instruction(dr);
	    break;
	case '!': /* comment or doctype */
            dr->str = dr->cur;
            c = sax_drive_get(dr);
	    if ('\0' == c) {
                sax_drive_error(dr, "invalid format, DOCTYPE or comment not terminated", 1);
                err = 1;
	    } else if ('-' == c) {
                c = sax_drive_get(dr); /* skip first - and get next character */
		if ('-' != c) {
                    sax_drive_error(dr, "invalid format, bad comment format", 1);
                    err = 1;
		} else {
                    c = sax_drive_get(dr); /* skip second - */
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
                    err = read_doctype(dr);
                } else if (0 == strncmp("[CDATA[", dr->str, 7)) {
                    err = read_cdata(dr);
                } else {
                    sax_drive_error(dr, "invalid format, DOCTYPE or comment expected", 1);
                    err = 1;
                }
	    }
	    break;
	case '/': /* element end */
	    line = dr->line;
	    col = dr->col;
            err = ('\0' == read_name_token(dr));
	    dr->line = line;
	    dr->col = col;
	    return err;
            break;
	case '\0':
            sax_drive_error(dr, "invalid format, document not terminated", 1);
            err = 1;
            break;
	default:
	    backup(dr); /* safe since no read occurred after getting last character */
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

static void
read_content(SaxDrive dr, char *content, size_t len) {
    char	c;
    char	*end = content + len;

    while ('\0' != (c = sax_drive_get(dr))) {
	if (end < content) {
	    sax_drive_error(dr, "processing instruction content too large", 1);
	}
	if ('?' == c) {
	    if ('\0' == (c = sax_drive_get(dr))) {
		sax_drive_error(dr, "invalid format, document not terminated", 1);
	    }
	    if ('>' == c) {
		*content = '\0';
		return;
	    } else {
		*content++ = c;
	    }
	} else {
	    *content++ = c;
	}
    }
    *content = '\0';
}

/* Entered after the "<?" sequence. Ready to read the rest.
 */
static int
read_instruction(SaxDrive dr) {
    char	content[1024];
    char        c;
    char	*cend;
    const char	*err;
    VALUE	target = Qnil;
    int		is_xml;
    int		line = dr->line;
    int		col = dr->col - 1;

    if ('\0' == (c = read_name_token(dr))) {
        return -1;
    }
    is_xml = (0 == strcmp("xml", dr->str));
    if (dr->has_instruct || dr->has_end_instruct) {
	target = rb_str_new2(dr->str);
    }
    if (dr->has_instruct) {
        VALUE       args[1];

	if (dr->has_line) {
	    rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	}
	if (dr->has_column) {
	    rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	}
        args[0] = target;
        rb_funcall2(dr->handler, ox_instruct_id, 1, args);
    }
    dr->str = dr->cur; /* make sure the start doesn't get compacted out */
    line = dr->line;
    col = dr->col;
    read_content(dr, content, sizeof(content) - 1);
    cend = dr->cur;
    reset_reader(dr, dr->str, line, col);
    if (0 != (err = read_attrs(dr, c, '?', '?', is_xml))) {
	if (dr->has_text) {
	    VALUE   args[1];

	    if (dr->convert_special) {
		if (0 != collapse_special(content)) {
		    sax_drive_error(dr, "invalid format, special character does not end with a semicolon", 0);
		}
	    }
	    args[0] = rb_str_new2(content);
#if HAS_ENCODING_SUPPORT
	    if (0 != dr->encoding) {
		rb_enc_associate(args[0], dr->encoding);
	    }
#elif HAS_PRIVATE_ENCODING
	    if (Qnil != dr->encoding) {
		rb_funcall(args[0], ox_force_encoding_id, 1, dr->encoding);
	    }
#endif
	    if (dr->has_line) {
		rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	    }
	    if (dr->has_column) {
		rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	    }
	    rb_funcall2(dr->handler, ox_text_id, 1, args);
	}
	dr->cur = cend;
    } else {
	line = dr->line;
	col = dr->col;
	c = next_non_white(dr);
	if ('>' != c) {
	    sax_drive_error(dr, "invalid format, instruction not terminated", 1);
	    return -1;
	}
    }
    if (dr->has_end_instruct) {
        VALUE       args[1];

	if (dr->has_line) {
	    rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	}
	if (dr->has_column) {
	    rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	}
        args[0] = target;
        rb_funcall2(dr->handler, ox_end_instruct_id, 1, args);
    }
    dr->str = 0;

    return 0;
}

/* Entered after the "<!DOCTYPE" sequence. Ready to read the rest.
 */
static int
read_doctype(SaxDrive dr) {
    char        c;
    int		line = dr->line;
    int		col = dr->col - 10;

    dr->str = dr->cur - 1; /* mark the start */
    while ('>' != (c = sax_drive_get(dr))) {
        if ('\0' == c) {
            sax_drive_error(dr, "invalid format, doctype terminated unexpectedly", 1);
            return -1;
        }
    }
    *(dr->cur - 1) = '\0';
    if (dr->has_doctype) {
        VALUE       args[1];

	if (dr->has_line) {
	    rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	}
	if (dr->has_column) {
	    rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	}
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
    int		line = dr->line;
    int		col = dr->col - 10;

    backup(dr); /* back up to the start in case the cdata is empty */
    dr->str = dr->cur; /* mark the start */
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
#if HAS_ENCODING_SUPPORT
        if (0 != dr->encoding) {
            rb_enc_associate(args[0], dr->encoding);
        }
#elif HAS_PRIVATE_ENCODING
        if (Qnil != dr->encoding) {
	    rb_funcall(args[0], ox_force_encoding_id, 1, dr->encoding);
        }
#endif
	if (dr->has_line) {
	    rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	}
	if (dr->has_column) {
	    rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	}
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
    int		line = dr->line;
    int		col = dr->col - 5;

    dr->str = dr->cur - 1; /* mark the start */
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
#if HAS_ENCODING_SUPPORT
        if (0 != dr->encoding) {
            rb_enc_associate(args[0], dr->encoding);
        }
#elif HAS_PRIVATE_ENCODING
        if (Qnil != dr->encoding) {
	    rb_funcall(args[0], ox_force_encoding_id, 1, dr->encoding);
        }
#endif
	if (dr->has_line) {
	    rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	}
	if (dr->has_column) {
	    rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	}
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
    char	*ename = 0;
    VALUE       name = Qnil;
    const char	*err;
    char        c;
    int         closed;
    int		line = dr->line;
    int		col = dr->col - 1;

    if ('\0' == (c = read_name_token(dr))) {
        return -1;
    }
    name = str2sym(dr->str, dr, &ename);
    if (dr->has_start_element) {
        VALUE       args[1];

	if (dr->has_line) {
	    rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	}
	if (dr->has_column) {
	    rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	}
        args[0] = name;
        rb_funcall2(dr->handler, ox_start_element_id, 1, args);
    }
    if ('/' == c) {
        closed = 1;
    } else if ('>' == c) {
        closed = 0;
    } else {
        if (0 != (err = read_attrs(dr, c, '/', '>', 0))) {
	    sax_drive_error(dr, err, 1);
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
	line = dr->line;
	col = dr->col - 1;
        if (dr->has_end_element) {
            VALUE       args[1];

	    if (dr->has_line) {
		rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	    }
	    if (dr->has_column) {
		rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	    }
            args[0] = name;
            rb_funcall2(dr->handler, ox_end_element_id, 1, args);
        }
    } else {
        if (0 != read_children(dr, 0)) {
            return -1;
        }
	line = dr->line;
	col = dr->col;
	// read_children reads up to the end of the terminating element name
	dr->col += (uint32_t)(dr->cur - dr->str);
	if (0 != ename && 0 != strcmp(ename, dr->str)) {
	    if (dr->has_line) {
		rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	    }
	    if (dr->has_column) {
		rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	    }
	    //printf("*** ename: %s  close: %s\n", ename, dr->str);
            sax_drive_error(dr, "invalid format, element start and end names do not match", 1);
	    return -1;
	}
        if (0 != dr->has_end_element) {
            VALUE       args[1];

	    if (dr->has_line) {
		rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	    }
	    if (dr->has_column) {
		rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col - 2));
	    }
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
    int		line = dr->line;
    int		col = dr->col - 1;

    /* start marked in read_children */
    /*dr->str = dr->cur - 1; / * mark the start */
    while ('<' != (c = sax_drive_get(dr))) {
        if ('\0' == c) {
            sax_drive_error(dr, "invalid format, text terminated unexpectedly", 1);
            return -1;
        }
    }
    *(dr->cur - 1) = '\0';
    if (dr->has_value) {
        VALUE   args[1];

	if (dr->has_line) {
	    rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	}
	if (dr->has_column) {
	    rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	}
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
#if HAS_ENCODING_SUPPORT
        if (0 != dr->encoding) {
            rb_enc_associate(args[0], dr->encoding);
        }
#elif HAS_PRIVATE_ENCODING
        if (Qnil != dr->encoding) {
	    rb_funcall(args[0], ox_force_encoding_id, 1, dr->encoding);
        }
#endif
	if (dr->has_line) {
	    rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	}
	if (dr->has_column) {
	    rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	}
        rb_funcall2(dr->handler, ox_text_id, 1, args);
    }
    dr->str = 0;

    return 0;
}

static const char*
read_attrs(SaxDrive dr, char c, char termc, char term2, int is_xml) {
    VALUE       name = Qnil;
    int         is_encoding = 0;
    int		line;
    int		col;
    
    dr->str = dr->cur; /* lock it down */
    if (is_white(c)) {
        c = next_non_white(dr);
    }
    while (termc != c && term2 != c) {
        backup(dr);
	line = dr->line;
	col = dr->col;
        if ('\0' == c) {
	    return "invalid format, attributes not terminated";
        }
        if ('\0' == (c = read_name_token(dr))) {
            return "error reading tolen";
        }
        if (is_xml && 0 == strcmp("encoding", dr->str)) {
            is_encoding = 1;
        }
	/* TBD use symbol cache */
        if (dr->has_attr || dr->has_attr_value) {
            name = str2sym(dr->str, dr, 0);
        }
        if (is_white(c)) {
            c = next_non_white(dr);
        }
        if ('=' != c) {
            return "invalid format, no attribute value";
        }
        if (0 != read_quoted_value(dr)) {
            return "error reading quoted value";
        }
        if (is_encoding) {
#if HAS_ENCODING_SUPPORT
            dr->encoding = rb_enc_find(dr->str);
#elif HAS_PRIVATE_ENCODING
	    dr->encoding = rb_str_new2(dr->str);
#endif
            is_encoding = 0;
        }
        if (dr->has_attr_value) {
            VALUE       args[2];

	    if (dr->has_line) {
		rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	    }
	    if (dr->has_column) {
		rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	    }
            args[0] = name;
            args[1] = dr->value_obj;
            rb_funcall2(dr->handler, ox_attr_value_id, 2, args);
	} else if (dr->has_attr) {
            VALUE       args[2];

            args[0] = name;
            if (0 != collapse_special(dr->str) && 0 != strchr(dr->str, '&')) {
                sax_drive_error(dr, "invalid format, special character does not end with a semicolon", 0);
            }
            args[1] = rb_str_new2(dr->str);
#if HAS_ENCODING_SUPPORT
            if (0 != dr->encoding) {
                rb_enc_associate(args[1], dr->encoding);
            }
#elif HAS_PRIVATE_ENCODING
	    if (Qnil != dr->encoding) {
		rb_funcall(args[1], ox_force_encoding_id, 1, dr->encoding);
	    }
#endif
	    if (dr->has_line) {
		rb_ivar_set(dr->handler, ox_at_line_id, INT2FIX(line));
	    }
	    if (dr->has_column) {
		rb_ivar_set(dr->handler, ox_at_column_id, INT2FIX(col));
	    }
            rb_funcall2(dr->handler, ox_attr_id, 2, args);
        }
        c = next_non_white(dr);
    }
    dr->str = 0;

    return 0;
}

static char
read_name_token(SaxDrive dr) {
    char        c;

    dr->str = dr->cur; /* make sure the start doesn't get compacted out */
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
            /* documents never terminate after a name token */
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
    *(dr->cur - 1) = '\0'; /* terminate value */

    return 0;
}

static VALUE
rescue_cb(VALUE rdr, VALUE err) {
#ifndef JRUBY_RUBY
    /* JRuby seems to play by a different set if rules. It passes in an Fixnum
     * instead of an error like other Rubies. For now assume all errors are
     * EOF and deal with the results further down the line. */
#if (defined(RUBINIUS_RUBY) || (1 == RUBY_VERSION_MAJOR && 8 == RUBY_VERSION_MINOR))
    if (rb_obj_class(err) != rb_eTypeError) {
#else
    if (rb_obj_class(err) != rb_eEOFError) {
#endif
	SaxDrive	dr = (SaxDrive)rdr;

        sax_drive_cleanup(dr);
        rb_raise(err, "at line %d, column %d\n", dr->line, dr->col);
    }
#endif
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
    /*printf("*** read %lu bytes, str: '%s'\n", cnt, str); */
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
    /*args[0] = SIZET2NUM(dr->buf_end - dr->cur); */
    rstr = rb_funcall2(dr->io, ox_read_id, 1, args);
    str = StringValuePtr(rstr);
    cnt = strlen(str);
    /*printf("*** read %lu bytes, str: '%s'\n", cnt, str); */
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

#ifndef JRUBY_RUBY
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
#endif

static int
read_from_str(SaxDrive dr) {
    size_t      max = dr->buf_end - dr->cur - 1;
    char	*s;
    long	cnt;

    if ('\0' == *dr->in_str) {
	/* done */
	return -1;
    }
    s = stpncpy(dr->cur, dr->in_str, max);
    *s = '\0';
    cnt = s - dr->cur;
    dr->in_str += cnt;
    dr->read_end = dr->cur + cnt;

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
    for (; text - dot <= 9; text++) {
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
    char        c = '\0';
    struct _Tp  tpa[10] = { { 4, '-', '-' },
                           { 2, '-', '-' },
                           { 2, 'T', ' ' },
                           { 2, ':', ':' },
                           { 2, ':', ':' },
                           { 2, '.', '.' },
                           { 9, '+', '-' },
                           { 2, ':', ':' },
                           { 2, '\0', '\0' },
                           { 0, '\0', '\0' } };
    Tp          tp = tpa;
    struct tm   tm;

    memset(cargs, 0, sizeof(cargs));
    for (; 0 != tp->cnt; tp++) {
        for (i = tp->cnt, v = 0; 0 < i ; text++, i--) {
            c = *text;
            if (c < '0' || '9' < c) {
                if ('\0' == c || tp->end == c || tp->alt == c) {
                    break;
                }
		return Qnil;
            }
            v = 10 * v + (long)(c - '0');
        }
	if ('\0' == c) {
	    break;
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
    VALUE	rs;

    if ('\0' == *dr->str) {
	return Qnil;
    }
    if (dr->convert_special) {
	if (0 != collapse_special(dr->str) && 0 != strchr(dr->str, '&')) {
	    sax_drive_error(dr, "invalid format, special character does not end with a semicolon", 0);
	}
    }
    rs = rb_str_new2(dr->str);
#if HAS_ENCODING_SUPPORT
    if (0 != dr->encoding) {
	rb_enc_associate(rs, dr->encoding);
    }
#elif HAS_PRIVATE_ENCODING
    if (Qnil != dr->encoding) {
	rb_funcall(rs, ox_force_encoding_id, 1, dr->encoding);
    }
#endif
    return rs;
}

static VALUE
sax_value_as_sym(VALUE self) {
    SaxDrive	dr = DATA_PTR(self);

    if ('\0' == *dr->str) {
	return Qnil;
    }
    return str2sym(dr->str, dr, 0);
}

static VALUE
sax_value_as_f(VALUE self) {
    SaxDrive	dr = DATA_PTR(self);

    if ('\0' == *dr->str) {
	return Qnil;
    }
    return rb_float_new(strtod(dr->str, 0));
}

static VALUE
sax_value_as_i(VALUE self) {
    SaxDrive	dr = DATA_PTR(self);
    const char	*s = dr->str;
    long	n = 0;
    int		neg = 0;

    if ('\0' == *s) {
	return Qnil;
    }
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
	    rb_raise(ox_arg_error_class, "Not a valid Fixnum.\n");
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

    if ('\0' == *str) {
	return Qnil;
    }
    if (Qnil == (t = parse_double_time(str)) &&
	Qnil == (t = parse_xsd_time(str))) {
        VALUE       args[1];

        /*printf("**** time parse\n"); */
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
