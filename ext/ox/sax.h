/* sax.h
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

#ifndef __OX_SAX_H__
#define __OX_SAX_H__

#include "sax_buf.h"
#include "sax_has.h"
#include "sax_stack.h"
#include "sax_hint.h"

typedef struct _SaxOptions {
    int			symbolize;
    int			convert_special;
    int			smart;
} *SaxOptions;

typedef struct _SaxDrive {
    struct _Buf		buf;
    struct _NStack	stack;	/* element name stack */
    VALUE		handler;
    VALUE		value_obj;
    struct _SaxOptions	options;
    int			err;
    struct _Has		has;
    Hints		hints;
#if HAS_ENCODING_SUPPORT
    rb_encoding *encoding;
#elif HAS_PRIVATE_ENCODING
    VALUE	encoding;
#endif
} *SaxDrive;

extern void	ox_sax_parse(VALUE handler, VALUE io, SaxOptions options);
extern void	ox_sax_drive_cleanup(SaxDrive dr);
extern void	ox_sax_drive_error(SaxDrive dr, const char *msg);
extern int	ox_sax_collapse_special(SaxDrive dr, char *str, int line, int col);

extern VALUE	ox_sax_value_class;

inline static VALUE
str2sym(SaxDrive dr, const char *str, char **strp) {
    VALUE       *slot;
    VALUE       sym;

    if (dr->options.symbolize) {
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
    } else {
	sym = rb_str_new2(str);
#if HAS_ENCODING_SUPPORT
	if (0 != dr->encoding) {
	    rb_enc_associate(sym, dr->encoding);
	}
#elif HAS_PRIVATE_ENCODING
	if (Qnil != dr->encoding) {
	    rb_funcall(sym, ox_force_encoding_id, 1, dr->encoding);
	}
#endif
	if (0 != strp) {
	    *strp = StringValuePtr(sym);
	}
    }
    return sym;
}

#endif /* __OX_SAX_H__ */
