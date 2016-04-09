/* builder.c
 * Copyright (c) 2011, 2016 Peter Ohler
 * All rights reserved.
 */

#include <stdlib.h>
//#include <errno.h>
//#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ox.h"
#include "buf.h"
#include "err.h"

typedef struct _Element {
    char	*name;
    char	buf[64];
} *Element;

typedef struct _Builder {
    struct _Buf		buf;
    int			indent;
    char		encoding[64];
    int			depth;
    struct _Element	stack[128];
} *Builder;

static VALUE		builder_class = Qundef;
static const char	indent_spaces[] = "\n                                                                                                                                "; // 128 spaces

static void
append_indent(Builder b) {
    if (0 == b->indent) {
	return;
    }
    if (b->buf.head < b->buf.tail) {
	// TBD \n and indent, keep in bounds of indent_spaces
	buf_append_string(&b->buf, indent_spaces, 1);
    }
}

static void
append_sym_str(Buf b, VALUE v) {
    const char	*s;
    int		len;
    
    switch (rb_type(v)) {
    case T_STRING:
	s = StringValuePtr(v);
	len = RSTRING_LEN(v);
	break;
    case T_SYMBOL:
	s = rb_id2name(SYM2ID(v));
	len = strlen(s);
	break;
    default:
	rb_raise(ox_arg_error_class, "expected a Symbol or String");
	break;
    }
    buf_append_string(b, s, len);
}

static void
builder_init(Builder b) {
    buf_init(&b->buf, 0);
    b->indent = ox_default_options.indent;
    *b->encoding = '\0';
    b->depth = 0;
    b->stack->name = NULL;
    *b->stack->buf = '\0';
}

static void
builder_free(void *ptr) {
    Builder	b;
    Element	e;
    int		d;
    
    if (0 == ptr) {
	return;
    }
    b = (Builder)ptr;
    buf_cleanup(&b->buf);
    for (e = b->stack, d = b->depth; 0 < d; d--, e++) {
	if (e->name != e->buf) {
	    free(e->name);
	}
    }
    xfree(ptr);
}

/* call-seq: new(options)
 *
 * Creates a new Builder.
 * @param [Hash] options formating options
 */
static VALUE
builder_new(int argc, VALUE *argv, VALUE self) {
    Builder	b = ALLOC(struct _Builder);
    
    builder_init(b);
    if (1 == argc) {
	volatile VALUE	v;
	
	if (Qnil != (v = rb_hash_lookup(argv[1], ox_indent_sym))) {
	    if (rb_cFixnum != rb_obj_class(v)) {
		rb_raise(ox_parse_error_class, ":indent must be a fixnum.\n");
	    }
	    b->indent = NUM2INT(v);
	}
    }
    return Data_Wrap_Struct(builder_class, NULL, builder_free, b);
}

/* call-seq: instruct(decl,options)
 *
 * Adds the top level <?xml?> element.
 * @param [String] decl 'xml' expected
 * @param [Hash] options version or encoding
 */
static VALUE
builder_instruct(int argc, VALUE *argv, VALUE self) {
    Builder	b = (Builder)DATA_PTR(self);

    append_indent(b);
    if (0 == argc) {
	buf_append_string(&b->buf, "<?xml?>", 7);
    } else {
	volatile VALUE	v;
	
	buf_append_string(&b->buf, "<?", 2);
	append_sym_str(&b->buf, *argv);
	if (1 < argc && rb_cHash == rb_obj_class(argv[1])) {
	    if (Qnil != (v = rb_hash_lookup(argv[1], ox_version_sym))) {
		if (rb_cString != rb_obj_class(v)) {
		    rb_raise(ox_parse_error_class, ":version must be a Symbol.\n");
		}
		buf_append_string(&b->buf, " version=\"", 10);
		buf_append_string(&b->buf, StringValuePtr(v), RSTRING_LEN(v));
		buf_append(&b->buf, '"');
	    }
	    if (Qnil != (v = rb_hash_lookup(argv[1], ox_encoding_sym))) {
		if (rb_cString != rb_obj_class(v)) {
		    rb_raise(ox_parse_error_class, ":encoding must be a Symbol.\n");
		}
		buf_append_string(&b->buf, " encoding=\"", 11);
		buf_append_string(&b->buf, StringValuePtr(v), RSTRING_LEN(v));
		buf_append(&b->buf, '"');
		strncmp(b->encoding, StringValuePtr(v), sizeof(b->encoding));
		b->encoding[sizeof(b->encoding) - 1] = '\0';
	    }
	    if (Qnil != (v = rb_hash_lookup(argv[1], ox_standalone_sym))) {
		if (rb_cString != rb_obj_class(v)) {
		    rb_raise(ox_parse_error_class, ":standalone must be a Symbol.\n");
		}
		buf_append_string(&b->buf, " standalone=\"", 13);
		buf_append_string(&b->buf, StringValuePtr(v), RSTRING_LEN(v));
		buf_append(&b->buf, '"');
	    }
	}
	buf_append_string(&b->buf, "?>", 2);
    }
    return Qnil;
}

