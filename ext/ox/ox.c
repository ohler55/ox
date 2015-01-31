/* ox.c
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ruby.h"
#include "ox.h"
#include "sax.h"

/* maximum to allocate on the stack, arbitrary limit */
#define SMALL_XML	4096

typedef struct _YesNoOpt {
    VALUE	sym;
    char	*attr;
} *YesNoOpt;

void Init_ox();

VALUE	 Ox = Qnil;

ID	ox_at_column_id;
ID	ox_at_content_id;
ID	ox_at_id;
ID	ox_at_line_id;
ID	ox_at_value_id;
ID	ox_attr_id;
ID	ox_attr_value_id;
ID	ox_attributes_id;
ID	ox_attrs_done_id;
ID	ox_beg_id;
ID	ox_cdata_id;
ID	ox_comment_id;
ID	ox_den_id;
ID	ox_doctype_id;
ID	ox_end_element_id;
ID	ox_end_id;
ID	ox_end_instruct_id;
ID	ox_error_id;
ID	ox_excl_id;
ID	ox_external_encoding_id;
ID	ox_fileno_id;
ID	ox_force_encoding_id;
ID	ox_inspect_id;
ID	ox_instruct_id;
ID	ox_jd_id;
ID	ox_keys_id;
ID	ox_local_id;
ID	ox_mesg_id;
ID	ox_message_id;
ID	ox_new_id;
ID	ox_nodes_id;
ID	ox_num_id;
ID	ox_parse_id;
ID	ox_pos_id;
ID	ox_read_id;
ID	ox_readpartial_id;
ID	ox_start_element_id;
ID	ox_string_id;
ID	ox_text_id;
ID	ox_to_c_id;
ID	ox_to_s_id;
ID	ox_to_sym_id;
ID	ox_tv_nsec_id;
ID	ox_tv_sec_id;
ID	ox_tv_usec_id;
ID	ox_value_id;

VALUE	ox_encoding_sym;

VALUE	ox_empty_string;
VALUE	ox_zero_fixnum;
VALUE	ox_sym_bank; // Array

VALUE	ox_arg_error_class;
VALUE	ox_bag_clas;
VALUE	ox_bigdecimal_class;
VALUE	ox_cdata_clas;
VALUE	ox_comment_clas;
VALUE	ox_date_class;
VALUE	ox_doctype_clas;
VALUE	ox_document_clas;
VALUE	ox_element_clas;
VALUE	ox_instruct_clas;
VALUE	ox_parse_error_class;
VALUE	ox_stringio_class;
VALUE	ox_struct_class;
VALUE	ox_time_class;

Cache	ox_symbol_cache = 0;
Cache	ox_class_cache = 0;
Cache	ox_attr_cache = 0;

static VALUE	auto_define_sym;
static VALUE	auto_sym;
static VALUE	circular_sym;
static VALUE	convert_special_sym;
static VALUE	effort_sym;
static VALUE	generic_sym;
static VALUE	indent_sym;
static VALUE	limited_sym;
static VALUE	mode_sym;
static VALUE	object_sym;
static VALUE	opt_format_sym;
static VALUE	optimized_sym;
static VALUE	skip_none_sym;
static VALUE	skip_return_sym;
static VALUE	skip_sym;
static VALUE	skip_white_sym;
static VALUE	smart_sym;
static VALUE	strict_sym;
static VALUE	symbolize_keys_sym;
static VALUE	symbolize_sym;
static VALUE	tolerant_sym;
static VALUE	trace_sym;
static VALUE	with_dtd_sym;
static VALUE	with_instruct_sym;
static VALUE	with_xml_sym;
static VALUE	xsd_date_sym;

#if HAS_ENCODING_SUPPORT
rb_encoding	*ox_utf8_encoding = 0;
#elif HAS_PRIVATE_ENCODING
VALUE		ox_utf8_encoding = Qnil;
#else
void		*ox_utf8_encoding = 0;
#endif

struct _Options	 ox_default_options = {
    { '\0' },		/* encoding */
    2,			/* indent */
    0,			/* trace */
    No,			/* with_dtd */
    No,			/* with_xml */
    No,			/* with_instruct */
    No,			/* circular */
    No,			/* xsd_date */
    NoMode,		/* mode */
    StrictEffort,	/* effort */
    Yes,		/* sym_keys */
    NoSkip,		/* skip */
#if HAS_PRIVATE_ENCODING
    Qnil		/* rb_enc */
#else
    0			/* rb_enc */
#endif
};

