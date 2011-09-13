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
    union {
        FILE    *fp;
        VALUE   io;
    };
    int         has_instruct;
    int         has_doctype;
    int         has_comment;
    int         has_cdata;
    int         has_text;
    int         has_start_element;
    int         has_end_element;
    int         has_error;
} *SaxDrive;

static void     sax_drive_init(SaxDrive dr, VALUE handler, VALUE io);
static void     sax_drive_cleanup(SaxDrive dr);
static int      sax_drive_read(SaxDrive dr);
static int      sax_drive_expect(SaxDrive dr, const char *str, int len);
static void     sax_drive_error(SaxDrive dr, const char *msg);

static int      read_instruction(SaxDrive dr);
static int      read_attrs(SaxDrive dr, VALUE attrs, char c, char termc);
static char     read_name_token(SaxDrive dr);
static int      read_quoted_value(SaxDrive dr);

static VALUE    io_cb(VALUE rdr);
static int      read_from_io(SaxDrive dr);
static int      read_from_file(SaxDrive dr);

static inline char
sax_drive_get(SaxDrive dr) {
    if (dr->read_end <= dr->cur) {
        if (0 != sax_drive_read(dr)) {
            return 0;
        }
    }
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

void
ox_sax_parse(VALUE handler, VALUE io) {
    struct _SaxDrive    dr;
    char                c;
    int                 err = 0;
    
    sax_drive_init(&dr, handler, io);
#if 0
    printf("*** sax_parse with these flags\n");
    printf("    has_instruct = %s\n", dr.has_instruct ? "true" : "false");
    printf("    has_doctype = %s\n", dr.has_doctype ? "true" : "false");
    printf("    has_comment = %s\n", dr.has_comment ? "true" : "false");
    printf("    has_cdata = %s\n", dr.has_cdata ? "true" : "false");
    printf("    has_text = %s\n", dr.has_text ? "true" : "false");
    printf("    has_start_element = %s\n", dr.has_start_element ? "true" : "false");
    printf("    has_end_element = %s\n", dr.has_end_element ? "true" : "false");
    printf("    has_error = %s\n", dr.has_error ? "true" : "false");
#endif
    while (!err) {
        if ('\0' == (c = next_non_white(&dr))) {
            break; // normal completion
        }
	if ('<' != c) { // all top level entities start with <
            sax_drive_error(&dr, "invalid format, expected <");
            break; // unrecoverable
	}
        c = sax_drive_get(&dr);
	switch (c) {
	case '?':	// instructions (xml or otherwise)
	    err = read_instruction(&dr);
	    break;
	case '!':	/* comment or doctype */
            c = sax_drive_get(&dr);
	    if ('\0' == c) {
                sax_drive_error(&dr, "invalid format, DOCTYPE or comment not terminated");
                err = 1;
	    } else if ('-' == c) {
                c = sax_drive_get(&dr); // skip first - and get next character
		if ('-' != c) {
                    sax_drive_error(&dr, "invalid format, bad comment format");
                    err = 1;
		} else {
                    c = sax_drive_get(&dr); // skip second -
		    //read_comment(&dr);
		}
	    } else if (0 == sax_drive_expect(&dr, "DOCTYPE", 7)) {
		//read_doctype(&dr);
	    } else {
                sax_drive_error(&dr, "invalid format, DOCTYPE or comment expected");
                err = 1;
	    }
	    break;
	case '\0':
            sax_drive_error(&dr, "invalid format, document not terminated");
            err = 1;
            break;
	default:
	    //read_element(&dr);
	    break;
	}
    }
    sax_drive_cleanup(&dr);
}

static void
sax_drive_init(SaxDrive dr, VALUE handler, VALUE io) {
    if (T_STRING == rb_type(io)) {
        dr->read_func = read_from_file;
        dr->fp = fopen(StringValuePtr(io), "r");
    } else if (rb_respond_to(io, readpartial_id)) {
        dr->read_func = read_from_io;
        dr->io = io;
    } else {
        rb_raise(rb_eArgError, "sax_parser io argument must respond to read_nonblock().\n");
    }
    dr->buf = dr->base_buf;
    *dr->buf = '\0';
    dr->buf_end = dr->buf + sizeof(dr->base_buf) - 1; // 1 less to make debugging easier
    dr->cur = dr->buf;
    dr->read_end = dr->buf;
    dr->str = 0;
    dr->line = 0;
    dr->col = 0;
    dr->handler = handler;
    dr->has_instruct = rb_respond_to(handler, instruct_id);
    dr->has_doctype = rb_respond_to(handler, doctype_id);
    dr->has_comment = rb_respond_to(handler, comment_id);
    dr->has_cdata = rb_respond_to(handler, cdata_id);
    dr->has_text = rb_respond_to(handler, text_id);
    dr->has_start_element = rb_respond_to(handler, start_element_id);
    dr->has_end_element = rb_respond_to(handler, end_element_id);
    dr->has_error = rb_respond_to(handler, error_id);
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
    err = dr->read_func(dr); // TBD temporary

    *dr->read_end = '\0';
    //printf("\n*** sax_drive_read: '%s'\n", dr->buf);

    return err;
}

static int
sax_drive_expect(SaxDrive dr, const char *str, int len) {
    // TBD
    return 0;
}

static void
sax_drive_error(SaxDrive dr, const char *msg) {
    if (dr->has_error) {
        VALUE       args[3];

        args[0] = rb_str_new2(msg);
        args[1] = INT2FIX(dr->line);
        args[2] = INT2FIX(dr->col);
        rb_funcall2(dr->handler, error_id, 3, args);
    } else {
        rb_raise(rb_eSyntaxError, "%s at line %d, column %d\n", msg, dr->line, dr->col);
    }
}

/* Entered after the "<?" sequence. Ready to read the rest.
 */
static int
read_instruction(SaxDrive dr) {
    VALUE       target = Qnil;
    VALUE       attrs = Qnil;
    char        c;

    if ('\0' == (c = read_name_token(dr))) {
        return -1;
    }
    if (dr->has_instruct) {
        target = rb_str_new2(dr->str);
        attrs = rb_hash_new();
    }
    if (0 != read_attrs(dr, attrs, c, '?')) {
        return -1;
    }
    c = next_non_white(dr);
    if ('>' != c) {
        sax_drive_error(dr, "invalid format, instruction not terminated");
        return -1;
    }
    if (0 != dr->has_instruct) {
        VALUE       args[2];

        args[0] = target;
        args[1] = attrs;
        rb_funcall2(dr->handler, instruct_id, 2, args);
    }
    dr->str = 0;

    return 0;
}

static int
read_attrs(SaxDrive dr, VALUE attrs, char c, char termc) {
    VALUE       name = Qnil;
    
    dr->str = dr->cur; // lock it down
    if (is_white(c)) {
        c = next_non_white(dr);
    }
    while (termc != c) {
        dr->cur--;
        if ('\0' == c) {
            sax_drive_error(dr, "invalid format, processing instruction not terminated");
            return -1;
        }
        if ('\0' == (c = read_name_token(dr))) {
            return -1;
        }
        if (dr->has_instruct) {
            name = rb_str_new2(dr->str);
        }
        if (is_white(c)) {
            c = next_non_white(dr);
        }
        if ('=' != c) {
            sax_drive_error(dr, "invalid format, no attribute value");
            return -1;
        }
        if (0 != read_quoted_value(dr)) {
            return -1;
        }
        if (dr->has_instruct) {
            rb_hash_aset(attrs, name, rb_str_new2(dr->str));
        }
        c = next_non_white(dr);
    }
    return 0;
}

static char
read_name_token(SaxDrive dr) {
    char        c;

    dr->str = dr->cur; // make sure the start doesn't get compacted out
    c = *dr->cur; // TBD use get here instead
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
            sax_drive_error(dr, "invalid format, document not terminated");
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
    c = *dr->cur; // TBD use get here instead
    if (is_white(c)) {
        c = next_non_white(dr);
    } else {
        c = sax_drive_get(dr);
    }
    if ('"' != c) {
        sax_drive_error(dr, "invalid format, attibute value not in quotes");
        return -1;
    }
    dr->str = dr->cur;
    while ('"' != (c = sax_drive_get(dr))) {
        if ('\0' == c) {
            sax_drive_error(dr, "invalid format, quoted value not terminated");
            return -1;
        }
    }
    *(dr->cur - 1) = '\0'; // terminate value

    return 0;
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

    args[0] = SIZET2NUM(dr->buf_end - dr->cur);
    rstr = rb_funcall2(dr->io, readpartial_id, 1, args);
    str = StringValuePtr(rstr);
    cnt = strlen(str);
    //printf("*** read %lu bytes, str: '%s'\n", cnt, str);
    strcpy(dr->cur, str);
    dr->read_end = dr->cur + cnt;

    return Qnil;
}

static int
read_from_file(SaxDrive dr) {
    // TBD
    return 0;
}