/* call-seq: element(name,attributes)
 *
 * Adds an element with the name and attributes provided.
 * @param [String] name name of the element
 * @param [Hash] attributes of the element
 */
static VALUE
builder_element(int argc, VALUE *argv, VALUE self) {
    Builder	b = (Builder)DATA_PTR(self);
    // TBD
    printf("*** b: %p\n", b);
    return Qnil;
}

/* call-seq: comment(text)
 *
 * Adds a comment element to the XML string being formed.
 * @param [String] text contents of the comment
 */
static VALUE
builder_comment(VALUE self, VALUE text) {
    Builder	b = (Builder)DATA_PTR(self);

    append_indent(b);
    buf_append_string(&b->buf, "<!-- ", 5);
    buf_append_string(&b->buf, StringValuePtr(text), RSTRING_LEN(text));
    buf_append_string(&b->buf, " --/> ", 5);

    return Qnil;
}

/* call-seq: doctype(text)
 *
 * Adds a DOCTYPE element to the XML string being formed.
 * @param [String] text contents of the doctype
 */
static VALUE
builder_doctype(VALUE self, VALUE text) {
    Builder	b = (Builder)DATA_PTR(self);

    append_indent(b);
    buf_append_string(&b->buf, "<!DOCTYPE ", 10);
    buf_append_string(&b->buf, StringValuePtr(text), RSTRING_LEN(text));
    buf_append(&b->buf, '>');

    return Qnil;
}

/* call-seq: text(txt)
 *
 * Adds a text element to the XML string being formed.
 * @param [String] text contents of the text field
 */
static VALUE
builder_text(VALUE self, VALUE text) {
    Builder	b = (Builder)DATA_PTR(self);

    // TBD encode special
    buf_append_string(&b->buf, StringValuePtr(text), RSTRING_LEN(text));

    return Qnil;
}

/* call-seq: cdata(data)
 *
 * Adds a CDATA element to the XML string being formed.
 * @param [String] data contents of the CDATA element
 */
static VALUE
builder_cdata(VALUE self, VALUE raw) {
    Builder	b = (Builder)DATA_PTR(self);

    append_indent(b);
    buf_append_string(&b->buf, "<![CDATA[", 9);
    buf_append_string(&b->buf, StringValuePtr(raw), RSTRING_LEN(raw));
    buf_append_string(&b->buf, "]]>", 3);

    return Qnil;
}

/* call-seq: raw(data)
 *
 * Adds the provided string directly to the XML without formatting or modifications.
 * @param [String] data contents to be added
 */
static VALUE
builder_raw(VALUE self, VALUE raw) {
    Builder	b = (Builder)DATA_PTR(self);

    buf_append_string(&b->buf, StringValuePtr(raw), RSTRING_LEN(raw));

    return Qnil;
}

/* call-seq: pop()
 *
 * Closes the current element.
 */
static VALUE
builder_pop(VALUE self) {
    Builder	b = (Builder)DATA_PTR(self);
    // TBD
    printf("*** b: %p\n", b);
    return Qnil;
}

/* call-seq: close()
 *
 * Closes the all elements and the document.
 */
static VALUE
builder_close(VALUE self) {
    Builder	b = (Builder)DATA_PTR(self);
    // TBD
    printf("*** b: %p\n", b);
    return Qnil;
}

/* call-seq: to_s()
 *
 * Returns the JSON document string in what ever state the construction is at.
 */

/* Document-method: Ox::Builder#to_s
 *
 * Returns the JSON document string in what ever state the construction is at.
 */
extern VALUE builder_to_s(VALUE self) {
    Builder		b = (Builder)DATA_PTR(self);
    volatile VALUE	rstr;

    if ('\n' != *(b->buf.tail - 1)) {
	buf_append(&b->buf, '\n');
    }
    *b->buf.tail = '\0'; // for debugging
    rstr = rb_str_new(b->buf.head, buf_len(&b->buf));

    // TBD encode
    //return oj_encode(rstr);
    return rstr;
}

/*
 * Document-class: Ox::Builder
 *
 * An XML builder.
 */
void ox_init_builder(VALUE ox) {
#if 0
    ox = rb_define_module("Ox");
#endif
    builder_class = rb_define_class_under(ox, "Builder", rb_cObject);
    rb_define_module_function(builder_class, "new", builder_new, -1);
    rb_define_method(builder_class, "instruct", builder_instruct, -1);
    rb_define_method(builder_class, "comment", builder_comment, 1);
    rb_define_method(builder_class, "doctype", builder_doctype, 1);
    rb_define_method(builder_class, "element", builder_element, -1);
    rb_define_method(builder_class, "text", builder_text, 1);
    rb_define_method(builder_class, "cdata", builder_cdata, 1);
    rb_define_method(builder_class, "raw", builder_raw, 1);
    rb_define_method(builder_class, "pop", builder_pop, 0);
    rb_define_method(builder_class, "close", builder_close, 0);
    rb_define_method(builder_class, "to_s", builder_to_s, 0);
}
