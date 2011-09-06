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

typedef struct _YesNoOpt {
    VALUE       sym;
    char        *attr;
} *YesNoOpt;

void Init_ox();

VALUE    Ox = Qnil;

ID      at_id;
ID      attributes_id;
ID      beg_id;
ID      cdata_id;
ID      comment_id;
ID      den_id;
ID      doctype_id;
ID      end_element_id;
ID      end_id;
ID      error_id;
ID      excl_id;
ID      inspect_id;
ID      instruct_id;
ID      keys_id;
ID      local_id;
ID      nodes_id;
ID      num_id;
ID      parse_id;
ID      read_nonblock_id;
ID      start_element_id;
ID      text_id;
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
VALUE   strict_sym;
VALUE   tolerant_sym;
VALUE   effort_sym;
VALUE   auto_define_sym;
VALUE   trace_sym;
VALUE   strict_sym;
VALUE   with_dtd_sym;
VALUE   with_instruct_sym;
VALUE   with_xml_sym;
VALUE   empty_string;
VALUE   zero_fixnum;

VALUE   ox_cdata_clas;
VALUE   ox_comment_clas;
VALUE   ox_doctype_clas;
VALUE   ox_document_clas;
VALUE   ox_element_clas;
VALUE   ox_bag_clas;
VALUE   struct_class;
VALUE   time_class;

Cache   symbol_cache = 0;
Cache   class_cache = 0;
Cache   attr_cache = 0;

static struct _Options  default_options = {
    { '\0' },           // encoding
    2,                  // indent
    0,                  // trace
    No,                 // with_dtd
    No,                 // with_xml
    No,                 // with_instruct
    No,                 // circular
    No,                 // xsd_date
    NoMode,             // mode
    StrictEffort,       // effort
};

extern ParseCallbacks   ox_obj_callbacks;
extern ParseCallbacks   ox_gen_callbacks;
extern ParseCallbacks   ox_limited_callbacks;
extern ParseCallbacks   ox_nomode_callbacks;

static void     parse_dump_options(VALUE ropts, Options copts);

/* call-seq: default_options() => Hash
 *
 * Returns the default load and dump options as a Hash. The options are
 * - indent: [Fixnum] number of spaces to indent each element in an XML document
 * - trace: [Fixnum] trace level where 0 is silent
 * - encoding: [String] character encoding for the XML file
 * - with_dtd: [true|false|nil] include DTD in the dump
 * - with_instruct: [true|false|nil] include instructions in the dump
 * - with_xml: [true|false|nil] include XML prolog in the dump
 * - circular: [true|false|nil] support circular references while dumping
 * - xsd_date: [true|false|nil] use XSD date format instead of decimal format
 * - mode: [:object|:generic|:limited|nil] load method to use for XML
 * - effort: [:strict|:tolerant|:auto_define] set the tolerance level for loading
 * @return [Hash] all current option settings.
 */
static VALUE
get_def_opts(VALUE self) {
    VALUE       opts = rb_hash_new();
    int         elen = (int)strlen(default_options.encoding);
    
    rb_hash_aset(opts, encoding_sym, (0 == elen) ? Qnil : rb_str_new(default_options.encoding, elen));
    rb_hash_aset(opts, indent_sym, INT2FIX(default_options.indent));
    rb_hash_aset(opts, trace_sym, INT2FIX(default_options.trace));
    rb_hash_aset(opts, with_dtd_sym, (Yes == default_options.with_dtd) ? Qtrue : ((No == default_options.with_dtd) ? Qfalse : Qnil));
    rb_hash_aset(opts, with_xml_sym, (Yes == default_options.with_xml) ? Qtrue : ((No == default_options.with_xml) ? Qfalse : Qnil));
    rb_hash_aset(opts, with_instruct_sym, (Yes == default_options.with_instruct) ? Qtrue : ((No == default_options.with_instruct) ? Qfalse : Qnil));
    rb_hash_aset(opts, circular_sym, (Yes == default_options.circular) ? Qtrue : ((No == default_options.circular) ? Qfalse : Qnil));
    rb_hash_aset(opts, xsd_date_sym, (Yes == default_options.xsd_date) ? Qtrue : ((No == default_options.xsd_date) ? Qfalse : Qnil));
    switch (default_options.mode) {
    case ObjMode:       rb_hash_aset(opts, mode_sym, object_sym);       break;
    case GenMode:       rb_hash_aset(opts, mode_sym, generic_sym);      break;
    case LimMode:       rb_hash_aset(opts, mode_sym, limited_sym);      break;
    case NoMode:
    default:            rb_hash_aset(opts, mode_sym, Qnil);             break;
    }
    switch (default_options.effort) {
    case StrictEffort:          rb_hash_aset(opts, effort_sym, strict_sym);             break;
    case TolerantEffort:        rb_hash_aset(opts, effort_sym, tolerant_sym);           break;
    case AutoEffort:            rb_hash_aset(opts, effort_sym, auto_define_sym);        break;
    case NoEffort:
    default:                    rb_hash_aset(opts, effort_sym, Qnil);                   break;
    }
    return opts;
}

