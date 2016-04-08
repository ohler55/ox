/* builder.c
 * Copyright (c) 2011, 2016 Peter Ohler
 * All rights reserved.
 */

#include <stdlib.h>
//#include <errno.h>
//#include <stdint.h>
#include <stdio.h>
//#include <string.h>

#include "ox.h"
//#include "sax.h"

typedef struct _Builder {
    char	*buf;
    char	*end;
    char	*cur;
    int		indent;
} *Builder;

static VALUE	builder_class = Qundef;

static void
builder_init(Builder b) {
    b->buf = ALLOC_N(char, 65536);
    b->end = b->buf + 65536;
    b->cur = b->buf;
    b->indent = 0;
}

static void
builder_free(void *ptr) {
    Builder	b;

    if (0 == ptr) {
	return;
    }
    b = (Builder)ptr;
    xfree(b->buf);
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
	//oj_parse_options(argv[0], &sw->opts);
	// TBD get indent
	b->indent = 2;
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

    printf("*** b: %p\n", b);
    // TBD
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
    // TBD
    printf("*** b: %p\n", b);
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
    // TBD
    printf("*** b: %p\n", b);
    return Qnil;
}

/* call-seq: text(txt)
 *
 * Adds a text element to the XML string being formed.
 * @param [String] txt contents of the text field
 */
static VALUE
builder_text(VALUE self, VALUE txt) {
    Builder	b = (Builder)DATA_PTR(self);
    // TBD
    printf("*** b: %p\n", b);
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
    // TBD
    printf("*** b: %p\n", b);
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
    // TBD
    printf("*** b: %p\n", b);
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
    Builder	b = (Builder)DATA_PTR(self);
    VALUE	rstr = rb_str_new(b->buf, b->cur - b->buf);

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
