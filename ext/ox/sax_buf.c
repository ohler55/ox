/* sax_buf.c
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
#include <strings.h>
#include <sys/types.h>
#if NEEDS_UIO
#include <sys/uio.h>    
#endif
#include <unistd.h>
#include <time.h>

#include "ruby.h"
#include "ox.h"
#include "sax.h"

#define BUF_PAD	4

static VALUE		rescue_cb(VALUE rdr, VALUE err);
static VALUE		io_cb(VALUE rdr);
static VALUE		partial_io_cb(VALUE rdr);
static int		read_from_io(Buf buf);
#ifndef JRUBY_RUBY
static int		read_from_fd(Buf buf);
#endif
static int		read_from_io_partial(Buf buf);
static int		read_from_str(Buf buf);

void
ox_sax_buf_init(Buf buf, VALUE io) {
    if (ox_stringio_class == rb_obj_class(io)) {
	VALUE	s = rb_funcall2(io, ox_string_id, 0, 0);

	buf->read_func = read_from_str;
	buf->in_str = StringValuePtr(s);
    } else if (rb_respond_to(io, ox_readpartial_id)) {
#ifdef JRUBY_RUBY
	buf->read_func = read_from_io_partial;
	buf->io = io;
#else
        VALUE   rfd;

        if (rb_respond_to(io, ox_fileno_id) && Qnil != (rfd = rb_funcall(io, ox_fileno_id, 0))) {
            buf->read_func = read_from_fd;
            buf->fd = FIX2INT(rfd);
        } else {
            buf->read_func = read_from_io_partial;
            buf->io = io;
        }
#endif
    } else if (rb_respond_to(io, ox_read_id)) {
#ifdef JRUBY_RUBY
	buf->read_func = read_from_io;
	buf->io = io;
#else
        VALUE   rfd;

        if (rb_respond_to(io, ox_fileno_id) && Qnil != (rfd = rb_funcall(io, ox_fileno_id, 0))) {
            buf->read_func = read_from_fd;
            buf->fd = FIX2INT(rfd);
        } else {
            buf->read_func = read_from_io;
            buf->io = io;
        }
#endif
    } else {
        rb_raise(ox_arg_error_class, "sax_parser io argument must respond to readpartial() or read().\n");
    }
    buf->head = buf->base;
    *buf->head = '\0';
    buf->end = buf->head + sizeof(buf->base) - BUF_PAD;
    buf->tail = buf->head;
    buf->read_end = buf->head;
    buf->pro = 0;
    buf->str = 0;
    buf->line = 1;
    buf->col = 0;
    buf->pro_line = 1;
    buf->pro_col = 0;
    buf->dr = 0;
}

int
ox_sax_buf_read(Buf buf) {
    int         err;
    size_t      shift = 0;
    
    // if there is not much room to read into, shift or realloc a larger buffer.
    if (buf->head < buf->tail && 4096 > buf->end - buf->tail) {
        if (0 == buf->pro) {
            shift = buf->tail - buf->head;
        } else {
            shift = buf->pro - buf->head - 1; // leave one character so we cab backup one
        }
        if (0 >= shift) { /* no space left so allocate more */
            char        *old = buf->head;
            size_t      size = buf->end - buf->head + BUF_PAD;
        
            if (buf->head == buf->base) {
                buf->head = ALLOC_N(char, size * 2);
                memcpy(buf->head, old, size);
            } else {
		REALLOC_N(buf->head, char, size * 2);
            }
            buf->end = buf->head + size * 2 - BUF_PAD;
            buf->tail = buf->head + (buf->tail - old);
            buf->read_end = buf->head + (buf->read_end - old);
            if (0 != buf->pro) {
                buf->pro = buf->head + (buf->pro - old);
            }
            if (0 != buf->str) {
                buf->str = buf->head + (buf->str - old);
            }
        } else {
            memmove(buf->head, buf->head + shift, buf->read_end - (buf->head + shift));
            buf->tail -= shift;
            buf->read_end -= shift;
            if (0 != buf->pro) {
                buf->pro -= shift;
            }
            if (0 != buf->str) {
                buf->str -= shift;
            }
        }
    }
    err = buf->read_func(buf);
    *buf->read_end = '\0';

    return err;
}