/* call-seq: default_options=(opts)
 *
 * Sets the default options for load and dump.
 * @param [Hash] opts options to change
 * @param [Fixnum] :indent number of spaces to indent each element in an XML document
 * @param [Fixnum] :trace trace level where 0 is silent
 * @param [String] :encoding character encoding for the XML file
 * @param [true|false|nil] :with_dtd include DTD in the dump
 * @param [true|false|nil] :with_instruct include instructions in the dump
 * @param [true|false|nil] :with_xml include XML prolog in the dump
 * @param [true|false|nil] :circular support circular references while dumping
 * @param [true|false|nil] :xsd_date use XSD date format instead of decimal format
 * @param [:object|:generic|:limited|nil] :mode load method to use for XML
 * @param [:strict|:tolerant|:auto_define] :effort set the tolerance level for loading
 * @return [nil]
 */
static VALUE
set_def_opts(VALUE self, VALUE opts) {
    struct _YesNoOpt    ynos[] = {
        { with_xml_sym, &default_options.with_xml },
        { with_dtd_sym, &default_options.with_dtd },
        { with_instruct_sym, &default_options.with_instruct },
        { xsd_date_sym, &default_options.xsd_date },
        { circular_sym, &default_options.circular },
        { Qnil, 0 }
    };
    YesNoOpt    o;
    VALUE       v;
    
    Check_Type(opts, T_HASH);

    v = rb_hash_aref(opts, encoding_sym);
    if (Qnil == v) {
        *default_options.encoding = '\0';
    } else {
        Check_Type(v, T_STRING);
        strncpy(default_options.encoding, StringValuePtr(v), sizeof(default_options.encoding) - 1);
    }

    v = rb_hash_aref(opts, indent_sym);
    if (Qnil != v) {
        Check_Type(v, T_FIXNUM);
        default_options.indent = FIX2INT(v);
    }

    v = rb_hash_aref(opts, trace_sym);
    if (Qnil != v) {
        Check_Type(v, T_FIXNUM);
        default_options.trace = FIX2INT(v);
    }

    v = rb_hash_aref(opts, mode_sym);
    if (Qnil == v) {
        default_options.mode = NoMode;
    } else if (object_sym == v) {
        default_options.mode = ObjMode;
    } else if (generic_sym == v) {
        default_options.mode = GenMode;
    } else if (limited_sym == v) {
        default_options.mode = LimMode;
    } else {
        rb_raise(rb_eArgError, ":mode must be :object, :generic, :limited, or nil.\n");
    }

    v = rb_hash_aref(opts, effort_sym);
    if (Qnil == v) {
        default_options.effort = NoEffort;
    } else if (strict_sym == v) {
        default_options.effort = StrictEffort;
    } else if (tolerant_sym == v) {
        default_options.effort = TolerantEffort;
    } else if (auto_define_sym == v) {
        default_options.effort = AutoEffort;
    } else {
        rb_raise(rb_eArgError, ":effort must be :strict, :tolerant, :auto_define, or nil.\n");
    }
    for (o = ynos; 0 != o->attr; o++) {
        v = rb_hash_lookup(opts, o->sym);
        if (Qnil == v) {
            *o->attr = NotSet;
        } else if (Qtrue == v) {
            *o->attr = Yes;
        } else if (Qfalse == v) {
            *o->attr = No;
        } else {
            rb_raise(rb_eArgError, "%s must be true, false, or nil.\n", StringValuePtr(o->sym));
        }
    }
    return Qnil;
}

