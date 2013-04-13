/* sax_as.c
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
#include "sax.h"

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

    if ('\0' == *dr->buf.str) {
	return Qnil;
    }
    if (dr->options.convert_special) {
	ox_sax_collapse_special(dr, dr->buf.str, dr->buf.line, dr->buf.col);
    }
    rs = rb_str_new2(dr->buf.str);
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

    if ('\0' == *dr->buf.str) {
	return Qnil;
    }
    return str2sym(dr, dr->buf.str, 0);
}

static VALUE
sax_value_as_f(VALUE self) {
    SaxDrive	dr = DATA_PTR(self);

    if ('\0' == *dr->buf.str) {
	return Qnil;
    }
    return rb_float_new(strtod(dr->buf.str, 0));
}

static VALUE
sax_value_as_i(VALUE self) {
    SaxDrive	dr = DATA_PTR(self);
    const char	*s = dr->buf.str;
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
    const char	*str = dr->buf.str;
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
    return (0 == strcasecmp("true", ((SaxDrive)DATA_PTR(self))->buf.str)) ? Qtrue : Qfalse;
}

static VALUE
sax_value_empty(VALUE self) {
    return ('\0' == *((SaxDrive)DATA_PTR(self))->buf.str) ? Qtrue : Qfalse;
}

void
ox_sax_define() {
    VALUE	sax_module = rb_const_get_at(Ox, rb_intern("Sax"));

    ox_sax_value_class = rb_define_class_under(sax_module, "Value", rb_cObject);

    rb_define_method(ox_sax_value_class, "as_s", sax_value_as_s, 0);
    rb_define_method(ox_sax_value_class, "as_sym", sax_value_as_sym, 0);
    rb_define_method(ox_sax_value_class, "as_i", sax_value_as_i, 0);
    rb_define_method(ox_sax_value_class, "as_f", sax_value_as_f, 0);
    rb_define_method(ox_sax_value_class, "as_time", sax_value_as_time, 0);
    rb_define_method(ox_sax_value_class, "as_bool", sax_value_as_bool, 0);
    rb_define_method(ox_sax_value_class, "empty?", sax_value_empty, 0);
}
