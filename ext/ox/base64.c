/* base64.c
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

#include "base64.h"

static char	digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// invalid or terminating characters are set to 'X' or \x58
static u_char   s_digits[256] = "\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x3E\x58\x58\x58\x3F\
\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x58\x58\x58\x58\x58\x58\
\x58\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\
\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x58\x58\x58\x58\x58\
\x58\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\
\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30\x31\x32\x33\x58\x58\x58\x58\x58\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\
\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58\x58";

void
to_base64(const u_char *src, int len, char *b64) {
    const u_char	*end3;
    int			len3 = len % 3;
    u_char		b1, b2, b3;
    
    end3 = src + (len - len3);
    while (src < end3) {
	b1 = *src++;
	b2 = *src++;
	b3 = *src++;
	*b64++ = digits[b1 >> 2];
	*b64++ = digits[((b1 & 0x03) << 4) | (b2 >> 4)];
	*b64++ = digits[((b2 & 0x0F) << 2) | (b3 >> 6)];
	*b64++ = digits[b3 & 0x3F];
    }
    if (1 == len3) {
	b1 = *src++;
	*b64++ = digits[b1 >> 2];
	*b64++ = digits[(b1 & 0x03) << 4];
	*b64++ = '=';
	*b64++ = '=';
    } else if (2 == len3) {
	b1 = *src++;
	b2 = *src++;
	*b64++ = digits[b1 >> 2];
	*b64++ = digits[((b1 & 0x03) << 4) | (b2 >> 4)];
	*b64++ = digits[(b2 & 0x0F) << 2];
	*b64++ = '=';
    }
    *b64 = '\0';
}

unsigned long
b64_orig_size(const char *text) {
    const char          *start = text;
    unsigned long        size = 0;
    
    if ('\0' != *text) {
        for (; 0 != *text; text++) { }
        size = (text - start) * 3 / 4;
        text--;
        if ('=' == *text) {
            size--;
            text--;
            if ('=' == *text) {
                size--;
            }
        }
    }
    return size;
}

void
from_base64(const char *b64, u_char *str) {
    u_char      b0, b1, b2, b3;
    
    while (1) {
        if ('X' == (b0 = s_digits[(u_char)*b64++])) { break; }
        if ('X' == (b1 = s_digits[(u_char)*b64++])) { break; }
        *str++ = (b0 << 2) | ((b1 >> 4) & 0x03);
        if ('X' == (b2 = s_digits[(u_char)*b64++])) { break; }
        *str++ = (b1 << 4) | ((b2 >> 2) & 0x0F);
        if ('X' == (b3 = s_digits[(u_char)*b64++])) { break; }
        *str++ = (b2 << 6) | b3;
    }
    *str = '\0';
}
