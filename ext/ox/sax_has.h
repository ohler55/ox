/* sax_has.h
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

#ifndef __OX_SAX_HAS_H__
#define __OX_SAX_HAS_H__

typedef struct _Has {
    int         instruct;
    int         end_instruct;
    int         attr;
    int         attrs_done;
    int         attr_value;
    int         doctype;
    int         comment;
    int         cdata;
    int         text;
    int         value;
    int         start_element;
    int         end_element;
    int         error;
    int		line;
    int		column;
} *Has;

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

inline static void
has_init(Has has, VALUE handler) {
    has->instruct = respond_to(handler, ox_instruct_id);
    has->end_instruct = respond_to(handler, ox_end_instruct_id);
    has->attr = respond_to(handler, ox_attr_id);
    has->attr_value = respond_to(handler, ox_attr_value_id);
    has->attrs_done = respond_to(handler, ox_attrs_done_id);
    has->doctype = respond_to(handler, ox_doctype_id);
    has->comment = respond_to(handler, ox_comment_id);
    has->cdata = respond_to(handler, ox_cdata_id);
    has->text = respond_to(handler, ox_text_id);
    has->value = respond_to(handler, ox_value_id);
    has->start_element = respond_to(handler, ox_start_element_id);
    has->end_element = respond_to(handler, ox_end_element_id);
    has->error = respond_to(handler, ox_error_id);
    has->line = (Qtrue == rb_ivar_defined(handler, ox_at_line_id));
    has->column = (Qtrue == rb_ivar_defined(handler, ox_at_column_id));
}

#endif /* __OX_SAX_HAS_H__ */
