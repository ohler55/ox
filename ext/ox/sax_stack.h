/* sax_stack.h
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

#ifndef __OX_SAX_STACK_H__
#define __OX_SAX_STACK_H__

#include "sax_hint.h"

#define STACK_INC	32

typedef struct _Nv {
    const char	*name;
    VALUE	val;
    Hint	hint;
} *Nv;

typedef struct _NStack {
    struct _Nv	base[STACK_INC];
    Nv		head;	/* current stack */
    Nv		end;	/* stack end */
    Nv		tail;	/* pointer to one past last element name on stack */
} *NStack;

inline static void
stack_init(NStack stack) {
    stack->head = stack->base;
    stack->end = stack->base + sizeof(stack->base) / sizeof(struct _Nv);
    stack->tail = stack->head;
}

inline static int
stack_empty(NStack stack) {
    return (stack->head == stack->tail);
}

inline static void
stack_cleanup(NStack stack) {
    if (stack->base != stack->head) {
        xfree(stack->head);
    }
}

inline static void
stack_push(NStack stack, const char *name, VALUE val, Hint hint) {
    if (stack->end <= stack->tail) {
	size_t	len = stack->end - stack->head;
	size_t	toff = stack->tail - stack->head;

	if (stack->base == stack->head) {
	    stack->head = ALLOC_N(struct _Nv, len + STACK_INC);
	    memcpy(stack->head, stack->base, sizeof(struct _Nv) * len);
	} else {
	    REALLOC_N(stack->head, struct _Nv, len + STACK_INC);
	}
	stack->tail = stack->head + toff;
	stack->end = stack->head + len + STACK_INC;
    }
    stack->tail->name = name;
    stack->tail->val = val;
    stack->tail->hint = hint;
    stack->tail++;
}

inline static Nv
stack_peek(NStack stack) {
    if (stack->head < stack->tail) {
	return stack->tail - 1;
    }
    return 0;
}

inline static Nv
stack_pop(NStack stack) {
    if (stack->head < stack->tail) {
	stack->tail--;
	return stack->tail;
    }
    return 0;
}

#endif /* __OX_SAX_STACK_H__ */
