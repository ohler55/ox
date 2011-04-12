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

static void     add_prolog(PInfo pi, const char *version, const char *encoding, const char *standalone);
static void     add_doctype(PInfo pi, const char *docType);
static void     add_comment(PInfo pi, const char *comment);
static void     add_cdata(PInfo pi, const char *cdata, size_t len);
static void     add_text(PInfo pi, char *text, int closed);
static void     add_element(PInfo pi, const char *ename, Attr attrs, int hasChildren);
static void     end_element(PInfo pi, const char *ename);

struct _ParseCallbacks   _ox_gen_callbacks = {
    add_prolog,
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

static void
add_prolog(PInfo pi, const char *version, const char *encoding, const char *standalone) {
    VALUE       doc;
    VALUE       ah;
    VALUE       nodes;
    
    if (0 != pi->h) { // top level object
        rb_raise(rb_eEncodingError, "Prolog must be the first element in an XML document.\n");
    }
    pi->h = pi->helpers;
    doc = rb_obj_alloc(ox_document_clas);
    ah = rb_hash_new();
    if (0 != version) {
        rb_hash_aset(ah, version_sym, rb_str_new2(version));
    }
    if (0 != encoding) {
        rb_hash_aset(ah, encoding_sym, rb_str_new2(encoding));
        pi->encoding = rb_enc_find(encoding);
    }
    if (0 != standalone) {
        rb_hash_aset(ah, standalone_sym, rb_str_new2(standalone));
    }
    nodes = rb_ary_new();
    rb_ivar_set(doc, attributes_id, ah);
    rb_ivar_set(doc, nodes_id, nodes);
    pi->h->obj = nodes;
    pi->obj = doc;
}

static void
add_doctype(PInfo pi, const char *docType) {
    VALUE       n = rb_obj_alloc(ox_doctype_clas);
    VALUE       s = rb_str_new2(docType);

    if (0 != pi->encoding) {
        rb_enc_associate(s, pi->encoding);
    }
    rb_ivar_set(n, value_id, s);
    rb_ary_push(pi->h->obj, n);
}

static void
add_comment(PInfo pi, const char *comment) {
    VALUE       n = rb_obj_alloc(ox_comment_clas);
    VALUE       s = rb_str_new2(comment);

    if (0 != pi->encoding) {
        rb_enc_associate(s, pi->encoding);
    }
    rb_ivar_set(n, value_id, s);
    rb_ary_push(pi->h->obj, n);
}

static void
add_cdata(PInfo pi, const char *cdata, size_t len) {
    VALUE       n = rb_obj_alloc(ox_cdata_clas);
    VALUE       s = rb_str_new2(cdata);

    if (0 != pi->encoding) {
        rb_enc_associate(s, pi->encoding);
    }
    rb_ivar_set(n, value_id, s);
    rb_ary_push(pi->h->obj, n);
}

static void
add_text(PInfo pi, char *text, int closed) {
    VALUE       s = rb_str_new2(text);

    if (0 != pi->encoding) {
        rb_enc_associate(s, pi->encoding);
    }
    rb_ary_push(pi->h->obj, s);
}

static void
add_element(PInfo pi, const char *ename, Attr attrs, int hasChildren) {
    VALUE       e;
    VALUE       s = rb_str_new2(ename);

    if (0 != pi->encoding) {
        rb_enc_associate(s, pi->encoding);
    }
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
            if (0 != pi->encoding) {
                rb_enc_associate(s, pi->encoding);
            }
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
