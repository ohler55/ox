/* ox.c
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * - Neither the name of Peter Ohler nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
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

void Init_ox();

VALUE    Ox = Qnil;

ID      at_id;
ID      attributes_id;
ID      beg_id;
ID      den_id;
ID      end_id;
ID      excl_id;
ID      inspect_id;
ID      keys_id;
ID      local_id;
ID      nodes_id;
ID      num_id;
ID      parse_id;
ID      to_c_id;
ID      to_s_id;
ID      tv_sec_id;
ID      tv_usec_id;
ID      value_id;

VALUE   version_sym;
VALUE   standalone_sym;
VALUE   encoding_sym;
VALUE   indent_sym;
VALUE   xsd_date_sym;
VALUE   opt_format_sym;
VALUE   circular_sym;
VALUE   mode_sym;
VALUE   auto_sym;
VALUE   optimized_sym;
VALUE   object_sym;
VALUE   generic_sym;
VALUE   limited_sym;
VALUE   best_effort_sym;
VALUE   trace_sym;
VALUE   empty_string;
VALUE   zero_fixnum;

VALUE   ox_cdata_clas;
VALUE   ox_comment_clas;
VALUE   ox_doctype_clas;
VALUE   ox_document_clas;
VALUE   ox_element_clas;
VALUE   ox_text_clas;
VALUE   struct_class;
VALUE   time_class;

Cache   symbol_cache = 0;
Cache   class_cache = 0;
Cache   attr_cache = 0;

extern ParseCallbacks   ox_obj_callbacks;
extern ParseCallbacks   ox_gen_callbacks;
extern ParseCallbacks   ox_limited_callbacks;

static void     parse_dump_options(VALUE options, int *indent, int *xsd_date, int *circular);

/* call-seq: parse_obj(xml) => Object
 *
 * Parses an XML document String that is in the object format and returns an
 * Object of the type represented by the XML. This function expects an
 * optimized XML formated String. For other formats use the more generic
 * Ox.load() method.  Raises an exception if the XML is malformed or the
 * classes specified in the file are not valid.
 * [xml] XML String in optimized Object format.
 */
static VALUE
to_obj(VALUE self, VALUE ruby_xml) {
    VALUE       obj;
    char        *xml;

    Check_Type(ruby_xml, T_STRING);
    // the xml string gets modified so make a copy of it
    xml = strdup(StringValuePtr(ruby_xml));
    obj = parse(xml, ox_obj_callbacks, 0, 0, 0);
    free(xml);
    return obj;
}

/* call-seq: parse(xml) => Ox::Document or Ox::Element
 *
 * Parses and XML document String into an Ox::Document or Ox::Element.
 * Raises an exception if the XML is malformed.
 * [xml] XML String
 */
static VALUE
to_gen(VALUE self, VALUE ruby_xml) {
    VALUE       obj;
    char        *xml;

    Check_Type(ruby_xml, T_STRING);
    // the xml string gets modified so make a copy of it
    xml = strdup(StringValuePtr(ruby_xml));
    obj = parse(xml, ox_gen_callbacks, 0, 0, 0);
    free(xml);
    return obj;
}

typedef enum {
    AutoMode = 0, // not supported
    ObjMode  = 1,
    GenMode  = 2,
    LimMode  = 3,
} LoadMode;

static VALUE
load(char *xml, int argc, VALUE *argv, VALUE self) {
    VALUE       obj;
    int         mode = AutoMode;
    int         trace = 0;
    int         best_effort = 0;
    
    if (1 == argc && rb_cHash == rb_obj_class(*argv)) {
        VALUE   h = *argv;
        VALUE   v;
        
        if (Qnil != (v = rb_hash_lookup(h, mode_sym))) {
            if (auto_sym == v) {
                mode = AutoMode;
            } else if (object_sym == v) {
                mode = ObjMode;
            } else if (optimized_sym == v) {
                mode = ObjMode;
            } else if (generic_sym == v) {
                mode = GenMode;
            } else if (limited_sym == v) {
                mode = LimMode;
            } else {
                rb_raise(rb_eArgError, ":mode must be :generic, :object, or :limited.\n");
            }
        }
        if (Qnil != (v = rb_hash_lookup(h, trace_sym))) {
            Check_Type(v, T_FIXNUM);
            trace = FIX2INT(v);
        }
        if (Qnil != (v = rb_hash_lookup(h, best_effort_sym))) {
            best_effort = (Qfalse != v);
        }
    }
    switch (mode) {
    case ObjMode:
        obj = parse(xml, ox_obj_callbacks, 0, trace, best_effort);
        break;
    case GenMode:
        obj = parse(xml, ox_gen_callbacks, 0, trace, 0);
        break;
    case LimMode:
        obj = parse(xml, ox_limited_callbacks, 0, trace, best_effort);
        break;
    case AutoMode:
    default:
        obj = parse(xml, ox_gen_callbacks, 0, trace, 0);
        break;
    }
    free(xml);

    return obj;
}

