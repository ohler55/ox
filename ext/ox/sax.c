/* sax.c
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

#include "ruby.h"
#include "ox.h"

// 65536 = 0x00010000
#define BUF_SIZE = 0x00010000

typedef struct _SaxDrive {
    // TBD reader function
    void        *read_func;
    union {
        FILE    *fp;
        VALUE   io;
    };
    int         has_instruct;
    int         has_doctype;
    int         has_comment;
    int         has_cdata;
    int         has_text;
    int         has_start_element;
    int         has_end_element;
    int         has_error;
} *SaxDrive;

static void     ox_sax_drive_init(SaxDrive dr, VALUE handler, VALUE io);
static size_t   read_from_io(SaxDrive dr, char *buf, size_t max_len);
static size_t   read_from_file(SaxDrive dr, char *buf, size_t max_len);

void
ox_sax_parse(VALUE handler, VALUE io) {
    struct _SaxDrive       dr;

    ox_sax_drive_init(&dr, handler, io);
    
    printf("*** sax_parse with these flags\n");
    printf("    has_instruct = %s\n", dr.has_instruct ? "true" : "false");
    printf("    has_doctype = %s\n", dr.has_doctype ? "true" : "false");
    printf("    has_comment = %s\n", dr.has_comment ? "true" : "false");
    printf("    has_cdata = %s\n", dr.has_cdata ? "true" : "false");
    printf("    has_text = %s\n", dr.has_text ? "true" : "false");
    printf("    has_start_element = %s\n", dr.has_start_element ? "true" : "false");
    printf("    has_end_element = %s\n", dr.has_end_element ? "true" : "false");
    printf("    has_error = %s\n", dr.has_error ? "true" : "false");
}

static void
ox_sax_drive_init(SaxDrive dr, VALUE handler, VALUE io) {
    if (T_STRING == rb_type(io)) {
        dr->read_func = read_from_file;
        dr->fp = fopen(StringValuePtr(io), "r");
    } else if (rb_respond_to(io, read_nonblock_id)) {
        dr->read_func = read_from_io;
        dr->io = io;
    } else {
        rb_raise(rb_eArgError, "sax_parser io argument must respond to read_nonblock().\n");
    }
    dr->has_instruct = rb_respond_to(handler, instruct_id);
    dr->has_doctype = rb_respond_to(handler, doctype_id);
    dr->has_comment = rb_respond_to(handler, comment_id);
    dr->has_cdata = rb_respond_to(handler, cdata_id);
    dr->has_text = rb_respond_to(handler, text_id);
    dr->has_start_element = rb_respond_to(handler, start_element_id);
    dr->has_end_element = rb_respond_to(handler, end_element_id);
    dr->has_error = rb_respond_to(handler, error_id);
}

static size_t
read_from_io(SaxDrive dr, char *buf, size_t max_len) {
    // TBD
    return 0;
}

static size_t
read_from_file(SaxDrive dr, char *buf, size_t max_len) {
    // TBD
    return 0;
}