/* call-seq: parse_obj(xml) => Object
 *
 * Parses an XML document String that is in the object format and returns an
 * Object of the type represented by the XML. This function expects an
 * optimized XML formated String. For other formats use the more generic
 * Ox.load() method.  Raises an exception if the XML is malformed or the
 * classes specified in the file are not valid.
 * @param [String] xml XML String in optimized Object format.
 * @return [Object] deserialized Object.
 */
static VALUE
to_obj(VALUE self, VALUE ruby_xml) {
    VALUE       obj;
    char        *xml;

    Check_Type(ruby_xml, T_STRING);
    // the xml string gets modified so make a copy of it
    xml = strdup(StringValuePtr(ruby_xml));
    obj = parse(xml, ox_obj_callbacks, 0, 0, StrictEffort);
    free(xml);
    return obj;
}

/* call-seq: parse(xml) => Ox::Document or Ox::Element
 *
 * Parses and XML document String into an Ox::Document or Ox::Element.
 * @param [String] xml XML String
 * @return [Ox::Document or Ox::Element] parsed XML document.
 * @raise [Exception] if the XML is malformed.
 */
static VALUE
to_gen(VALUE self, VALUE ruby_xml) {
    VALUE       obj;
    char        *xml;

    Check_Type(ruby_xml, T_STRING);
    // the xml string gets modified so make a copy of it
    xml = strdup(StringValuePtr(ruby_xml));
    obj = parse(xml, ox_gen_callbacks, 0, 0, StrictEffort);
    free(xml);
    return obj;
}