/* call-seq: load(xml, options) => Ox::Document or Ox::Element or Object
 *
 * Parses and XML document String into an Ox::Document, or Ox::Element, or
 * Object depending on the options.  Raises an exception if the XML is
 * malformed or the classes specified are not valid.
 * [xml]     XML String
 * [options] load options
 *           [:mode]        format expected
 *                          [:object]  object format
 *                          [:generic] read as a generic XML file
 *                          [:limited] read as a generic XML file but with callbacks on text and elements events only
 *           [:trace]       trace level as a Fixnum, default: 0 (silent)
 *           [:best_effort] use best effort to create Objects using nil if undefined Class, default: 0
 */
static VALUE
load_str(int argc, VALUE *argv, VALUE self) {
    char        *xml;
    
    Check_Type(*argv, T_STRING);
    // the xml string gets modified so make a copy of it
    xml = strdup(StringValuePtr(*argv));

    return load(xml, argc - 1, argv + 1, self);
}

/* call-seq: load_file(file_path, xml, options) => Ox::Document or Ox::Element or Object
 *
 * Parses and XML document from a file into an Ox::Document, or Ox::Element,
 * or Object depending on the options.  Raises an exception if the XML is
 * malformed or the classes specified are not valid.
 * [file_path] file path to read the XML document from
 * [options] load options
 *           [:mode]        format expected
 *                          [:object]  object format
 *                          [:generic] read as a generic XML file
 *                          [:limited] read as a generic XML file but with callbacks on text and elements events only
 *           [:trace]       trace level as a Fixnum, default: 0 (silent)
 *           [:best_effort] use best effort to create Objects using nil if undefined Class, default: 0
 */