static VALUE
rescue_cb(VALUE rbuf, VALUE err) {
#ifndef JRUBY_RUBY
    /* JRuby seems to play by a different set if rules. It passes in an Fixnum
     * instead of an error like other Rubies. For now assume all errors are
     * EOF and deal with the results further down the line. */
#if (defined(RUBINIUS_RUBY) || (1 == RUBY_VERSION_MAJOR && 8 == RUBY_VERSION_MINOR))
    if (rb_obj_class(err) != rb_eTypeError) {
#else
    if (rb_obj_class(err) != rb_eEOFError) {
#endif
	Buf	buf = (Buf)rbuf;

        //ox_sax_drive_cleanup(buf->dr); called after exiting protect
        rb_raise(err, "at line %d, column %d\n", buf->line, buf->col);
    }
#endif
    return Qfalse;
}

static VALUE
partial_io_cb(VALUE rbuf) {
    Buf		buf = (Buf)rbuf;
    VALUE	args[1];
    VALUE	rstr;
    char	*str;
    size_t	cnt;

    args[0] = ULONG2NUM(buf->end - buf->tail);
    rstr = rb_funcall2(buf->io, ox_readpartial_id, 1, args);
    str = StringValuePtr(rstr);
    cnt = strlen(str);
    //printf("*** read %lu bytes, str: '%s'\n", cnt, str);
    strcpy(buf->tail, str);
    buf->read_end = buf->tail + cnt;

    return Qtrue;
}

static VALUE
io_cb(VALUE rbuf) {
    Buf		buf = (Buf)rbuf;
    VALUE	args[1];
    VALUE	rstr;
    char	*str;
    size_t	cnt;

    args[0] = ULONG2NUM(buf->end - buf->tail);
    rstr = rb_funcall2(buf->io, ox_read_id, 1, args);
    str = StringValuePtr(rstr);
    cnt = strlen(str);
    /*printf("*** read %lu bytes, str: '%s'\n", cnt, str); */
    strcpy(buf->tail, str);
    buf->read_end = buf->tail + cnt;

    return Qtrue;
}

static int
read_from_io_partial(Buf buf) {
    return (Qfalse == rb_rescue(partial_io_cb, (VALUE)buf, rescue_cb, (VALUE)buf));
}

static int
read_from_io(Buf buf) {
    return (Qfalse == rb_rescue(io_cb, (VALUE)buf, rescue_cb, (VALUE)buf));
}

#ifndef JRUBY_RUBY
static int
read_from_fd(Buf buf) {
    ssize_t     cnt;
    size_t      max = buf->end - buf->tail;

    cnt = read(buf->fd, buf->tail, max);
    if (cnt < 0) {
        ox_sax_drive_error(buf->dr, "failed to read from file");
        return -1;
    } else if (0 != cnt) {
        buf->read_end = buf->tail + cnt;
    }
    return 0;
}
#endif

static char*
ox_stpncpy(char *dest, const char *src, size_t n) {
    size_t	cnt = strlen(src) + 1;

    if (n < cnt) {
	cnt = n;
    }
    strncpy(dest, src, cnt);

    return dest + cnt - 1;
}


static int
read_from_str(Buf buf) {
    size_t      max = buf->end - buf->tail - 1;
    char	*s;
    long	cnt;

    if ('\0' == *buf->in_str) {
	/* done */
	return -1;
    }
    s = ox_stpncpy(buf->tail, buf->in_str, max);
    *s = '\0';
    cnt = s - buf->tail;
    buf->in_str += cnt;
    buf->read_end = buf->tail + cnt;

    return 0;
}

