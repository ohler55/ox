/* sax_buf.h
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

#ifndef __OX_SAX_BUF_H__
#define __OX_SAX_BUF_H__

typedef struct _Buf {
    char	base[0x00001000];
    char	*head;
    char	*end;
    char	*tail;
    char	*read_end;      /* one past last character read */
    char       	*pro;           /* protection start, buffer can not slide past this point */
    char        *str;           /* start of current string being read */
    int		line;
    int		col;
    int		pro_line;
    int		pro_col;
    int		(*read_func)(struct _Buf *buf);
    union {
        int     	fd;
        VALUE   	io;
	const char	*in_str;
    };
    struct _SaxDrive	*dr;
} *Buf;

typedef struct _CheckPt {
    int		pro_dif;
    int		line;
    int		col;
    char	c;
} *CheckPt;

#define CHECK_PT_INIT { -1, 0, 0, '\0' }

extern void	ox_sax_buf_init(Buf buf, VALUE io);
extern int	ox_sax_buf_read(Buf buf);

static inline char
buf_get(Buf buf) {
    //printf("*** drive get from '%s'  from start: %ld  buf: %p  from read_end: %ld\n", buf->tail, buf->tail - buf->head, buf->head, buf->read_end - buf->tail);
    if (buf->read_end <= buf->tail) {
        if (0 != ox_sax_buf_read(buf)) {
            return '\0';
        }
    }
    if ('\n' == *buf->tail) {
        buf->line++;
        buf->col = 0;
    }
    buf->col++;
    
    return *buf->tail++;
}

static inline void
buf_backup(Buf buf) {
    buf->tail--;
    buf->col--;
    if (0 >= buf->col) {
	buf->line--;
	// allow col to be negative since we never backup twice in a row
    }
}

static inline void
buf_protect(Buf buf) {
    buf->pro = buf->tail;
    buf->str = buf->tail; // can't have str before pro
    buf->pro_line = buf->line;
    buf->pro_col = buf->col;
}

static inline void
buf_reset(Buf buf) {
    buf->tail = buf->pro;
    buf->line = buf->pro_line;
    buf->col = buf->pro_col;
}

/* Starts by reading a character so it is safe to use with an empty or
 * compacted buffer.
 */
static inline char
buf_next_non_white(Buf buf) {
    char        c;

    while ('\0' != (c = buf_get(buf))) {
	switch(c) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	    break;
	default:
	    return c;
	}
    }
    return '\0';
}

/* Starts by reading a character so it is safe to use with an empty or
 * compacted buffer.
 */
static inline char
buf_next_white(Buf buf) {
    char        c;

    while ('\0' != (c = buf_get(buf))) {
	switch(c) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	case '\0':
	    return c;
	default:
	    break;
	}
    }
    return '\0';
}

static inline void
buf_cleanup(Buf buf) {
    if (buf->base != buf->head && 0 != buf->head) {
        xfree(buf->head);
	buf->head = 0;
    }
}

static inline int
is_white(char c) {
    switch(c) {
    case ' ':
    case '\t':
    case '\f':
    case '\n':
    case '\r':
        return 1;
    default:
        break;
    }
    return 0;
}

static inline void
buf_checkpoint(Buf buf, CheckPt cp) {
    cp->pro_dif = (int)(buf->tail - buf->pro);
    cp->line = buf->line;
    cp->col = buf->col;
    cp->c = *(buf->tail - 1);
}

static inline int
buf_checkset(CheckPt cp) {
    return (0 <= cp->pro_dif);
}

static inline char
buf_checkback(Buf buf, CheckPt cp) {
    buf->tail = buf->pro + cp->pro_dif;
    buf->line = cp->line;
    buf->col = cp->col;
    return cp->c;
}

#endif /* __OX_SAX_BUF_H__ */
