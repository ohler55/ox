/* parse.c
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

static void	read_instruction(PInfo pi);
static void	read_doctype(PInfo pi);
static void	read_comment(PInfo pi);
static char*	read_element(PInfo pi);
static void	read_text(PInfo pi);
/*static void	  read_reduced_text(PInfo pi); */
static void	read_cdata(PInfo pi);
static char*	read_name_token(PInfo pi);
static char*	read_quoted_value(PInfo pi);
static char*	read_hex_uint64(char *b, uint64_t *up);
static char*	read_10_uint64(char *b, uint64_t *up);
static char*	ucs_to_utf8_chars(char *text, uint64_t u);
static char*	read_coded_chars(PInfo pi, char *text);
static void	next_non_white(PInfo pi);
static int	collapse_special(PInfo pi, char *str);

/* This XML parser is a single pass, destructive, callback parser. It is a
 * single pass parse since it only make one pass over the characters in the
 * XML document string. It is destructive because it re-uses the content of
 * the string for values in the callback and places \0 characters at various
 * places to mark the end of tokens and strings. It is a callback parser like
 * a SAX parser because it uses callback when document elements are
 * encountered.
 *
 * Parsing is very tolerant. Lack of headers and even mispelled element
 * endings are passed over without raising an error. A best attempt is made in
 * all cases to parse the string.
 */

inline static void
next_non_white(PInfo pi) {
    for (; 1; pi->s++) {
	switch (*pi->s) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	    break;
	default:
	    return;
	}
    }
}

inline static void
next_white(PInfo pi) {
    for (; 1; pi->s++) {
	switch (*pi->s) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	case '\0':
	    return;
	default:
	    break;
	}
    }
}

VALUE
ox_parse(char *xml, ParseCallbacks pcb, char **endp, Options options) {
    struct _PInfo	pi;
    int			body_read = 0;

    if (0 == xml) {
	raise_error("Invalid arg, xml string can not be null", xml, 0);
    }
    if (DEBUG <= options->trace) {
	printf("Parsing xml:\n%s\n", xml);
    }
    /* initialize parse info */
    pi.str = xml;
    pi.s = xml;
    pi.h = 0;
    pi.pcb = pcb;
    pi.obj = Qnil;
    pi.circ_array = 0;
    pi.options = options;
    while (1) {
	next_non_white(&pi);	/* skip white space */
	if ('\0' == *pi.s) {
	    break;
	}
	if (body_read && 0 != endp) {
	    *endp = pi.s;
	    break;
	}
	if ('<' != *pi.s) {		/* all top level entities start with < */
	    raise_error("invalid format, expected <", pi.str, pi.s);
	}
	pi.s++;		/* past < */
	switch (*pi.s) {
	case '?':	/* processing instruction */
	    pi.s++;
	    read_instruction(&pi);
	    break;
	case '!':	/* comment or doctype */
	    pi.s++;
	    if ('\0' == *pi.s) {
		raise_error("invalid format, DOCTYPE or comment not terminated", pi.str, pi.s);
	    } else if ('-' == *pi.s) {
		pi.s++;	/* skip - */
		if ('-' != *pi.s) {
		    raise_error("invalid format, bad comment format", pi.str, pi.s);
		} else {
		    pi.s++;	/* skip second - */
		    read_comment(&pi);
		}
	    } else if ((TolerantEffort == options->effort) ? 0 == strncasecmp("DOCTYPE", pi.s, 7) : 0 == strncmp("DOCTYPE", pi.s, 7)) {
		pi.s += 7;
		read_doctype(&pi);
	    } else {
		raise_error("invalid format, DOCTYPE or comment expected", pi.str, pi.s);
	    }
	    break;
	case '\0':
	    raise_error("invalid format, document not terminated", pi.str, pi.s);
	default:
	    read_element(&pi);
	    body_read = 1;
	    break;
	}
    }
    return pi.obj;
}

static char*
gather_content(const char *src, char *content, size_t len) {
    for (; 0 < len; src++, content++, len--) {
	switch (*src) {
	case '?':
	    if ('>' == *(src + 1)) {
		*content = '\0';
		return (char*)(src + 1);
	    }
	    *content = *src;
	    break;
	case '\0':
	    return 0;
	default:
	    *content = *src;
	    break;
	}
    }
    return 0;
}