extern ParseCallbacks	ox_obj_callbacks;
extern ParseCallbacks	ox_gen_callbacks;
extern ParseCallbacks	ox_limited_callbacks;
extern ParseCallbacks	ox_nomode_callbacks;

static void	parse_dump_options(VALUE ropts, Options copts);

static char*
defuse_bom(char *xml, Options options) {
    switch ((uint8_t)*xml) {
    case 0xEF: /* UTF-8 */
	if (0xBB == (uint8_t)xml[1] && 0xBF == (uint8_t)xml[2]) {
	    options->rb_enc = ox_utf8_encoding;
	    xml += 3;
	} else {
	    rb_raise(ox_parse_error_class, "Invalid BOM in XML string.\n");
	}
	break;
#if 0
    case 0xFE: /* UTF-16BE */
	if (0xFF == (uint8_t)xml[1]) {
	    options->rb_enc = ox_utf16be_encoding;
	    xml += 2;
	} else {
	    rb_raise(ox_parse_error_class, "Invalid BOM in XML string.\n");
	}
	break;
    case 0xFF: /* UTF-16LE or UTF-32LE */
	if (0xFE == (uint8_t)xml[1]) {
	    if (0x00 == (uint8_t)xml[2] && 0x00 == (uint8_t)xml[3]) {
		options->rb_enc = ox_utf32le_encoding;
		xml += 4;
	    } else {
		options->rb_enc = ox_utf16le_encoding;
		xml += 2;
	    }
	} else {
	    rb_raise(ox_parse_error_class, "Invalid BOM in XML string.\n");
	}
	break;
    case 0x00: /* UTF-32BE */
	if (0x00 == (uint8_t)xml[1] && 0xFE == (uint8_t)xml[2] && 0xFF == (uint8_t)xml[3]) {
	    options->rb_enc = ox_utf32be_encoding;
	    xml += 4;
	} else {
	    rb_raise(ox_parse_error_class, "Invalid BOM in XML string.\n");
	}
	break;
#endif
    default:
	/* Let it fail if there is a BOM that is not UTF-8. Other BOM options are not ASCII compatible. */
	break;
    }
    return xml;
}

/* call-seq: ox_default_options() => Hash
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
 * - symbolize_keys: [true|false|nil] symbolize element attribute keys or leave as Strings
 * - skip: [:skip_none|:skip_return|:skip_white] determines how to handle white space in text
 * @return [Hash] all current option settings.
 *
 * Note that an indent of less than zero will result in a tight one line output
 * unless the text in the XML fields contain new line characters.
 */
static VALUE
get_def_opts(VALUE self) {
    VALUE	opts = rb_hash_new();
    int		elen = (int)strlen(ox_default_options.encoding);

    rb_hash_aset(opts, ox_encoding_sym, (0 == elen) ? Qnil : rb_str_new(ox_default_options.encoding, elen));
    rb_hash_aset(opts, indent_sym, INT2FIX(ox_default_options.indent));
    rb_hash_aset(opts, trace_sym, INT2FIX(ox_default_options.trace));
    rb_hash_aset(opts, with_dtd_sym, (Yes == ox_default_options.with_dtd) ? Qtrue : ((No == ox_default_options.with_dtd) ? Qfalse : Qnil));
    rb_hash_aset(opts, with_xml_sym, (Yes == ox_default_options.with_xml) ? Qtrue : ((No == ox_default_options.with_xml) ? Qfalse : Qnil));
    rb_hash_aset(opts, with_instruct_sym, (Yes == ox_default_options.with_instruct) ? Qtrue : ((No == ox_default_options.with_instruct) ? Qfalse : Qnil));
    rb_hash_aset(opts, circular_sym, (Yes == ox_default_options.circular) ? Qtrue : ((No == ox_default_options.circular) ? Qfalse : Qnil));
    rb_hash_aset(opts, xsd_date_sym, (Yes == ox_default_options.xsd_date) ? Qtrue : ((No == ox_default_options.xsd_date) ? Qfalse : Qnil));
    rb_hash_aset(opts, symbolize_keys_sym, (Yes == ox_default_options.sym_keys) ? Qtrue : ((No == ox_default_options.sym_keys) ? Qfalse : Qnil));
    switch (ox_default_options.mode) {
    case ObjMode:	rb_hash_aset(opts, mode_sym, object_sym);	break;
    case GenMode:	rb_hash_aset(opts, mode_sym, generic_sym);	break;
    case LimMode:	rb_hash_aset(opts, mode_sym, limited_sym);	break;
    case NoMode:
    default:		rb_hash_aset(opts, mode_sym, Qnil);		break;
    }
    switch (ox_default_options.effort) {
    case StrictEffort:		rb_hash_aset(opts, effort_sym, strict_sym);		break;
    case TolerantEffort:	rb_hash_aset(opts, effort_sym, tolerant_sym);		break;
    case AutoEffort:		rb_hash_aset(opts, effort_sym, auto_define_sym);	break;
    case NoEffort:
    default:			rb_hash_aset(opts, effort_sym, Qnil);			break;
    }
    switch (ox_default_options.skip) {
    case NoSkip:		rb_hash_aset(opts, skip_sym, skip_none_sym);		break;
    case CrSkip:		rb_hash_aset(opts, skip_sym, skip_return_sym);		break;
    case SpcSkip:		rb_hash_aset(opts, skip_sym, skip_white_sym);		break;
    default:			rb_hash_aset(opts, skip_sym, Qnil);			break;
    }
    return opts;
}

