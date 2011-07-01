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

static void     read_prolog(PInfo pi);
static void     read_doctype(PInfo pi);
static void     read_comment(PInfo pi);
static void     read_element(PInfo pi);
static void     read_text(PInfo pi);
static void     read_cdata(PInfo pi);
static char*    read_name_token(PInfo pi);
static char*    read_quoted_value(PInfo pi);
static int      read_coded_char(PInfo pi);
static void     next_non_white(PInfo pi);

static int	validateProlog = 1;

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
	switch(*pi->s) {
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

VALUE
parse(char *xml, ParseCallbacks pcb, char **endp, int trace, int best_effort) {
    struct _PInfo       pi;
    int                 body_read = 0;

    if (0 == xml) {
	raise_error("Invalid arg, xml string can not be null", xml, 0);
    }
    if (DEBUG <= trace) {
        printf("Parsing xml:\n%s\n", xml);
    }
    /* initialize parse info */
    pi.str = xml;
    pi.s = xml;
    pi.h = 0;
    pi.pcb = pcb;
    pi.obj = Qnil;
    pi.circ_array = 0;
    pi.encoding = 0;
    pi.trace = trace;
    pi.best_effort = best_effort;
    while (1) {
	next_non_white(&pi);	// skip white space
	if ('\0' == *pi.s) {
	    break;
	}
        if (body_read && 0 != endp) {
            *endp = pi.s;
            break;
        }
	if ('<' != *pi.s) {		// all top level entities start with <
	    raise_error("invalid format, expected <", pi.str, pi.s);
	}
	pi.s++;		// past <
	switch (*pi.s) {
	case '?':	// prolog
	    pi.s++;
	    read_prolog(&pi);
	    break;
	case '!':	/* comment or doctype */
	    pi.s++;
	    if ('\0' == *pi.s) {
		raise_error("invalid format, DOCTYPE or comment not terminated", pi.str, pi.s);
	    } else if ('-' == *pi.s) {
		pi.s++;	// skip -
		if ('-' != *pi.s) {
		    raise_error("invalid format, bad comment format", pi.str, pi.s);
		} else {
		    pi.s++;	// skip second -
		    read_comment(&pi);
		}
	    } else if (0 == strncmp("DOCTYPE", pi.s, 7)) {
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

/* Entered after the "<?" sequence. Ready to read the rest.
 */
static void
read_prolog(PInfo pi) {
    char        *version = 0;
    char        *encoding = 0;
    char        *standalone = 0;
    char        *name;
    char        *end;
    char        c;
    
    // skip xml string
    if (0 != strncasecmp("xml", pi->s, 3)) {
	raise_error("invalid format, expected 'xml'", pi->str, pi->s);
    }
    pi->s += 3;	// past xml
    /* looking for ?> to terminate the prolog */
    while ('?' != *pi->s) {
	if ('\0' == *pi->s) {
	    raise_error("invalid format, prolog not terminated", pi->str, pi->s);
	}
        name = read_name_token(pi);
        end = pi->s;
	next_non_white(pi);
        c = *pi->s;
        *end = '\0'; // terminate name
        if ('=' == c) {
	    // Figure out what the token is, read a value for it, and check
	    // against supported values.
	    pi->s++;
	    next_non_white(pi);
	    if (0 == strcasecmp("version", name)) {
		version = read_quoted_value(pi);
		if (validateProlog &&
                    (0 != strcmp("1.0", version) &&
                     0 != strcmp("1.1", version))) {
		    raise_error("invalid format, wrong XML version", pi->str, pi->s);
		}
	    } else if (0 == strcasecmp("encoding", name)) {
		encoding = read_quoted_value(pi);
                /*
		if (validateProlog && 0 != strcasecmp("UTF-8", encoding)) {
		    raise_error("invalid format, only UTF-8 supported", pi->str, pi->s);
		}
                */
	    } else if (0 == strcasecmp("standalone", name)) {
		standalone = read_quoted_value(pi);
		if (validateProlog && 0 != strcmp("yes", standalone)) {
		    raise_error("invalid format, only standalone XML supported", pi->str, pi->s);
		}
	    } else {
		raise_error("invalid format, unknown prolog attribute", pi->str, pi->s);
	    }
	} else if ('?' == c) {
	    pi->s++;
	    if ('>' != *pi->s++) {
		raise_error("invalid format, prolog not terminated", pi->str, pi->s);
	    }
	    return;
	} else {
	    raise_error("invalid format, prolog format error", pi->str, pi->s);
	}
    }
    if ('\0' == pi->s) {
	raise_error("invalid format, prolog not terminated", pi->str, pi->s);
    }
    if ('?' == *pi->s) {
	pi->s++;
    }
    if ('>' != *pi->s++) {
	raise_error("invalid format, prolog not terminated", pi->str, pi->s);
    }
    if (0 != pi->pcb->add_prolog) {
        pi->pcb->add_prolog(pi, version, encoding, standalone);
    }
}

/* Entered after the "<!DOCTYPE" sequence plus the first character after
 * that. Ready to read the rest. Returns error code.
 */
static void
read_doctype(PInfo pi) {
    char        *docType;
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
    char        *comment;
    int         done = 0;
    
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
    *end = '\0'; // in case the comment was blank
    pi->s = end + 3;
    if (0 != pi->pcb->add_comment) {
        pi->pcb->add_comment(pi, comment);
    }
}

/* Entered after the '<' and the first character after that. Returns status
 * code.
 */
static void
read_element(PInfo pi) {
    struct _Attr        attrs[MAX_ATTRS];
    Attr                ap = attrs;
    char	        *name;
    char                *ename;
    char                *end;
    char        	c;
    long	        elen;
    int 		hasChildren = 0;
    int	        	done = 0;

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
            printf("*** '%s'***\n", pi->s);
	    raise_error("invalid format, element not closed", pi->str, pi->s);
	}
	pi->s++;	/* past > */
        ap->name = 0;
        pi->pcb->add_element(pi, ename, attrs, hasChildren);
        pi->pcb->end_element(pi, ename);

	return;
    }
    /* read attribute names until the close (/ or >) is reached */
    while (!done) {
        if ('\0' == c) {
            next_non_white(pi);
            c = *pi->s;
        }
	switch (c) {
	case '\0':
	    raise_error("invalid format, document not terminated", pi->str, pi->s);
	case '/':
	    // Element with just attributes.
	    pi->s++;
	    if ('>' != *pi->s) {
		raise_error("invalid format, element not closed", pi->str, pi->s);
	    }
	    pi->s++;
            ap->name = 0;
            pi->pcb->add_element(pi, ename, attrs, hasChildren);
            pi->pcb->end_element(pi, ename);

	    return;
	case '>':
	    // has either children or a value
	    pi->s++;
	    hasChildren = 1;
	    done = 1;
            ap->name = 0;
            pi->pcb->add_element(pi, ename, attrs, hasChildren);
	    break;
	default:
	    // Attribute name so it's an element and the attribute will be
	    // added to it.
	    ap->name = read_name_token(pi);
            end = pi->s;
	    next_non_white(pi);
	    if ('=' != *pi->s++) {
		raise_error("invalid format, no attribute value", pi->str, pi->s);
	    }
            *end = '\0'; // terminate name
	    // read value
	    next_non_white(pi);
	    ap->value = read_quoted_value(pi);
            ap++;
            if (MAX_ATTRS <= (ap - attrs)) {
		raise_error("too many attributes", pi->str, pi->s);
            }
	    break;
	}
        c = '\0';
    }
    if (hasChildren) {
	char	*start;
	
	done = 0;
	// read children
	while (!done) {
	    start = pi->s;
	    next_non_white(pi);
	    c = *pi->s++;
	    if ('\0' == c) {
		raise_error("invalid format, document not terminated", pi->str, pi->s);
	    }
	    if ('<' == c) {
		switch (*pi->s) {
		case '!':	/* better be a comment or CDATA */
		    pi->s++;
		    if ('-' == *pi->s && '-' == *(pi->s + 1)) {
			pi->s += 2;
			read_comment(pi);
		    } else if (0 == strncmp("[CDATA[", pi->s, 7)) {
			pi->s += 7;
			read_cdata(pi);
		    } else {
			raise_error("invalid format, invalid comment or CDATA format", pi->str, pi->s);
		    }
		    break;
		case '/':
		    pi->s++;
		    name = read_name_token(pi);
                    end = pi->s;
		    next_non_white(pi);
                    c = *pi->s;
                    *end = '\0';
		    if (0 != strcmp(name, ename)) {
			raise_error("invalid format, elements overlap", pi->str, pi->s);
		    }
		    if ('>' != c) {
			raise_error("invalid format, element not closed", pi->str, pi->s);
		    }
                    pi->s++;
                    pi->pcb->end_element(pi, ename);
		    return;
		case '\0':
		    raise_error("invalid format, document not terminated", pi->str, pi->s);
		default:
		    // a child element
		    read_element(pi);
		    break;
		}
	    } else {	// read as TEXT
		pi->s = start;
		//pi->s--;
		read_text(pi);
		// to exit read_text with no errors the next character must be <
		if ('/' == *(pi->s + 1) &&
		    0 == strncmp(ename, pi->s + 2, elen) &&
		    '>' == *(pi->s + elen + 2)) {
		    // close tag after text so treat as a value
		    pi->s += elen + 3;
                    pi->pcb->end_element(pi, ename);
		    return;
		}
	    }
	}
    }
}

static void
read_text(PInfo pi) {
    char        buf[MAX_TEXT_LEN];
    char	*b = buf;
    char        *alloc_buf = 0;
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
	    if ('&' == c) {
		c = read_coded_char(pi);
	    }
	    if (end <= b + spc) {
                unsigned long   size;
                
                if (0 == alloc_buf) {
                    size = sizeof(buf) * 2;
                    if (0 == (alloc_buf = (char*)malloc(size))) {
                        raise_error("text too long", pi->str, pi->s);
                    }
                    memcpy(alloc_buf, buf, b - buf);
                    b = alloc_buf + (b - buf);
                } else {
                    unsigned long       pos = b - alloc_buf;

                    size = (end - alloc_buf) * 2;
                    if (0 == (alloc_buf = (char*)realloc(alloc_buf, size))) {
                        raise_error("text too long", pi->str, pi->s);
                    }
                    b = alloc_buf + pos;
                }
                end = alloc_buf + size;
	    }
	    if (spc) {
		*b++ = ' ';
	    }
	    spc = 0;
	    *b++ = c;
	    break;
	}
    }
    *b = '\0';
    pi->pcb->add_text(pi, buf, ('/' == *(pi->s + 1)));
    if (0 != alloc_buf) {
        free(alloc_buf);
    }
}

static char*
read_name_token(PInfo pi) {
    char        *start;

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
            // documents never terminate after a name token
	    raise_error("invalid format, document not terminated", pi->str, pi->s);
            break; // to avoid warnings
	default:
	    break;
	}
    }
    return start;
}

