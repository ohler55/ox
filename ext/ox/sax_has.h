/* sax_has.h
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#ifndef OX_SAX_HAS_H
#define OX_SAX_HAS_H

typedef struct _has {
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
} *Has;

inline static int
respond_to(VALUE obj, ID method) {
    return rb_respond_to(obj, method);
}

inline static void
has_init(Has has, VALUE handler) {
    has->attr = respond_to(handler, ox_attr_id);
    has->attr_value = respond_to(handler, ox_attr_value_id);
    has->doctype = respond_to(handler, ox_doctype_id);
    has->comment = respond_to(handler, ox_comment_id);
    has->cdata = respond_to(handler, ox_cdata_id);
    has->text = respond_to(handler, ox_text_id);
    has->value = respond_to(handler, ox_value_id);
    has->start_element = respond_to(handler, ox_start_element_id);
    has->end_element = respond_to(handler, ox_end_element_id);
    has->error = respond_to(handler, ox_error_id);
}

#endif /* OX_SAX_HAS_H */
