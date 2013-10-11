/* type.h
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

#ifndef __OX_TYPE_H__
#define __OX_TYPE_H__

typedef enum {
    NoCode	   = 0,
    ArrayCode	   = 'a',
    String64Code   = 'b', /* base64 encoded String */
    ClassCode	   = 'c',
    Symbol64Code   = 'd', /* base64 encoded Symbol */
    DateCode	   = 'D',
    BigDecimalCode = 'B',
    ExceptionCode  = 'e',
    FloatCode	   = 'f',
    RegexpCode	   = 'g',
    HashCode	   = 'h',
    FixnumCode	   = 'i',
    BignumCode	   = 'j',
    KeyCode	   = 'k', /* indicates the value is a hash key, kind of a hack */
    RationalCode   = 'l',
    SymbolCode	   = 'm',
    FalseClassCode = 'n',
    ObjectCode	   = 'o',
    RefCode	   = 'p',
    RangeCode	   = 'r',
    StringCode	   = 's',
    TimeCode	   = 't',
    StructCode	   = 'u',
    ComplexCode	   = 'v',
    RawCode	   = 'x',
    TrueClassCode  = 'y',
    NilClassCode   = 'z',
} Type;

#endif /* __OX_TYPE_H__ */
