/* xml_check.h
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#ifndef OX_XML_CHECK_H
#define OX_XML_CHECK_H

#include <stdbool.h>
#include <stddef.h>

#include "err.h"
#include "xml_str.h"

/* What a writer asks before it writes anything, shared by dump.c and builder.c.
 * The scans are in xml_str.h, which stays free of Ruby so it can be reasoned
 * about on its own; these are the wrappers that turn an answer into a raise.
 */

/* Raises on a character XML has no way to write, for a value that is copied
 * through rather than escaped. The escape path answers the same question on its
 * way past, but inside a CDATA section, a comment or an instruction a character
 * reference is not expanded, so escaping is not an option for those.
 */
inline static void check_unescaped(const char *str, size_t size) {
    const unsigned char *bad = xml_first_invalid((const unsigned char *)str, size);

    if (NULL != bad) {
        rb_raise(ox_syntax_error_class, "'\\#x%02x' is not a valid XML character.", *bad);
    }
}

/* Refuses a name only for a character that could end it where it is written,
 * the narrow rule #469 settled on, since anything else is odd but round trips.
 * A byte XML can not hold at all keeps the message the values raise with.
 *
 * Escaping is not the answer here even where a table has an escape: a name is
 * not a place a character reference is expanded, so &lt; in a name reads back
 * as four more characters rather than as the one that was asked for.
 */
inline static void check_name_chars(const char *str, size_t size, bool attr) {
    const char           ends = attr ? XML_NAME_ATTR : XML_NAME_ELEMENT;
    const unsigned char *bad  = xml_first_bad_name((const unsigned char *)str, size, ends);
    const char          *kind = attr ? "an attribute" : "an element";

    if (NULL == bad) {
        return;
    }
    if (0x20 > *bad && '\t' != *bad && '\n' != *bad && '\r' != *bad) {
        rb_raise(ox_syntax_error_class, "'\\#x%02x' is not a valid XML character.", *bad);
    }
    if (0x20 >= *bad) {
        rb_raise(ox_syntax_error_class, "'\\#x%02x' can not be used in %s name.", *bad, kind);
    }
    rb_raise(ox_syntax_error_class, "'%c' can not be used in %s name.", *bad, kind);
}

#endif /* OX_XML_CHECK_H */