/* call-seq: ox_default_options=(opts)
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
 * @param [true|false|nil] :symbolize_keys symbolize element attribute keys or leave as Strings
 * @param [:skip_none|:skip_return|:skip_white] determines how to handle white space in text
 * @return [nil]
 */
static VALUE
set_def_opts(VALUE self, VALUE opts) {
    struct _YesNoOpt	ynos[] = {
	{ with_xml_sym, &ox_default_options.with_xml },
	{ with_dtd_sym, &ox_default_options.with_dtd },
	{ with_instruct_sym, &ox_default_options.with_instruct },
	{ xsd_date_sym, &ox_default_options.xsd_date },
	{ circular_sym, &ox_default_options.circular },
	{ symbolize_keys_sym, &ox_default_options.sym_keys },
	{ Qnil, 0 }
    };
    YesNoOpt	o;
    VALUE	v;
    
    Check_Type(opts, T_HASH);

    v = rb_hash_aref(opts, ox_encoding_sym);
    if (Qnil == v) {
	*ox_default_options.encoding = '\0';
    } else {
	Check_Type(v, T_STRING);
	strncpy(ox_default_options.encoding, StringValuePtr(v), sizeof(ox_default_options.encoding) - 1);
#if HAS_ENCODING_SUPPORT
	ox_default_options.rb_enc = rb_enc_find(ox_default_options.encoding);
#elif HAS_PRIVATE_ENCODING
	ox_default_options.rb_enc = rb_str_new2(ox_default_options.encoding);
	rb_gc_register_address(&ox_default_options.rb_enc);
#endif
    }

    v = rb_hash_aref(opts, indent_sym);
    if (Qnil != v) {
	Check_Type(v, T_FIXNUM);
	ox_default_options.indent = FIX2INT(v);
    }

    v = rb_hash_aref(opts, trace_sym);
    if (Qnil != v) {
	Check_Type(v, T_FIXNUM);
	ox_default_options.trace = FIX2INT(v);
    }

    v = rb_hash_aref(opts, mode_sym);
    if (Qnil == v) {
	ox_default_options.mode = NoMode;
    } else if (object_sym == v) {
	ox_default_options.mode = ObjMode;
    } else if (generic_sym == v) {
	ox_default_options.mode = GenMode;
    } else if (limited_sym == v) {
	ox_default_options.mode = LimMode;
    } else {
	rb_raise(ox_parse_error_class, ":mode must be :object, :generic, :limited, or nil.\n");
    }

    v = rb_hash_aref(opts, effort_sym);
    if (Qnil == v) {
	ox_default_options.effort = NoEffort;
    } else if (strict_sym == v) {
	ox_default_options.effort = StrictEffort;
    } else if (tolerant_sym == v) {
	ox_default_options.effort = TolerantEffort;
    } else if (auto_define_sym == v) {
	ox_default_options.effort = AutoEffort;
    } else {
	rb_raise(ox_parse_error_class, ":effort must be :strict, :tolerant, :auto_define, or nil.\n");
    }

    v = rb_hash_aref(opts, skip_sym);
    if (Qnil == v) {
	ox_default_options.skip = NoSkip;
    } else if (skip_none_sym == v) {
	ox_default_options.skip = NoSkip;
    } else if (skip_return_sym == v) {
	ox_default_options.skip = CrSkip;
    } else if (skip_white_sym == v) {
	ox_default_options.skip = SpcSkip;
    } else {
	rb_raise(ox_parse_error_class, ":skip must be :skip_none, :skip_return, :skip_white, or nil.\n");
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
	    rb_raise(ox_parse_error_class, "%s must be true or false.\n", rb_id2name(SYM2ID(o->sym)));
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
    char		*xml, *x;
    size_t		len;
    VALUE		obj;
    struct _Options	options = ox_default_options;
    struct _Err		err;

    err_init(&err);
    Check_Type(ruby_xml, T_STRING);
    /* the xml string gets modified so make a copy of it */
    len = RSTRING_LEN(ruby_xml) + 1;
    x = defuse_bom(StringValuePtr(ruby_xml), &options);
    if (SMALL_XML < len) {
	xml = ALLOC_N(char, len);
    } else {
	xml = ALLOCA_N(char, len);
    }
    memcpy(xml, x, len);
#if HAS_GC_GUARD
    rb_gc_disable();
#endif
    obj = ox_parse(xml, ox_obj_callbacks, 0, &options, &err);
    if (SMALL_XML < len) {
	xfree(xml);
    }
#if HAS_GC_GUARD
    RB_GC_GUARD(obj);
    rb_gc_enable();
#endif
    if (err_has(&err)) {
	ox_err_raise(&err);
    }
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
    char		*xml, *x;
    size_t		len;
    VALUE		obj;
    struct _Options	options = ox_default_options;
    struct _Err		err;

    err_init(&err);
    Check_Type(ruby_xml, T_STRING);
    /* the xml string gets modified so make a copy of it */
    len = RSTRING_LEN(ruby_xml) + 1;
    x = defuse_bom(StringValuePtr(ruby_xml), &options);
    if (SMALL_XML < len) {
	xml = ALLOC_N(char, len);
    } else {
	xml = ALLOCA_N(char, len);
    }
    memcpy(xml, x, len);
    obj = ox_parse(xml, ox_gen_callbacks, 0, &options, &err);
    if (SMALL_XML < len) {
	xfree(xml);
    }
    if (err_has(&err)) {
	ox_err_raise(&err);
    }
    return obj;
}

static VALUE
load(char *xml, int argc, VALUE *argv, VALUE self, VALUE encoding, Err err) {
    VALUE		obj;
    struct _Options	options = ox_default_options;

    if (1 == argc && rb_cHash == rb_obj_class(*argv)) {
	VALUE	h = *argv;
	VALUE	v;
	
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
		rb_raise(ox_parse_error_class, ":mode must be :generic, :object, or :limited.\n");
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
		rb_raise(ox_parse_error_class, ":effort must be :strict, :tolerant, or :auto_define.\n");
	    }
	}
	if (Qnil != (v = rb_hash_lookup(h, skip_sym))) {
	    if (skip_none_sym == v) {
		options.skip = NoSkip;
	    } else if (skip_return_sym == v) {
		options.skip = CrSkip;
	    } else if (skip_white_sym == v) {
		options.skip = SpcSkip;
	    } else {
		rb_raise(ox_parse_error_class, ":effort must be :skip_none, :skip_return, or :skip_white.\n");
	    }
	}

	if (Qnil != (v = rb_hash_lookup(h, trace_sym))) {
	    Check_Type(v, T_FIXNUM);
	    options.trace = FIX2INT(v);
	}
	if (Qnil != (v = rb_hash_lookup(h, symbolize_keys_sym))) {
	    options.sym_keys = (Qfalse == v) ? No : Yes;
	}
    }
