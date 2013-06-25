/* special.c
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

#include "special.h"

/*
u0000..u007F                00000000000000xxxxxxx  0xxxxxxx
u0080..u07FF                0000000000yyyyyxxxxxx  110yyyyy 10xxxxxx
u0800..uD7FF, uE000..uFFFF  00000zzzzyyyyyyxxxxxx  1110zzzz 10yyyyyy 10xxxxxx
u10000..u10FFFF             uuuzzzzzzyyyyyyxxxxxx  11110uuu 10zzzzzz 10yyyyyy 10xxxxxx
*/
char*
ox_ucs_to_utf8_chars(char *text, uint64_t u) {
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
