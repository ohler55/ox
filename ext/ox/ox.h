/* ox.h
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

#ifndef __OX_H__
#define __OX_H__

#if defined(__cplusplus)
extern "C" {
#if 0
} /* satisfy cc-mode */
#endif
#endif

#include "cache.h"

#define raise_error(msg, xml, current) _raise_error(msg, xml, current, __FILE__, __LINE__)

#define MAX_TEXT_LEN    4096
#define MAX_ATTRS       1024
#define MAX_DEPTH       1024

#define SILENT          0
#define TRACE           1
#define DEBUG           2

typedef enum {
    UseObj      = 1,
    UseAttr     = 2,
    UseAttrSet  = 3,
    UseArray    = 4,
    UseAMember  = 5,
    UseHash     = 6,
    UseHashKey  = 7,
    UseHashVal  = 8,
    UseRange    = 9,
    UseRangeAttr= 10,
    UseRaw      = 11,
} Use;

typedef enum {
    NoCode         = 0,
    ArrayCode      = 'a',
    Base64Code     = 'b',
    ClassCode      = 'c',
    FloatCode      = 'f',
    RegexpCode     = 'g',
    HashCode       = 'h',
    FixnumCode     = 'i',
    BignumCode     = 'j',
    KeyCode        = 'k', // indicates the value is a hash key, kind of a hack
    RationalCode   = 'l',
    SymbolCode     = 'm',
    FalseClassCode = 'n',
    ObjectCode     = 'o',
    RangeCode      = 'r',
    StringCode     = 's',
    TimeCode       = 't',
    StructCode     = 'u',
    ComplexCode    = 'v',
    RawCode        = 'x',
    TrueClassCode  = 'y',
    NilClassCode   = 'z',
} Type;

typedef struct _Attr {
    const char  *name;
    const char  *value;
} *Attr;

typedef struct _Helper {
    ID          var;    /* Object var ID */
    VALUE       obj;    /* object created or Qundef if not appropriate */
    Type        type;   /* type of object in obj */
} *Helper;

typedef struct _PInfo   *PInfo;

typedef struct _ParseCallbacks {
    void        (*add_prolog)(PInfo pi, const char *version, const char *encoding, const char *standalone);
    void        (*add_doctype)(PInfo pi, const char *docType);
    void        (*add_comment)(PInfo pi, const char *comment);
    void        (*add_cdata)(PInfo pi, const char *cdata, size_t len);
    void        (*add_text)(PInfo pi, char *text, int closed);
    void        (*add_element)(PInfo pi, const char *ename, Attr attrs, int hasChildren);
    void        (*end_element)(PInfo pi, const char *ename);
} *ParseCallbacks;

/* parse information structure */
struct _PInfo {
    struct _Helper      helpers[MAX_DEPTH];
    Helper              h;              /* current helper or 0 if not set */
    char	        *str;		/* buffer being read from */
    char	        *s;		/* current position in buffer */
    VALUE               obj;
    ParseCallbacks      pcb;
    int                 trace;
};

extern VALUE    parse(char *xml, ParseCallbacks pcb, char **endp, int trace);
extern void     _raise_error(const char *msg, const char *xml, const char *current, const char* file, int line);

extern char*    write_obj_to_str(VALUE obj, int indent, int xsd_date);
extern void     write_obj_to_file(VALUE obj, const char *path, int indent, int xsd_date);

extern VALUE    Ox;

extern ID       at_id;
extern ID       attributes_id;
extern ID       beg_id;
extern ID       den_id;
extern ID       end_id;
extern ID       excl_id;
extern ID       inspect_id;
extern ID       keys_id;
extern ID       local_id;
extern ID       nodes_id;
extern ID       num_id;
extern ID       parse_id;
extern ID       to_c_id;
extern ID       to_s_id;
extern ID       tv_sec_id;
extern ID       tv_usec_id;
extern ID       value_id;

extern VALUE    empty_string;
extern VALUE    encoding_sym;
extern VALUE    standalone_sym;
extern VALUE    struct_class;
extern VALUE    time_class;
extern VALUE    version_sym;
extern VALUE    zero_fixnum;

extern VALUE    ox_document_clas;
extern VALUE    ox_element_clas;
extern VALUE    ox_text_clas;
extern VALUE    ox_comment_clas;
extern VALUE    ox_doctype_clas;
extern VALUE    ox_cdata_clas;

extern Cache    symbol_cache;
extern Cache    class_cache;
extern Cache    attr_cache;

#if defined(__cplusplus)
#if 0
{ /* satisfy cc-mode */
#endif
}  /* extern "C" { */
#endif
#endif /* __OX_H__ */