#if HAS_ENCODING_SUPPORT
    if ('\0' == *options.encoding) {
	if (Qnil != encoding) {
	    options.rb_enc = rb_enc_from_index(rb_enc_get_index(encoding));
	} else {
	    options.rb_enc = 0;
	}
    } else if (0 == options.rb_enc) {
	options.rb_enc = rb_enc_find(options.encoding);
    }
#elif HAS_PRIVATE_ENCODING
    if ('\0' == *options.encoding) {
	if (Qnil != encoding) {
	    options.rb_enc = encoding;
	} else {
	    options.rb_enc = Qnil;
	}
    } else if (0 == options.rb_enc) {
	options.rb_enc = rb_str_new2(options.encoding);
	rb_gc_register_address(&options.rb_enc);
    }
#endif
    xml = defuse_bom(xml, &options);
    switch (options.mode) {
    case ObjMode:
#if HAS_GC_GUARD
	rb_gc_disable();
#endif
	obj = ox_parse(xml, ox_obj_callbacks, 0, &options, err);
#if HAS_GC_GUARD
	RB_GC_GUARD(obj);
	rb_gc_enable();
#endif
	break;
    case GenMode:
	obj = ox_parse(xml, ox_gen_callbacks, 0, &options, err);
	break;
    case LimMode:
	obj = ox_parse(xml, ox_limited_callbacks, 0, &options, err);
	break;
    case NoMode:
	obj = ox_parse(xml, ox_nomode_callbacks, 0, &options, err);
	break;
    default:
	obj = ox_parse(xml, ox_gen_callbacks, 0, &options, err);
	break;
    }
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
 * @param [true|false|nil] :symbolize_keys symbolize element attribute keys or leave as Strings
 */