/* Entered after the "<?" sequence. Ready to read the rest.
 */
static void
read_instruction(PInfo pi) {
    char		content[1024];
    struct _Attr	attrs[MAX_ATTRS + 1];
    Attr		a = attrs;
    char		*target;
    char		*end;
    char		c;
    char		*cend;
    int			attrs_ok = 1;

    *content = '\0';
    memset(attrs, 0, sizeof(attrs));
    target = read_name_token(pi);
    end = pi->s;
    if (0 == (cend = gather_content(pi->s, content, sizeof(content) - 1))) {
	raise_error("processing instruction content too large or not terminated", pi->str, pi->s);
    }
    next_non_white(pi);
    c = *pi->s;
    *end = '\0'; /* terminate name */
    if ('?' != c) {
	while ('?' != c) {
	    pi->last = 0;
	    if ('\0' == *pi->s) {
		raise_error("invalid format, processing instruction not terminated", pi->str, pi->s);
	    }
	    next_non_white(pi);
	    a->name = read_name_token(pi);
	    end = pi->s;
	    next_non_white(pi);
	    if ('=' != *pi->s++) {
		attrs_ok = 0;
		break;
	    }
	    *end = '\0'; /* terminate name */
	    /* read value */
	    next_non_white(pi);
	    a->value = read_quoted_value(pi);
	    a++;
	    if (MAX_ATTRS <= (a - attrs)) {
		attrs_ok = 0;
		break;
	    }
	    next_non_white(pi);
	    if ('\0' == pi->last) {
		c = *pi->s;
	    } else {
		c = pi->last;
	    }
	}
	if ('?' == *pi->s) {
	    pi->s++;
	}
    } else {
	pi->s++;
    }
    if (attrs_ok) {
	if ('>' != *pi->s++) {
	    raise_error("invalid format, processing instruction not terminated", pi->str, pi->s);
	}
    } else {
	pi->s = cend + 1;
    }
    if (0 != pi->pcb->instruct) {
	if (attrs_ok) {
	    pi->pcb->instruct(pi, target, attrs, 0);
	} else {
	    pi->pcb->instruct(pi, target, attrs, content);
	}
    }
}

/* Entered after the "<!DOCTYPE" sequence plus the first character after
 * that. Ready to read the rest. Returns error code.
 */
static void
read_doctype(PInfo pi) {
    char	*docType;
    int		depth = 1;
    char	c;

    next_non_white(pi);
    docType = pi->s;
    while (1) {
	c = *pi->s++;
	if ('\0' == c) {
	    raise_error("invalid format, prolog not terminated", pi->str, pi->s);
	} else if ('<' == c) {
	    depth++;
	} else if ('>' == c) {
	    depth--;
	    if (0 == depth) {	/* done, at the end */
		pi->s--;
		break;
	    }
	}
    }
    *pi->s = '\0';
    pi->s++;
    if (0 != pi->pcb->add_doctype) {
	pi->pcb->add_doctype(pi, docType);
    }
}

/* Entered after "<!--". Returns error code.
 */
static void
read_comment(PInfo pi) {
    char	*end;
    char	*s;
    char	*comment;
    int		done = 0;
    
    next_non_white(pi);
    comment = pi->s;
    end = strstr(pi->s, "-->");
    if (0 == end) {
	raise_error("invalid format, comment not terminated", pi->str, pi->s);
    }
    for (s = end - 1; pi->s < s && !done; s--) {
	switch(*s) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	    break;
	default:
	    *(s + 1) = '\0';
	    done = 1;
	    break;
	}
    }
    *end = '\0'; /* in case the comment was blank */
    pi->s = end + 3;
    if (0 != pi->pcb->add_comment) {
	pi->pcb->add_comment(pi, comment);
    }
}

/* Entered after the '<' and the first character after that. Returns status
 * code.
 */