static VALUE
load(char *xml, int argc, VALUE *argv, VALUE self) {
    VALUE               obj;
    struct _Options     options = default_options;
    
    if (1 == argc && rb_cHash == rb_obj_class(*argv)) {
        VALUE   h = *argv;
        VALUE   v;
        
        if (Qnil != (v = rb_hash_lookup(h, mode_sym))) {
            if (object_sym == v) {
                options.mode = ObjMode;
            } else if (optimized_sym == v) {
                options.mode = ObjMode;
            } else if (generic_sym == v) {
                options.mode = GenMode;
            } else if (limited_sym == v) {
                options.mode = LimMode;
            } else {
                rb_raise(rb_eArgError, ":mode must be :generic, :object, or :limited.\n");
            }
        }
        if (Qnil != (v = rb_hash_lookup(h, effort_sym))) {
            if (auto_define_sym == v) {
                options.effort = AutoEffort;
            } else if (tolerant_sym == v) {
                options.effort = TolerantEffort;
            } else if (strict_sym == v) {
                options.effort = StrictEffort;
            } else {
                rb_raise(rb_eArgError, ":effort must be :strict, :tolerant, or :auto_define.\n");
            }
        }
        if (Qnil != (v = rb_hash_lookup(h, trace_sym))) {
            Check_Type(v, T_FIXNUM);
            options.trace = FIX2INT(v);
        }
    }
    switch (options.mode) {
    case ObjMode:
        obj = parse(xml, ox_obj_callbacks, 0, options.trace, options.effort);
        break;
    case GenMode:
        obj = parse(xml, ox_gen_callbacks, 0, options.trace, StrictEffort);
        break;
    case LimMode:
        obj = parse(xml, ox_limited_callbacks, 0, options.trace, StrictEffort);
        break;
    case NoMode:
        obj = parse(xml, ox_nomode_callbacks, 0, options.trace, options.effort);
        break;
    default:
        obj = parse(xml, ox_gen_callbacks, 0, options.trace, StrictEffort);
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
 * @param [String] xml XML String
 * @param [Hash] options load options
 * @param [:object|:generic|:limited] :mode format expected
 *  - *:object* - object format
 *  - *:generic* - read as a generic XML file
 *  - *:limited* - read as a generic XML file but with callbacks on text and elements events only
 * @param [:strict|:tolerant|:auto_define] :effort effort to use when an undefined class is encountered, default: :strict
 *  - *:strict* - raise an NameError for missing classes and modules
 *  - *:tolerant* - return nil for missing classes and modules
 *  - *:auto_define* - auto define missing classes and modules
 * @param [Fixnum] :trace trace level as a Fixnum, default: 0 (silent)
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
 * @param [String] file_path file path to read the XML document from
 * @param [Hash] options load options
 * @param [:object|:generic|:limited] :mode format expected
 *  - *:object* - object format
 *  - *:generic* - read as a generic XML file
 *  - *:limited* - read as a generic XML file but with callbacks on text and elements events only
 * @param [:strict|:tolerant|:auto_define] :effort effort to use when an undefined class is encountered, default: :strict
 *  - *:strict* - raise an NameError for missing classes and modules
 *  - *:tolerant* - return nil for missing classes and modules
 *  - *:auto_define* - auto define missing classes and modules
 * @param [Fixnum] :trace trace level as a Fixnum, default: 0 (silent)
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

static size_t
read_from_io(SaxControl ctrl, char *buf, size_t max_len) {
    // TBD
    return 0;
}

static size_t
read_from_file(SaxControl ctrl, char *buf, size_t max_len) {
    // TBD
    return 0;
}

/* call-seq: sax_parse(handler, io, options) => Ox::Document or Ox::Element or Object
 *
 * Parses and IO stream or file containing an XML document into an
 * Ox::Document, or Ox::Element, or Object depending on the options.  Raises
 * an exception if the XML is malformed or the classes specified are not
 * valid.
 * @param [Ox::Sax] handler SAX like handler
 * @param [IO|String] io IO Object or file path to read from
 */
static VALUE
sax_parse(VALUE self, VALUE handler, VALUE io) {
    struct _SaxControl  ctrl;

    if (T_STRING == rb_type(io)) {
        ctrl.read_func = read_from_file;
        ctrl.fp = fopen(StringValuePtr(io), "r");
    } else if (rb_respond_to(io, read_nonblock_id)) {
        ctrl.read_func = read_from_io;
        ctrl.io = io;
    } else {
        rb_raise(rb_eArgError, "sax_parser io argument must respond to read_nonblock().\n");
    }
    ctrl.has_instruct = rb_respond_to(handler, instruct_id);
    ctrl.has_doctype = rb_respond_to(handler, doctype_id);
    ctrl.has_comment = rb_respond_to(handler, comment_id);
    ctrl.has_cdata = rb_respond_to(handler, cdata_id);
    ctrl.has_text = rb_respond_to(handler, text_id);
    ctrl.has_start_element = rb_respond_to(handler, start_element_id);
    ctrl.has_end_element = rb_respond_to(handler, end_element_id);
    ctrl.has_error = rb_respond_to(handler, error_id);

    ox_sax_parse(handler, &ctrl);

    return Qnil;
}

static void
parse_dump_options(VALUE ropts, Options copts) {
    struct _YesNoOpt    ynos[] = {
        { with_xml_sym, &copts->with_xml },
        { with_dtd_sym, &copts->with_dtd },
        { with_instruct_sym, &copts->with_instruct },
        { xsd_date_sym, &copts->xsd_date },
        { circular_sym, &copts->circular },
        { Qnil, 0 }
    };
    YesNoOpt    o;
    
    if (rb_cHash == rb_obj_class(ropts)) {
        VALUE   v;
        
        if (Qnil != (v = rb_hash_lookup(ropts, indent_sym))) {
            if (rb_cFixnum != rb_obj_class(v)) {
                rb_raise(rb_eArgError, ":indent must be a Fixnum.\n");
            }
            copts->indent = NUM2INT(v);
        }
        if (Qnil != (v = rb_hash_lookup(ropts, trace_sym))) {
            if (rb_cFixnum != rb_obj_class(v)) {
                rb_raise(rb_eArgError, ":trace must be a Fixnum.\n");
            }
            copts->trace = NUM2INT(v);
        }
        if (Qnil != (v = rb_hash_lookup(ropts, encoding_sym))) {
            if (rb_cString != rb_obj_class(v)) {
                rb_raise(rb_eArgError, ":encoding must be a String.\n");
            }
            strncpy(copts->encoding, StringValuePtr(v), sizeof(copts->encoding) - 1);
        }
        if (Qnil != (v = rb_hash_lookup(ropts, effort_sym))) {
            if (auto_define_sym == v) {
                copts->effort = AutoEffort;
            } else if (tolerant_sym == v) {
                copts->effort = TolerantEffort;
            } else if (strict_sym == v) {
                copts->effort = StrictEffort;
            } else {
                rb_raise(rb_eArgError, ":effort must be :strict, :tolerant, or :auto_define.\n");
            }
        }
        for (o = ynos; 0 != o->attr; o++) {
            if (Qnil != (v = rb_hash_lookup(ropts, o->sym))) {
                VALUE       c = rb_obj_class(v);

                if (rb_cTrueClass == c) {
                    *o->attr = Yes;
                } else if (rb_cFalseClass == c) {
                    *o->attr = No;
                } else {
                    rb_raise(rb_eArgError, "%s must be true or false.\n", StringValuePtr(o->sym));
                }
            }
        }
    }
 }

/* call-seq: dump(obj, options) => xml-string
 *
 * Dumps an Object (obj) to a string.
 * @param [Object] obj Object to serialize as an XML document String
 * @param [Hash] options formating options
 * @param [Fixnum] :indent format expected
 * @param [true|false] :xsd_date use XSD date format if true, default: false
 * @param [true|false] :circular allow circular references, default: false
 * @param [:strict|:tolerant] :effort effort to use when an undumpable object (e.g., IO) is encountered, default: :strict
 *  - *:strict* - raise an NotImplementedError if an undumpable object is encountered
 *  - *:tolerant* - replaces undumplable objects with nil
 */
static VALUE
dump(int argc, VALUE *argv, VALUE self) {
    char                *xml;
    struct _Options     copts = default_options;
    VALUE               rstr;
    
    if (2 == argc) {
        parse_dump_options(argv[1], &copts);
    }
    if (0 == (xml = write_obj_to_str(*argv, &copts))) {
        rb_raise(rb_eNoMemError, "Not enough memory.\n");
    }
    rstr = rb_str_new2(xml);
#ifdef ENCODING_INLINE_MAX
    if ('\0' != *copts.encoding) {
        rb_enc_associate(rstr, rb_enc_find(copts.encoding));
    }
#endif
    free(xml);

    return rstr;
}

/* call-seq: to_file(file_path, obj, options)
 *
 * Dumps an Object to the specified file.
 * @param [String] file_path file path to write the XML document to
 * @param [Object] obj Object to serialize as an XML document String
 * @param [Hash] options formating options
 * @param [Fixnum] :indent format expected
 * @param [true|false] :xsd_date use XSD date format if true, default: false
 * @param [true|false] :circular allow circular references, default: false
 * @param [:strict|:tolerant] :effort effort to use when an undumpable object (e.g., IO) is encountered, default: :strict
 *  - *:strict* - raise an NotImplementedError if an undumpable object is encountered
 *  - *:tolerant* - replaces undumplable objects with nil
 */
static VALUE
to_file(int argc, VALUE *argv, VALUE self) {
    struct _Options     copts = default_options;
    
    if (3 == argc) {
        parse_dump_options(argv[2], &copts);
    }
    Check_Type(*argv, T_STRING);
    write_obj_to_file(argv[1], StringValuePtr(*argv), &copts);

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
    VALUE       keep = Qnil;

    Ox = rb_define_module("Ox");
    keep = rb_cv_get(Ox, "@@keep"); // needed to stop GC from deleting and reusing VALUEs

    rb_define_module_function(Ox, "default_options", get_def_opts, 0);
    rb_define_module_function(Ox, "default_options=", set_def_opts, 1);

    rb_define_module_function(Ox, "parse_obj", to_obj, 1);
    rb_define_module_function(Ox, "parse", to_gen, 1);
    rb_define_module_function(Ox, "load", load_str, -1);
    rb_define_module_function(Ox, "sax_parse", sax_parse, 2);

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
    read_nonblock_id = rb_intern("read_nonblock");
    instruct_id = rb_intern("instruct");
    doctype_id = rb_intern("doctype");
    comment_id = rb_intern("comment");
    cdata_id = rb_intern("cdata");
    text_id = rb_intern("text");
    start_element_id = rb_intern("start_element");
    end_element_id = rb_intern("end_element");
    error_id = rb_intern("error");

    time_class = rb_const_get(rb_cObject, rb_intern("Time"));
    struct_class = rb_const_get(rb_cObject, rb_intern("Struct"));

    version_sym = ID2SYM(rb_intern("version"));                 rb_ary_push(keep, version_sym);
    standalone_sym = ID2SYM(rb_intern("standalone"));           rb_ary_push(keep, standalone_sym);
    encoding_sym = ID2SYM(rb_intern("encoding"));               rb_ary_push(keep, encoding_sym);
    indent_sym = ID2SYM(rb_intern("indent"));                   rb_ary_push(keep, indent_sym);
    xsd_date_sym = ID2SYM(rb_intern("xsd_date"));               rb_ary_push(keep, xsd_date_sym);
    opt_format_sym = ID2SYM(rb_intern("opt_format"));           rb_ary_push(keep, opt_format_sym);
    mode_sym = ID2SYM(rb_intern("mode"));                       rb_ary_push(keep, mode_sym);
    auto_sym = ID2SYM(rb_intern("auto"));                       rb_ary_push(keep, auto_sym);
    optimized_sym = ID2SYM(rb_intern("optimized"));             rb_ary_push(keep, optimized_sym);
    object_sym = ID2SYM(rb_intern("object"));                   rb_ary_push(keep, object_sym);
    circular_sym = ID2SYM(rb_intern("circular"));               rb_ary_push(keep, circular_sym);
    generic_sym = ID2SYM(rb_intern("generic"));                 rb_ary_push(keep, generic_sym);
    limited_sym = ID2SYM(rb_intern("limited"));                 rb_ary_push(keep, limited_sym);
    trace_sym = ID2SYM(rb_intern("trace"));                     rb_ary_push(keep, trace_sym);
    effort_sym = ID2SYM(rb_intern("effort"));                   rb_ary_push(keep, effort_sym);
    strict_sym = ID2SYM(rb_intern("strict"));                   rb_ary_push(keep, strict_sym);
    tolerant_sym = ID2SYM(rb_intern("tolerant"));               rb_ary_push(keep, tolerant_sym);
    auto_define_sym = ID2SYM(rb_intern("auto_define"));         rb_ary_push(keep, auto_define_sym);
    with_dtd_sym = ID2SYM(rb_intern("with_dtd"));               rb_ary_push(keep, with_dtd_sym);
    with_instruct_sym = ID2SYM(rb_intern("with_instructions")); rb_ary_push(keep, with_instruct_sym);
    with_xml_sym = ID2SYM(rb_intern("with_xml"));               rb_ary_push(keep, with_xml_sym);

    empty_string = rb_str_new2("");                             rb_ary_push(keep, empty_string);
    zero_fixnum = INT2NUM(0);                                   rb_ary_push(keep, zero_fixnum);

    
    //rb_require("node"); // generic xml node classes
    ox_document_clas = rb_const_get_at(Ox, rb_intern("Document"));
    ox_element_clas = rb_const_get_at(Ox, rb_intern("Element"));
    ox_comment_clas = rb_const_get_at(Ox, rb_intern("Comment"));
    ox_doctype_clas = rb_const_get_at(Ox, rb_intern("DocType"));
    ox_cdata_clas = rb_const_get_at(Ox, rb_intern("CData"));
    ox_bag_clas = rb_const_get_at(Ox, rb_intern("Bag"));

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
    rb_raise(rb_eSyntaxError, "%s at line %d, column %d [%s:%d]\n", msg, xline, col, file, line);
}