static VALUE
load_str(int argc, VALUE *argv, VALUE self) {
    char	*xml;
    size_t	len;
    VALUE	obj;
    VALUE	encoding;
    struct _Err	err;

    err_init(&err);
    Check_Type(*argv, T_STRING);
    /* the xml string gets modified so make a copy of it */
    len = RSTRING_LEN(*argv) + 1;
    if (SMALL_XML < len) {
	xml = ALLOC_N(char, len);
    } else {
	xml = ALLOCA_N(char, len);
    }
#if HAS_ENCODING_SUPPORT
#ifdef MACRUBY_RUBY
    encoding = rb_funcall(*argv, rb_intern("encoding"), 0);
#else
    encoding = rb_obj_encoding(*argv);
#endif
#elif HAS_PRIVATE_ENCODING
    encoding = rb_funcall(*argv, rb_intern("encoding"), 0);
#else
    encoding = Qnil;
#endif
    memcpy(xml, StringValuePtr(*argv), len);
    obj = load(xml, argc - 1, argv + 1, self, encoding, &err);
    if (SMALL_XML < len) {
	xfree(xml);
    }
    if (err_has(&err)) {
	ox_err_raise(&err);
    }
    return obj;
}

/* call-seq: load_file(file_path, options) => Ox::Document or Ox::Element or Object
 *
 * Parses and XML document from a file into an Ox::Document, or Ox::Element,
 * or Object depending on the options.	Raises an exception if the XML is
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
 * @param [true|false|nil] :symbolize_keys symbolize element attribute keys or leave as Strings
 */
static VALUE
load_file(int argc, VALUE *argv, VALUE self) {
    char	*path;
    char	*xml;
    FILE	*f;
    size_t	len;
    VALUE	obj;
    struct _Err	err;

    err_init(&err);
    Check_Type(*argv, T_STRING);
    path = StringValuePtr(*argv);
    if (0 == (f = fopen(path, "r"))) {
	rb_raise(rb_eIOError, "%s\n", strerror(errno));
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (SMALL_XML < len) {
	xml = ALLOC_N(char, len + 1);
    } else {
	xml = ALLOCA_N(char, len + 1);
    }
    fseek(f, 0, SEEK_SET);
    if (len != fread(xml, 1, len, f)) {
	ox_err_set(&err, rb_eLoadError, "Failed to read %ld bytes from %s.\n", (long)len, path);
	obj = Qnil;
    } else {
	xml[len] = '\0';
	obj = load(xml, argc - 1, argv + 1, self, Qnil, &err);
    }
    fclose(f);
    if (SMALL_XML < len) {
	xfree(xml);
    }
    if (err_has(&err)) {
	ox_err_raise(&err);
    }
    return obj;
}

/* call-seq: sax_parse(handler, io, options)
 *
 * Parses an IO stream or file containing an XML document. Raises an exception
 * if the XML is malformed or the classes specified are not valid.
 * @param [Ox::Sax] handler SAX (responds to OX::Sax methods) like handler
 * @param [IO|String] io IO Object to read from
 * @param [Hash] options parse options
 * @param [true|false] :convert_special flag indicating special characters like &lt; are converted
 * @param [true|false] :symbolize flag indicating the parser symbolize element and attribute names
 * @param [true|false] :smart flag indicating the parser uses hints if available (use with html)
 * @param [:skip_return|:skip_white] :skip flag indicating the parser skips \r or collpase white space into a single space. Default (skip nothing)
 */
static VALUE
sax_parse(int argc, VALUE *argv, VALUE self) {
    struct _SaxOptions	options;

    options.symbolize = 1;
    options.convert_special = 0;
    options.smart = 0;
    options.skip = NoSkip;

    if (argc < 2) {
	rb_raise(ox_parse_error_class, "Wrong number of arguments to sax_parse.\n");
    }
    if (3 <= argc && rb_cHash == rb_obj_class(argv[2])) {
	VALUE	h = argv[2];
	VALUE	v;
	
	if (Qnil != (v = rb_hash_lookup(h, convert_special_sym))) {
	    options.convert_special = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(h, smart_sym))) {
	    options.smart = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(h, symbolize_sym))) {
	    options.symbolize = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(h, skip_sym))) {
	    if (skip_return_sym == v) {
		options.skip = CrSkip;
	    } else if (skip_white_sym == v) {
		options.skip = SpcSkip;
	    }
	}
    }
    ox_sax_parse(argv[0], argv[1], &options);

    return Qnil;
}