static char*
read_element(PInfo pi) {
    struct _Attr	attrs[MAX_ATTRS];
    Attr		ap = attrs;
    char		*name;
    char		*ename;
    char		*end;
    char		c;
    long		elen;
    int			hasChildren = 0;
    int			done = 0;

    ename = read_name_token(pi);
    end = pi->s;
    elen = end - ename;
    next_non_white(pi);
    c = *pi->s;
    *end = '\0';
    if ('/' == c) {
	/* empty element, no attributes and no children */
	pi->s++;
	if ('>' != *pi->s) {
	    /*printf("*** '%s' ***\n", pi->s); */
	    raise_error("invalid format, element not closed", pi->str, pi->s);
	}
	pi->s++;	/* past > */
	ap->name = 0;
	pi->pcb->add_element(pi, ename, attrs, hasChildren);
	pi->pcb->end_element(pi, ename);

	return 0;
    }
    /* read attribute names until the close (/ or >) is reached */
    while (!done) {
	if ('\0' == c) {
	    next_non_white(pi);
	    c = *pi->s;
	}
	pi->last = 0;
	switch (c) {
	case '\0':
	    raise_error("invalid format, document not terminated", pi->str, pi->s);
	case '/':
	    /* Element with just attributes. */
	    pi->s++;
	    if ('>' != *pi->s) {
		raise_error("invalid format, element not closed", pi->str, pi->s);
	    }
	    pi->s++;
	    ap->name = 0;
	    pi->pcb->add_element(pi, ename, attrs, hasChildren);
	    pi->pcb->end_element(pi, ename);

	    return 0;
	case '>':
	    /* has either children or a value */
	    pi->s++;
	    hasChildren = 1;
	    done = 1;
	    ap->name = 0;
	    pi->pcb->add_element(pi, ename, attrs, hasChildren);
	    break;
	default:
	    /* Attribute name so it's an element and the attribute will be */
	    /* added to it. */
	    ap->name = read_name_token(pi);
	    end = pi->s;
	    next_non_white(pi);
	    if ('=' != *pi->s++) {
		if (TolerantEffort == pi->options->effort) {
		    pi->s--;
		    pi->last = *pi->s;
		    *end = '\0'; /* terminate name */
		    ap->value = "";
		    ap++;
		    if (MAX_ATTRS <= (ap - attrs)) {
			raise_error("too many attributes", pi->str, pi->s);
		    }
		    break;
		} else {
		    raise_error("invalid format, no attribute value", pi->str, pi->s);
		}
	    }
	    *end = '\0'; /* terminate name */
	    /* read value */
	    next_non_white(pi);
	    ap->value = read_quoted_value(pi);
	    if (0 != strchr(ap->value, '&')) {
		if (0 != collapse_special(pi, (char*)ap->value)) {
		    raise_error("invalid format, special character does not end with a semicolon", pi->str, pi->s);
		}
	    }
	    ap++;
	    if (MAX_ATTRS <= (ap - attrs)) {
		raise_error("too many attributes", pi->str, pi->s);
	    }
	    break;
	}
	if ('\0' == pi->last) {
	    c = '\0';
	} else {
	    c = pi->last;
	    pi->last = '\0';
	}
    }
    if (hasChildren) {
	char	*start;
	int	first = 1;
	
	done = 0;
	/* read children */
	while (!done) {
	    start = pi->s;
	    next_non_white(pi);
	    c = *pi->s++;
	    if ('\0' == c) {
		raise_error("invalid format, document not terminated", pi->str, pi->s);
	    }
	    if ('<' == c) {
		char	*slash;

		switch (*pi->s) {
		case '!':	/* better be a comment or CDATA */
		    pi->s++;
		    if ('-' == *pi->s && '-' == *(pi->s + 1)) {
			pi->s += 2;
			read_comment(pi);
		    } else if ((TolerantEffort == pi->options->effort) ?
			       0 == strncasecmp("[CDATA[", pi->s, 7) :
			       0 == strncmp("[CDATA[", pi->s, 7)) {
			pi->s += 7;
			read_cdata(pi);
		    } else {
			raise_error("invalid format, invalid comment or CDATA format", pi->str, pi->s);
		    }
		    break;
		case '?':	/* processing instruction */
		    pi->s++;
		    read_instruction(pi);
		    break;
		case '/':
		    slash = pi->s;
		    pi->s++;
		    name = read_name_token(pi);
		    end = pi->s;
		    next_non_white(pi);
		    c = *pi->s;
		    *end = '\0';
		    if (0 != strcmp(name, ename)) {
			if (TolerantEffort == pi->options->effort) {
			    pi->pcb->end_element(pi, ename);
			    return name;
			} else {
			    raise_error("invalid format, elements overlap", pi->str, pi->s);
			}
		    }
		    if ('>' != c) {
			raise_error("invalid format, element not closed", pi->str, pi->s);
		    }
		    if (first && start != slash - 1) {
			/* some white space between start and here so add as text */
			*(slash - 1) = '\0';
			pi->pcb->add_text(pi, start, 1);
		    }
		    pi->s++;
		    pi->pcb->end_element(pi, ename);
		    return 0;
		case '\0':
		    if (TolerantEffort == pi->options->effort) {
			return 0;
		    } else {
			raise_error("invalid format, document not terminated", pi->str, pi->s);
		    }
		default:
		    first = 0;
		    /* a child element */
		    // Child closed with mismatched name.
		    if (0 != (name = read_element(pi))) {
			if (0 == strcmp(name, ename)) {
			    pi->s++;
			    pi->pcb->end_element(pi, ename);
			    return 0;
			} else { // not the correct element yet
			    pi->pcb->end_element(pi, ename);
			    return name;
			}
		    }
		    break;
		}
	    } else {	/* read as TEXT */
		pi->s = start;
		/*pi->s--; */
		read_text(pi);
		/*read_reduced_text(pi); */

		/* to exit read_text with no errors the next character must be < */
		if ('/' == *(pi->s + 1) &&
		    0 == strncmp(ename, pi->s + 2, elen) &&
		    '>' == *(pi->s + elen + 2)) {
		    /* close tag after text so treat as a value */
		    pi->s += elen + 3;
		    pi->pcb->end_element(pi, ename);
		    return 0;
		}
	    }
	}
    }
    return 0;
}