static void
read_cdata(PInfo pi) {
    char	*start;
    char        *end;

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

/* Assume the value starts immediately and goes until the quote character is
 * reached again. Do not read the character after the terminating quote.
 */
static char*
read_quoted_value(PInfo pi) {
    char        *value;
    
    if ('"' != *pi->s) {
	raise_error("invalid format, expected a quote character", pi->str, pi->s);
    }
    pi->s++;	// skip quote character
    value = pi->s;
    for (; *pi->s != '"'; pi->s++) {
	if ('\0' == *pi->s) {
	    raise_error("invalid format, document not terminated", pi->str, pi->s);
	}
    }
    *pi->s = '\0'; // terminate value
    pi->s++;       // move past quote

    return value;
}

static int
read_coded_char(PInfo pi) {
    char	*b, buf[8];
    char	*end = buf + sizeof(buf);
    char	*s;
    int	c;

    for (b = buf, s = pi->s; b < end; b++, s++) {
	if (';' == *s) {
	    *b = '\0';
	    s++;
	    break;
	}
	*b = *s;
    }
    if (b > end) {
	return *pi->s;
    }
    if ('#' == *buf) {
	c = (int)strtol(buf + 1, &end, 10);
	if (0 >= c || '\0' != *end) {
	    return *pi->s;
	}
	pi->s = s;

	return c;
    }
    if (0 == strcasecmp(buf, "nbsp")) {
	pi->s = s;
	return ' ';
    } else if (0 == strcasecmp(buf, "lt")) {
	pi->s = s;
	return '<';
    } else if (0 == strcasecmp(buf, "gt")) {
	pi->s = s;
	return '>';
    } else if (0 == strcasecmp(buf, "amp")) {
	pi->s = s;
	return '&';
    } else if (0 == strcasecmp(buf, "quot")) {
	pi->s = s;
	return '"';
    } else if (0 == strcasecmp(buf, "apos")) {
	pi->s = s;
	return '\'';
    }
    return *pi->s;
}

