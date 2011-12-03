/* gen_load.c
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
#include <stdarg.h>

#include "ruby.h"
#include "ox.h"

static void     instruct(PInfo pi, const char *target, Attr attrs);
static void	create_doc(PInfo pi);
static void     create_prolog_doc(PInfo pi, const char *target, Attr attrs);
static void     nomode_instruct(PInfo pi, const char *target, Attr attrs);
static void     add_doctype(PInfo pi, const char *docType);
static void     add_comment(PInfo pi, const char *comment);
static void     add_cdata(PInfo pi, const char *cdata, size_t len);
static void     add_text(PInfo pi, char *text, int closed);
static void     add_element(PInfo pi, const char *ename, Attr attrs, int hasChildren);
static void     end_element(PInfo pi, const char *ename);

extern ParseCallbacks   ox_obj_callbacks;

struct _ParseCallbacks   _ox_gen_callbacks = {
    instruct, // instruct,
    add_doctype,
    add_comment,
    add_cdata,
    add_text,
    add_element,
    end_element,
};

ParseCallbacks   ox_gen_callbacks = &_ox_gen_callbacks;

struct _ParseCallbacks   _ox_limited_callbacks = {
    0,
    0,
    0,
    0,
    add_text,
    add_element,
    end_element,
};

ParseCallbacks   ox_limited_callbacks = &_ox_limited_callbacks;

struct _ParseCallbacks   _ox_nomode_callbacks = {
    nomode_instruct,
    add_doctype,
    add_comment,
    add_cdata,
    add_text,
    add_element,
    end_element,
};

ParseCallbacks   ox_nomode_callbacks = &_ox_nomode_callbacks;

static void
create_doc(PInfo pi) {
    VALUE       doc;
    VALUE       nodes;

    pi->h = pi->helpers;
    doc = rb_obj_alloc(ox_document_clas);
    nodes = rb_ary_new();
    rb_ivar_set(doc, attributes_id, rb_hash_new());
    rb_ivar_set(doc, nodes_id, nodes);
    pi->h->obj = nodes;
    pi->obj = doc;
}

static void
create_prolog_doc(PInfo pi, const char *target, Attr attrs) {
    VALUE       doc;
    VALUE       ah;
    VALUE       nodes;

    if (0 != pi->h) { // top level object
        rb_raise(rb_eSyntaxError, "Prolog must be the first element in an XML document.\n");
    }
    pi->h = pi->helpers;
    doc = rb_obj_alloc(ox_document_clas);
    ah = rb_hash_new();
    for (; 0 != attrs->name; attrs++) {
        rb_hash_aset(ah, ID2SYM(rb_intern(attrs->name)), rb_str_new2(attrs->value));
#ifdef HAVE_RUBY_ENCODING_H
        if (0 == strcmp("encoding", attrs->name)) {
            pi->encoding = rb_enc_find(attrs->value);
        }
#endif
    }
    nodes = rb_ary_new();
    rb_ivar_set(doc, attributes_id, ah);
    rb_ivar_set(doc, nodes_id, nodes);
    pi->h->obj = nodes;
    pi->obj = doc;
}

static void
instruct(PInfo pi, const char *target, Attr attrs) {
    if (0 == strcmp("xml", target)) {
        create_prolog_doc(pi, target, attrs);
    } else if (0 == strcmp("ox", target)) {
        for (; 0 != attrs->name; attrs++) {
            if (0 == strcmp("version", attrs->name)) {
                if (0 != strcmp("1.0", attrs->value)) {
                    rb_raise(rb_eSyntaxError, "Only Ox XML Object version 1.0 supported, not %s.\n", attrs->value);
                }
            }
            // ignore other instructions
        }
    } else {
        if (TRACE <= pi->trace) {
            printf("Processing instruction %s ignored.\n", target);
        }
    }
}

static void
nomode_instruct(PInfo pi, const char *target, Attr attrs) {
    if (0 == strcmp("xml", target)) {
        create_prolog_doc(pi, target, attrs);
    } else if (0 == strcmp("ox", target)) {
        for (; 0 != attrs->name; attrs++) {
            if (0 == strcmp("version", attrs->name)) {
                if (0 != strcmp("1.0", attrs->value)) {
                    rb_raise(rb_eSyntaxError, "Only Ox XML Object version 1.0 supported, not %s.\n", attrs->value);
                }
            } else if (0 == strcmp("mode", attrs->name)) {
                if (0 == strcmp("object", attrs->value)) {
                    pi->pcb = ox_obj_callbacks;
                    pi->obj = Qnil;
                    pi->h = 0;
                } else if (0 == strcmp("generic", attrs->value)) {
                    pi->pcb = ox_gen_callbacks;
                } else if (0 == strcmp("limited", attrs->value)) {
                    pi->pcb = ox_limited_callbacks;
                    pi->obj = Qnil;
                    pi->h = 0;
                } else {
                    rb_raise(rb_eSyntaxError, "%s is not a valid processing instruction mode.\n", attrs->value);
                }
            }
        }
    } else {
        if (TRACE <= pi->trace) {
            printf("Processing instruction %s ignored.\n", target);
        }
    }
}

static void
add_doctype(PInfo pi, const char *docType) {
    VALUE       n = rb_obj_alloc(ox_doctype_clas);
    VALUE       s = rb_str_new2(docType);

#ifdef HAVE_RUBY_ENCODING_H
    if (0 != pi->encoding) {
        rb_enc_associate(s, pi->encoding);
    }
#endif
    rb_ivar_set(n, value_id, s);
    if (0 == pi->h) { // top level object
	create_doc(pi);
    }
    rb_ary_push(pi->h->obj, n);
}

static void
add_comment(PInfo pi, const char *comment) {
    VALUE       n = rb_obj_alloc(ox_comment_clas);
    VALUE       s = rb_str_new2(comment);

#ifdef HAVE_RUBY_ENCODING_H
    if (0 != pi->encoding) {
        rb_enc_associate(s, pi->encoding);
    }
#endif
    rb_ivar_set(n, value_id, s);
    if (0 == pi->h) { // top level object
	create_doc(pi);
    }
    rb_ary_push(pi->h->obj, n);
}

static void
add_cdata(PInfo pi, const char *cdata, size_t len) {
    VALUE       n = rb_obj_alloc(ox_cdata_clas);
    VALUE       s = rb_str_new2(cdata);

#ifdef HAVE_RUBY_ENCODING_H
    if (0 != pi->encoding) {
        rb_enc_associate(s, pi->encoding);
    }
#endif
    rb_ivar_set(n, value_id, s);
    if (0 == pi->h) { // top level object
	create_doc(pi);
    }
    rb_ary_push(pi->h->obj, n);
}

static void
add_text(PInfo pi, char *text, int closed) {
    VALUE       s = rb_str_new2(text);

#ifdef HAVE_RUBY_ENCODING_H
    if (0 != pi->encoding) {
        rb_enc_associate(s, pi->encoding);
    }
#endif
    if (0 == pi->h) { // top level object
	create_doc(pi);
    }
    rb_ary_push(pi->h->obj, s);
}

static void
add_element(PInfo pi, const char *ename, Attr attrs, int hasChildren) {
    VALUE       e;
    VALUE       s = rb_str_new2(ename);

#ifdef HAVE_RUBY_ENCODING_H
    if (0 != pi->encoding) {
        rb_enc_associate(s, pi->encoding);
    }
#endif
    e = rb_obj_alloc(ox_element_clas);
    rb_ivar_set(e, value_id, s);
    if (0 != attrs->name) {
        VALUE   ah = rb_hash_new();
        
        for (; 0 != attrs->name; attrs++) {
            VALUE   sym;
            VALUE   *slot;

            if (Qundef == (sym = ox_cache_get(symbol_cache, attrs->name, &slot))) {
                sym = ID2SYM(rb_intern(attrs->name));
                *slot = sym;
            }
            s = rb_str_new2(attrs->value);
#ifdef HAVE_RUBY_ENCODING_H
            if (0 != pi->encoding) {
                rb_enc_associate(s, pi->encoding);
            }
#endif
            rb_hash_aset(ah, sym, s);
        }
        rb_ivar_set(e, attributes_id, ah);
    }
    if (0 == pi->h) { // top level object
        pi->h = pi->helpers;
        pi->obj = e;
    } else {
        rb_ary_push(pi->h->obj, e);
        pi->h++;
    }
    if (hasChildren) {
        VALUE   nodes = rb_ary_new();

        rb_ivar_set(e, nodes_id, nodes);
        pi->h->obj = nodes;
    }
}

static void
end_element(PInfo pi, const char *ename) {
    if (0 != pi->h && pi->helpers <= pi->h) {
        pi->h--;
    }
}