static void
read_text(PInfo pi) {
    char	buf[MAX_TEXT_LEN];
    char	*b = buf;
    char	*alloc_buf = 0;
    char	*end = b + sizeof(buf) - 2;
    char	c;
    int		done = 0;

    while (!done) {
	c = *pi->s++;
	switch(c) {
	case '<':
	    done = 1;
	    pi->s--;
	    break;
	case '\0':
	    raise_error("invalid format, document not terminated", pi->str, pi->s);
	default:
	    if (end <= (b + (('&' == c) ? 7 : 0))) { /* extra 8 for special just in case it is sequence of bytes */
		unsigned long	size;
		
		if (0 == alloc_buf) {
		    size = sizeof(buf) * 2;
		    alloc_buf = ALLOC_N(char, size);
		    memcpy(alloc_buf, buf, b - buf);
		    b = alloc_buf + (b - buf);
		} else {
		    unsigned long	pos = b - alloc_buf;

		    size = (end - alloc_buf) * 2;
		    REALLOC_N(alloc_buf, char, size);
		    b = alloc_buf + pos;
		}
		end = alloc_buf + size - 2;
	    }
	    if ('&' == c) {
		b = read_coded_chars(pi, b);
	    } else {
		*b++ = c;
	    }
	    break;
	}
    }
    *b = '\0';
    if (0 != alloc_buf) {
	pi->pcb->add_text(pi, alloc_buf, ('/' == *(pi->s + 1)));
	xfree(alloc_buf);
    } else {
	pi->pcb->add_text(pi, buf, ('/' == *(pi->s + 1)));
    }
}

#if 0
static void
read_reduced_text(PInfo pi) {
    char	buf[MAX_TEXT_LEN];
    char	*b = buf;
    char	*alloc_buf = 0;
    char	*end = b + sizeof(buf) - 2;
    char	c;
    int		spc = 0;
    int		done = 0;

    while (!done) {
	c = *pi->s++;
	switch(c) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	    spc = 1;
	    break;
	case '<':
	    done = 1;
	    pi->s--;
	    break;
	case '\0':
	    raise_error("invalid format, document not terminated", pi->str, pi->s);
	default:
	    if (end <= (b + spc + (('&' == c) ? 7 : 0))) { /* extra 8 for special just in case it is sequence of bytes */
		unsigned long	size;
		
		if (0 == alloc_buf) {
		    size = sizeof(buf) * 2;
		    alloc_buf = ALLOC_N(char, size);
		    memcpy(alloc_buf, buf, b - buf);
		    b = alloc_buf + (b - buf);
		} else {
		    unsigned long	pos = b - alloc_buf;

		    size = (end - alloc_buf) * 2;
		    REALLOC(alloc_buf, char, size);
		    b = alloc_buf + pos;
		}
		end = alloc_buf + size - 2;
	    }
	    if (spc) {
		*b++ = ' ';
	    }
	    spc = 0;
	    if ('&' == c) {
		b = read_coded_chars(pi, b);
	    } else {
		*b++ = c;
	    }
	    break;
	}
    }
    *b = '\0';
    if (0 != alloc_buf) {
	pi->pcb->add_text(pi, alloc_buf, ('/' == *(pi->s + 1)));
	xfree(alloc_buf);
    } else {
	pi->pcb->add_text(pi, buf, ('/' == *(pi->s + 1)));
    }
}
#endif