static VALUE
load_file(int argc, VALUE *argv, VALUE self) {
    char                *path;
    char                *xml;
    FILE                *f;
    unsigned long       len;
    
    Check_Type(*argv, T_STRING);
    path = StringValuePtr(*argv);
    if (0 == (f = fopen(path, "r"))) {
        rb_raise(rb_eIOError, "%s\n", strerror(errno));
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (0 == (xml = malloc(len + 1))) {
        fclose(f);
        rb_raise(rb_eNoMemError, "Could not allocate memory for %ld byte file.\n", len);
    }
    fseek(f, 0, SEEK_SET);
    if (len != fread(xml, 1, len, f)) {
        fclose(f);
        rb_raise(rb_eLoadError, "Failed to read %ld bytes from %s.\n", len, path);
    }
    fclose(f);
    xml[len] = '\0';

    return load(xml, argc - 1, argv + 1, self);
}

static void
parse_dump_options(VALUE options, int *indent, int *xsd_date, int *circular) {
    if (rb_cHash == rb_obj_class(options)) {
        VALUE   v;
        
        if (Qnil != (v = rb_hash_lookup(options, indent_sym))) {
            if (rb_cFixnum != rb_obj_class(v)) {
                rb_raise(rb_eArgError, ":indent must be a Fixnum.\n");
            }
            *indent = NUM2INT(v);
        }
        if (Qnil != (v = rb_hash_lookup(options, xsd_date_sym))) {
            VALUE       c = rb_obj_class(v);
            
            if (rb_cTrueClass == c) {
                *xsd_date = 1;
            } else if (rb_cFalseClass == c) {
                *xsd_date = 0;
            } else {
                rb_raise(rb_eArgError, ":xsd_date must be true or false.\n");
            }
        }
        if (Qnil != (v = rb_hash_lookup(options, circular_sym))) {
            VALUE       c = rb_obj_class(v);
            
            if (rb_cTrueClass == c) {
                *circular = 1;
            } else if (rb_cFalseClass == c) {
                *circular = 0;
            } else {
                rb_raise(rb_eArgError, ":circular must be true or false.\n");
            }
        }
    }
 }

/* call-seq: dump(obj, options) => xml-string
 *
 * Dumps an Object (obj) to a string.
 * [obj]     Object to serialize as an XML document String
 * [options] formating options
 *           [:indent]   number of spaces to use as the standard indention, default: 2
 *           [:xsd_date] use XSD date format if true, default: false
 *           [:circular] allow circular references, default: false
 */
static VALUE
dump(int argc, VALUE *argv, VALUE self) {
    char        *xml;
    int         indent = 2;
    int         xsd_date = 0;
    int         circular = 0;
    VALUE       rstr;
    
    if (2 == argc) {
        parse_dump_options(argv[1], &indent, &xsd_date, &circular);
    }
    if (0 == (xml = write_obj_to_str(*argv, indent, xsd_date, circular))) {
        rb_raise(rb_eNoMemError, "Not enough memory.\n");
    }
    rstr = rb_str_new2(xml);
    free(xml);

    return rstr;
}

/* call-seq: to_file(file_path, obj, options)
 *
 * Dumps and Object to the specified file.
 * [file_path] file path to write the XML document to
 * [obj]       Object to serialize as an XML document String
 * [options]   formating options
 *             [:indent]   number of spaces to use as the standard indention, default: 2
 *             [:xsd_date] use XSD date format if true, default: false
 *             [:circular] allow circular references, default: false
 */
static VALUE
to_file(int argc, VALUE *argv, VALUE self) {
    int         indent = 2;
    int         xsd_date = 0;
    int         circular = 0;
    
    if (3 == argc) {
        parse_dump_options(argv[2], &indent, &xsd_date, &circular);
    }
    Check_Type(*argv, T_STRING);
    write_obj_to_file(argv[1], StringValuePtr(*argv), indent, xsd_date, circular);

    return Qnil;
}

extern void     ox_cache_test(void);

static VALUE
cache_test(VALUE self) {
    ox_cache_test();
    return Qnil;
}

extern void     ox_cache8_test(void);

static VALUE
cache8_test(VALUE self) {
    ox_cache8_test();
    return Qnil;
}

void Init_ox() {
    Ox = rb_define_module("Ox");
    rb_define_module_function(Ox, "parse_obj", to_obj, 1);
    rb_define_module_function(Ox, "parse", to_gen, 1);
    rb_define_module_function(Ox, "load", load_str, -1);

    rb_define_module_function(Ox, "to_xml", dump, -1);
    rb_define_module_function(Ox, "dump", dump, -1);

    rb_define_module_function(Ox, "load_file", load_file, -1);
    rb_define_module_function(Ox, "to_file", to_file, -1);

    rb_require("time");
    parse_id = rb_intern("parse");
    local_id = rb_intern("local");
    at_id = rb_intern("at");
    inspect_id = rb_intern("inspect");
    beg_id = rb_intern("@beg");
    end_id = rb_intern("@end");
    den_id = rb_intern("@den");
    excl_id = rb_intern("@excl");
    value_id = rb_intern("@value");
    nodes_id = rb_intern("@nodes");
    num_id = rb_intern("@num");
    attributes_id = rb_intern("@attributes");
    keys_id = rb_intern("keys");
    tv_sec_id = rb_intern("tv_sec");
    tv_usec_id = rb_intern("tv_usec");
    to_c_id = rb_intern("to_c");
    to_s_id = rb_intern("to_s");

    time_class = rb_const_get(rb_cObject, rb_intern("Time"));
    struct_class = rb_const_get(rb_cObject, rb_intern("Struct"));

    version_sym = ID2SYM(rb_intern("version"));
    standalone_sym = ID2SYM(rb_intern("standalone"));
    encoding_sym = ID2SYM(rb_intern("encoding"));
    indent_sym = ID2SYM(rb_intern("indent"));
    xsd_date_sym = ID2SYM(rb_intern("xsd_date"));
    opt_format_sym = ID2SYM(rb_intern("opt_format"));
    mode_sym = ID2SYM(rb_intern("mode"));
    auto_sym = ID2SYM(rb_intern("auto"));
    optimized_sym = ID2SYM(rb_intern("optimized"));
    object_sym = ID2SYM(rb_intern("object"));
    circular_sym = ID2SYM(rb_intern("circular"));
    generic_sym = ID2SYM(rb_intern("generic"));
    limited_sym = ID2SYM(rb_intern("limited"));
    trace_sym = ID2SYM(rb_intern("trace"));
    best_effort_sym = ID2SYM(rb_intern("best_effort"));
    empty_string = rb_str_new2("");
    zero_fixnum = INT2NUM(0);
    
    //rb_require("node"); // generic xml node classes
    ox_document_clas = rb_const_get(Ox, rb_intern("Document"));
    ox_element_clas = rb_const_get(Ox, rb_intern("Element"));
    ox_comment_clas = rb_const_get(Ox, rb_intern("Comment"));
    ox_doctype_clas = rb_const_get(Ox, rb_intern("DocType"));
    ox_cdata_clas = rb_const_get(Ox, rb_intern("CData"));

    ox_cache_new(&symbol_cache);
    ox_cache_new(&class_cache);
    ox_cache_new(&attr_cache);

    rb_define_module_function(Ox, "cache_test", cache_test, 0);
    rb_define_module_function(Ox, "cache8_test", cache8_test, 0);
}

void
_raise_error(const char *msg, const char *xml, const char *current, const char* file, int line) {
    int         xline = 1;
    int         col = 1;

    for (; xml < current && '\n' != *current; current--) {
        col++;
    }
    for (; xml < current; current--) {
        if ('\n' == *current) {
            xline++;
        }
    }
    rb_raise(rb_eEncodingError, "%s at line %d, column %d [%s:%d]\n", msg, xline, col, file, line);
}