static void
parse_dump_options(VALUE ropts, Options copts) {
    struct _YesNoOpt	ynos[] = {
	{ with_xml_sym, &copts->with_xml },
	{ with_dtd_sym, &copts->with_dtd },
	{ with_instruct_sym, &copts->with_instruct },
	{ xsd_date_sym, &copts->xsd_date },
	{ circular_sym, &copts->circular },
	{ Qnil, 0 }
    };
    YesNoOpt	o;
    
    if (rb_cHash == rb_obj_class(ropts)) {
	VALUE	v;
	
	if (Qnil != (v = rb_hash_lookup(ropts, indent_sym))) {
	    if (rb_cFixnum != rb_obj_class(v)) {
		rb_raise(ox_parse_error_class, ":indent must be a Fixnum.\n");
	    }
	    copts->indent = NUM2INT(v);
	}
	if (Qnil != (v = rb_hash_lookup(ropts, trace_sym))) {
	    if (rb_cFixnum != rb_obj_class(v)) {
		rb_raise(ox_parse_error_class, ":trace must be a Fixnum.\n");
	    }
	    copts->trace = NUM2INT(v);
	}
	if (Qnil != (v = rb_hash_lookup(ropts, ox_encoding_sym))) {
	    if (rb_cString != rb_obj_class(v)) {
		rb_raise(ox_parse_error_class, ":encoding must be a String.\n");
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
		rb_raise(ox_parse_error_class, ":effort must be :strict, :tolerant, or :auto_define.\n");
	    }
	}
	for (o = ynos; 0 != o->attr; o++) {
	    if (Qnil != (v = rb_hash_lookup(ropts, o->sym))) {
		VALUE	    c = rb_obj_class(v);

		if (rb_cTrueClass == c) {
		    *o->attr = Yes;
		} else if (rb_cFalseClass == c) {
		    *o->attr = No;
		} else {
		    rb_raise(ox_parse_error_class, "%s must be true or false.\n", rb_id2name(SYM2ID(o->sym)));
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
 *
 * Note that an indent of less than zero will result in a tight one line output
 * unless the text in the XML fields contain new line characters.
 */
static VALUE
dump(int argc, VALUE *argv, VALUE self) {
    char		*xml;
    struct _Options	copts = ox_default_options;
    VALUE		rstr;
    
    if (2 == argc) {
	parse_dump_options(argv[1], &copts);
    }
    if (0 == (xml = ox_write_obj_to_str(*argv, &copts))) {
	rb_raise(rb_eNoMemError, "Not enough memory.\n");
    }
    rstr = rb_str_new2(xml);
#if HAS_ENCODING_SUPPORT
    if ('\0' != *copts.encoding) {
	rb_enc_associate(rstr, rb_enc_find(copts.encoding));
    }
#elif HAS_PRIVATE_ENCODING
    if ('\0' != *copts.encoding) {
	rb_funcall(rstr, ox_force_encoding_id, 1, rb_str_new2(copts.encoding));
    }
#endif
    xfree(xml);

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
 *
 * Note that an indent of less than zero will result in a tight one line output
 * unless the text in the XML fields contain new line characters.
 */
static VALUE
to_file(int argc, VALUE *argv, VALUE self) {
    struct _Options	copts = ox_default_options;
    
    if (3 == argc) {
	parse_dump_options(argv[2], &copts);
    }
    Check_Type(*argv, T_STRING);
    ox_write_obj_to_file(argv[1], StringValuePtr(*argv), &copts);

    return Qnil;
}

extern void	ox_cache_test(void);

static VALUE
cache_test(VALUE self) {
    ox_cache_test();
    return Qnil;
}

extern void	ox_cache8_test(void);

static VALUE
cache8_test(VALUE self) {
    ox_cache8_test();
    return Qnil;
}

void Init_ox() {
    Ox = rb_define_module("Ox");

    rb_define_module_function(Ox, "default_options", get_def_opts, 0);
    rb_define_module_function(Ox, "default_options=", set_def_opts, 1);

    rb_define_module_function(Ox, "parse_obj", to_obj, 1);
    rb_define_module_function(Ox, "parse", to_gen, 1);
    rb_define_module_function(Ox, "load", load_str, -1);
    rb_define_module_function(Ox, "sax_parse", sax_parse, -1);

    rb_define_module_function(Ox, "to_xml", dump, -1);
    rb_define_module_function(Ox, "dump", dump, -1);

    rb_define_module_function(Ox, "load_file", load_file, -1);
    rb_define_module_function(Ox, "to_file", to_file, -1);

    rb_require("time");
    rb_require("date");
    rb_require("bigdecimal");
    rb_require("stringio");

    ox_at_column_id = rb_intern("@column");
    ox_at_content_id = rb_intern("@content");
    ox_at_id = rb_intern("at");
    ox_at_line_id = rb_intern("@line");
    ox_at_value_id = rb_intern("@value");
    ox_attr_id = rb_intern("attr");
    ox_attr_value_id = rb_intern("attr_value");
    ox_attributes_id = rb_intern("@attributes");
    ox_attrs_done_id = rb_intern("attrs_done");
    ox_beg_id = rb_intern("@beg");
    ox_cdata_id = rb_intern("cdata");
    ox_comment_id = rb_intern("comment");
    ox_den_id = rb_intern("@den");
    ox_doctype_id = rb_intern("doctype");
    ox_end_element_id = rb_intern("end_element");
    ox_end_id = rb_intern("@end");
    ox_end_instruct_id = rb_intern("end_instruct");
    ox_error_id = rb_intern("error");
    ox_excl_id = rb_intern("@excl");
    ox_external_encoding_id = rb_intern("external_encoding");
    ox_fileno_id = rb_intern("fileno");
    ox_force_encoding_id = rb_intern("force_encoding");
    ox_inspect_id = rb_intern("inspect");
    ox_instruct_id = rb_intern("instruct");
    ox_jd_id = rb_intern("jd");
    ox_keys_id = rb_intern("keys");
    ox_local_id = rb_intern("local");
    ox_mesg_id = rb_intern("mesg");
    ox_message_id = rb_intern("message");
    ox_nodes_id = rb_intern("@nodes");
    ox_new_id = rb_intern("new");
    ox_num_id = rb_intern("@num");
    ox_parse_id = rb_intern("parse");
    ox_pos_id = rb_intern("pos");
    ox_read_id = rb_intern("read");
    ox_readpartial_id = rb_intern("readpartial");
    ox_start_element_id = rb_intern("start_element");
    ox_string_id = rb_intern("string");
    ox_text_id = rb_intern("text");
    ox_to_c_id = rb_intern("to_c");
    ox_to_s_id = rb_intern("to_s");
    ox_to_sym_id = rb_intern("to_sym");
    ox_tv_nsec_id = rb_intern("tv_nsec");
    ox_tv_sec_id = rb_intern("tv_sec");
    ox_tv_usec_id = rb_intern("tv_usec");
    ox_value_id = rb_intern("value");

    rb_require("ox/version");
    rb_require("ox/error");
    rb_require("ox/hasattrs");
    rb_require("ox/node");
    rb_require("ox/comment");
    rb_require("ox/instruct");
    rb_require("ox/cdata");
    rb_require("ox/doctype");
    rb_require("ox/element");
    rb_require("ox/document");
    rb_require("ox/bag");
    rb_require("ox/sax");

    ox_time_class = rb_const_get(rb_cObject, rb_intern("Time"));
    ox_date_class = rb_const_get(rb_cObject, rb_intern("Date"));
    ox_parse_error_class = rb_const_get_at(Ox, rb_intern("ParseError"));
    ox_arg_error_class = rb_const_get_at(Ox, rb_intern("ArgError"));
    ox_struct_class = rb_const_get(rb_cObject, rb_intern("Struct"));
    ox_stringio_class = rb_const_get(rb_cObject, rb_intern("StringIO"));
    ox_bigdecimal_class = rb_const_get(rb_cObject, rb_intern("BigDecimal"));

    auto_define_sym = ID2SYM(rb_intern("auto_define"));		rb_gc_register_address(&auto_define_sym);
    auto_sym = ID2SYM(rb_intern("auto"));			rb_gc_register_address(&auto_sym);
    circular_sym = ID2SYM(rb_intern("circular"));		rb_gc_register_address(&circular_sym);
    convert_special_sym = ID2SYM(rb_intern("convert_special")); rb_gc_register_address(&convert_special_sym);
    effort_sym = ID2SYM(rb_intern("effort"));			rb_gc_register_address(&effort_sym);
    generic_sym = ID2SYM(rb_intern("generic"));			rb_gc_register_address(&generic_sym);
    indent_sym = ID2SYM(rb_intern("indent"));			rb_gc_register_address(&indent_sym);
    limited_sym = ID2SYM(rb_intern("limited"));			rb_gc_register_address(&limited_sym);
    mode_sym = ID2SYM(rb_intern("mode"));			rb_gc_register_address(&mode_sym);
    object_sym = ID2SYM(rb_intern("object"));			rb_gc_register_address(&object_sym);
    opt_format_sym = ID2SYM(rb_intern("opt_format"));		rb_gc_register_address(&opt_format_sym);
    optimized_sym = ID2SYM(rb_intern("optimized"));		rb_gc_register_address(&optimized_sym);
    ox_encoding_sym = ID2SYM(rb_intern("encoding"));		rb_gc_register_address(&ox_encoding_sym);
    skip_none_sym = ID2SYM(rb_intern("skip_none"));		rb_gc_register_address(&skip_none_sym);
    skip_return_sym = ID2SYM(rb_intern("skip_return"));		rb_gc_register_address(&skip_return_sym);
    skip_sym = ID2SYM(rb_intern("skip"));			rb_gc_register_address(&skip_sym);
    skip_white_sym = ID2SYM(rb_intern("skip_white"));		rb_gc_register_address(&skip_white_sym);
    smart_sym = ID2SYM(rb_intern("smart"));			rb_gc_register_address(&smart_sym);
    strict_sym = ID2SYM(rb_intern("strict"));			rb_gc_register_address(&strict_sym);
    symbolize_keys_sym = ID2SYM(rb_intern("symbolize_keys"));	rb_gc_register_address(&symbolize_keys_sym);
    symbolize_sym = ID2SYM(rb_intern("symbolize"));		rb_gc_register_address(&symbolize_sym);
    tolerant_sym = ID2SYM(rb_intern("tolerant"));		rb_gc_register_address(&tolerant_sym);
    trace_sym = ID2SYM(rb_intern("trace"));			rb_gc_register_address(&trace_sym);
    with_dtd_sym = ID2SYM(rb_intern("with_dtd"));		rb_gc_register_address(&with_dtd_sym);
    with_instruct_sym = ID2SYM(rb_intern("with_instructions")); rb_gc_register_address(&with_instruct_sym);
    with_xml_sym = ID2SYM(rb_intern("with_xml"));		rb_gc_register_address(&with_xml_sym);
    xsd_date_sym = ID2SYM(rb_intern("xsd_date"));		rb_gc_register_address(&xsd_date_sym);

    ox_empty_string = rb_str_new2("");				rb_gc_register_address(&ox_empty_string);
    ox_zero_fixnum = INT2NUM(0);				rb_gc_register_address(&ox_zero_fixnum);
    ox_sym_bank = rb_ary_new();					rb_gc_register_address(&ox_sym_bank);

    ox_document_clas = rb_const_get_at(Ox, rb_intern("Document"));
    ox_element_clas = rb_const_get_at(Ox, rb_intern("Element"));
    ox_instruct_clas = rb_const_get_at(Ox, rb_intern("Instruct"));
    ox_comment_clas = rb_const_get_at(Ox, rb_intern("Comment"));
    ox_doctype_clas = rb_const_get_at(Ox, rb_intern("DocType"));
    ox_cdata_clas = rb_const_get_at(Ox, rb_intern("CData"));
    ox_bag_clas = rb_const_get_at(Ox, rb_intern("Bag"));

    ox_cache_new(&ox_symbol_cache);
    ox_cache_new(&ox_class_cache);
    ox_cache_new(&ox_attr_cache);

    ox_sax_define();

    rb_define_module_function(Ox, "cache_test", cache_test, 0);
    rb_define_module_function(Ox, "cache8_test", cache8_test, 0);
#if HAS_ENCODING_SUPPORT
    ox_utf8_encoding = rb_enc_find("UTF-8");
#elif HAS_PRIVATE_ENCODING
    ox_utf8_encoding = rb_str_new2("UTF-8");			rb_gc_register_address(&ox_utf8_encoding);
#endif
}

void
_ox_raise_error(const char *msg, const char *xml, const char *current, const char* file, int line) {
    int	xline = 1;
    int	col = 1;

    for (; xml < current && '\n' != *current; current--) {
	col++;
    }
    for (; xml < current; current--) {
	if ('\n' == *current) {
	    xline++;
	}
    }
#if HAS_GC_GUARD
    rb_gc_enable();
#endif
    rb_raise(ox_parse_error_class, "%s at line %d, column %d [%s:%d]\n", msg, xline, col, file, line);
}