static char*
read_name_token(PInfo pi) {
    char	*start;

    next_non_white(pi);
    start = pi->s;
    for (; 1; pi->s++) {
	switch (*pi->s) {
	case ' ':
	case '\t':
	case '\f':
	case '?':
	case '=':
	case '/':
	case '>':
	case '\n':
	case '\r':
	    return start;
	case '\0':
	    /* documents never terminate after a name token */
	    raise_error("invalid format, document not terminated", pi->str, pi->s);
	    break; /* to avoid warnings */
	default:
	    break;
	}
    }
    return start;
}

static void
read_cdata(PInfo pi) {
    char	*start;
    char	*end;

    start = pi->s;
    end = strstr(pi->s, "]]>");
    if (end == 0) {
	raise_error("invalid format, CDATA not terminated", pi->str, pi->s);
    }
    *end = '\0';
    pi->s = end + 3;
    if (0 != pi->pcb->add_cdata) {
	pi->pcb->add_cdata(pi, start, end - start);
    }
}

inline static void
next_non_token(PInfo pi) {
    for (; 1; pi->s++) {
	switch(*pi->s) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	case '/':
	case '>':
	    return;
	default:
	    break;
	}
    }
}

/* Assume the value starts immediately and goes until the quote character is
 * reached again. Do not read the character after the terminating quote.
 */
static char*
read_quoted_value(PInfo pi) {
    char	*value = 0;
    
    if ('"' == *pi->s || ('\'' == *pi->s && StrictEffort != pi->options->effort)) {
        char	term = *pi->s;
        
        pi->s++;	/* skip quote character */
        value = pi->s;
        for (; *pi->s != term; pi->s++) {
            if ('\0' == *pi->s) {
                raise_error("invalid format, document not terminated", pi->str, pi->s);
            }
        }
        *pi->s = '\0';	/* terminate value */
        pi->s++;	/* move past quote */
    } else if (StrictEffort == pi->options->effort) {
	raise_error("invalid format, expected a quote character", pi->str, pi->s);
    } else if (TolerantEffort == pi->options->effort) {
        value = pi->s;
        for (; 1; pi->s++) {
	    switch (*pi->s) {
	    case '\0':
                raise_error("invalid format, document not terminated", pi->str, pi->s);
	    case ' ':
	    case '/':
	    case '>':
	    case '?': // for instructions
	    case '\t':
	    case '\n':
	    case '\r':
		pi->last = *pi->s;
		*pi->s = '\0';	/* terminate value */
		pi->s++;
		return value;
	    default:
		break;
            }
        }
    } else {
        value = pi->s;
        next_white(pi);
	if ('\0' == *pi->s) {
	    raise_error("invalid format, document not terminated", pi->str, pi->s);
        }
        *pi->s++ = '\0'; /* terminate value */
    }
    return value;
}

static char*
read_hex_uint64(char *b, uint64_t *up) {
    uint64_t	u = 0;
    char	c;

    for (; ';' != *b; b++) {
	c = *b;
	if ('0' <= c && c <= '9') {
	    u = (u << 4) | (uint64_t)(c - '0');
	} else if ('a' <= c && c <= 'f') {
	    u = (u << 4) | (uint64_t)(c - 'a' + 10);
	} else if ('A' <= c && c <= 'F') {
	    u = (u << 4) | (uint64_t)(c - 'A' + 10);
	} else {
	    return 0;
	}
    }
    *up = u;

    return b;
}

static char*
read_10_uint64(char *b, uint64_t *up) {
    uint64_t	u = 0;
    char	c;

    for (; ';' != *b; b++) {
	c = *b;
	if ('0' <= c && c <= '9') {
	    u = (u * 10) + (uint64_t)(c - '0');
	} else {
	    return 0;
	}
    }
    *up = u;

    return b;
}

/*
u0000..u007F                00000000000000xxxxxxx  0xxxxxxx
u0080..u07FF                0000000000yyyyyxxxxxx  110yyyyy 10xxxxxx
u0800..uD7FF, uE000..uFFFF  00000zzzzyyyyyyxxxxxx  1110zzzz 10yyyyyy 10xxxxxx
u10000..u10FFFF             uuuzzzzzzyyyyyyxxxxxx  11110uuu 10zzzzzz 10yyyyyy 10xxxxxx
*/
static char*
ucs_to_utf8_chars(char *text, uint64_t u) {
    int			reading = 0;
    int			i;
    unsigned char	c;

    if (u <= 0x000000000000007FULL) {
	/* 0xxxxxxx */
	*text++ = (char)u;
    } else if (u <= 0x00000000000007FFULL) {
	/* 110yyyyy 10xxxxxx */
	*text++ = (char)(0x00000000000000C0ULL | (0x000000000000001FULL & (u >> 6)));
	*text++ = (char)(0x0000000000000080ULL | (0x000000000000003FULL & u));
    } else if (u <= 0x000000000000D7FFULL || (0x000000000000E000ULL <= u && u <= 0x000000000000FFFFULL)) {
	/* 1110zzzz 10yyyyyy 10xxxxxx */
	*text++ = (char)(0x00000000000000E0ULL | (0x000000000000000FULL & (u >> 12)));
	*text++ = (char)(0x0000000000000080ULL | (0x000000000000003FULL & (u >> 6)));
	*text++ = (char)(0x0000000000000080ULL | (0x000000000000003FULL & u));
    } else if (0x0000000000010000ULL <= u && u <= 0x000000000010FFFFULL) {
	/* 11110uuu 10zzzzzz 10yyyyyy 10xxxxxx */
	*text++ = (char)(0x00000000000000F0ULL | (0x0000000000000007ULL & (u >> 18)));
	*text++ = (char)(0x0000000000000080ULL | (0x000000000000003FULL & (u >> 12)));
	*text++ = (char)(0x0000000000000080ULL | (0x000000000000003FULL & (u >> 6)));
	*text++ = (char)(0x0000000000000080ULL | (0x000000000000003FULL & u));
    } else {
	/* assume it is UTF-8 encoded directly and not UCS */
	for (i = 56; 0 <= i; i -= 8) {
	    c = (unsigned char)((u >> i) & 0x00000000000000FFULL);
	    if (reading) {
		*text++ = (char)c;
	    } else if ('\0' != c) {
		*text++ = (char)c;
		reading = 1;
	    }
	}
    }
    return text;
}

static char*
read_coded_chars(PInfo pi, char *text) {
    char	*b, buf[32];
    char	*end = buf + sizeof(buf) - 1;
    char	*s;

    for (b = buf, s = pi->s; b < end; b++, s++) {
	*b = *s;
	if (';' == *s) {
	    *(b + 1) = '\0';
	    s++;
	    break;
	}
    }
    if (b > end) {
	*text++ = '&';
    } else if ('#' == *buf) {
	uint64_t	u = 0;
	
	b = buf + 1;
	if ('x' == *b || 'X' == *b) {
	    b = read_hex_uint64(b + 1, &u);
	} else {
	    b = read_10_uint64(b, &u);
	}
	if (0 == b) {
	    *text++ = '&';
	} else {
	    if (u <= 0x000000000000007FULL) {
		*text++ = (char)u;
#if HAS_PRIVATE_ENCODING
	    } else if (ox_utf8_encoding == pi->options->rb_enc ||
		       0 == strcasecmp(rb_str_ptr(rb_String(ox_utf8_encoding)), rb_str_ptr(rb_String(pi->options->rb_enc)))) {
#else
	    } else if (ox_utf8_encoding == pi->options->rb_enc) {
#endif
		text = ucs_to_utf8_chars(text, u);
#if HAS_PRIVATE_ENCODING
	    } else if (Qnil == pi->options->rb_enc) {
#else
	    } else if (0 == pi->options->rb_enc) {
#endif
		pi->options->rb_enc = ox_utf8_encoding;
		text = ucs_to_utf8_chars(text, u);
	    } else if (TolerantEffort == pi->options->effort) {
		*text++ = '&';
		return text;
	    } else {
		/*raise_error("Invalid encoding, need UTF-8 or UTF-16 encoding to parse &#nnnn; character sequences.", pi->str, pi->s); */
		raise_error("Invalid encoding, need UTF-8 encoding to parse &#nnnn; character sequences.", pi->str, pi->s);
	    }
	    pi->s = s;
	}
    } else if (0 == strcasecmp(buf, "nbsp;")) {
	pi->s = s;
	*text++ = ' ';
    } else if (0 == strcasecmp(buf, "lt;")) {
	pi->s = s;
	*text++ = '<';
    } else if (0 == strcasecmp(buf, "gt;")) {
	pi->s = s;
	*text++ = '>';
    } else if (0 == strcasecmp(buf, "amp;")) {
	pi->s = s;
	*text++ = '&';
    } else if (0 == strcasecmp(buf, "quot;")) {
	pi->s = s;
	*text++ = '"';
    } else if (0 == strcasecmp(buf, "apos;")) {
	pi->s = s;
	*text++ = '\'';
    } else {
	*text++ = '&';
    }
    return text;
}

static int
collapse_special(PInfo pi, char *str) {
    char	*s = str;
    char	*b = str;

    while ('\0' != *s) {
	if ('&' == *s) {
	    int		c;
	    char	*end;
	    
	    s++;
	    if ('#' == *s) {
		uint64_t	u = 0;
		char		x;

		s++;
		if ('x' == *s || 'X' == *s) {
		    x = *s;
		    s++;
		    end = read_hex_uint64(s, &u);
		} else {
		    x = '\0';
		    end = read_10_uint64(s, &u);
		}
		if (0 == end) {
		    if (TolerantEffort == pi->options->effort) {
			*b++ = '&';
			*b++ = '#';
			if ('\0' != x) {
			    *b++ = x;
			}
			continue;
		    }
		    return EDOM;
		}
		if (u <= 0x000000000000007FULL) {
		    *b++ = (char)u;
#if HAS_PRIVATE_ENCODING
		} else if (ox_utf8_encoding == pi->options->rb_enc ||
			   0 == strcasecmp(rb_str_ptr(rb_String(ox_utf8_encoding)), rb_str_ptr(rb_String(pi->options->rb_enc)))) {
#else
		} else if (ox_utf8_encoding == pi->options->rb_enc) {
#endif
		    b = ucs_to_utf8_chars(b, u);
		    /* TBD support UTF-16 */
#if HAS_PRIVATE_ENCODING
		} else if (Qnil == pi->options->rb_enc) {
#else
		} else if (0 == pi->options->rb_enc) {
#endif
		    pi->options->rb_enc = ox_utf8_encoding;
		    b = ucs_to_utf8_chars(b, u);
		} else {
		    /* raise_error("Invalid encoding, need UTF-8 or UTF-16 encoding to parse &#nnnn; character sequences.", pi->str, pi->s);*/
		    raise_error("Invalid encoding, need UTF-8 encoding to parse &#nnnn; character sequences.", pi->str, pi->s);
		}
		s = end + 1;
	    } else {
		if (0 == strncasecmp(s, "lt;", 3)) {
		    c = '<';
		    s += 3;
		} else if (0 == strncasecmp(s, "gt;", 3)) {
		    c = '>';
		    s += 3;
		} else if (0 == strncasecmp(s, "amp;", 4)) {
		    c = '&';
		    s += 4;
		} else if (0 == strncasecmp(s, "quot;", 5)) {
		    c = '"';
		    s += 5;
		} else if (0 == strncasecmp(s, "apos;", 5)) {
		    c = '\'';
		    s += 5;
		} else if (TolerantEffort == pi->options->effort) {
		    *b++ = '&';
		    continue;
		} else {
		    c = '?';
		    while (';' != *s++) {
			if ('\0' == *s) {
			    return EDOM;
			}
		    }
		    s++;
		}
		*b++ = (char)c;
	    }
	} else {
	    *b++ = *s++;
	}
    }
    *b = '\0';

    return 0;
}
