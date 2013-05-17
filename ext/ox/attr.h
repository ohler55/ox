/* attr.h
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

#ifndef __OX_ATTR_H__
#define __OX_ATTR_H__

#include "ox.h"

#define ATTR_STACK_INC	16

typedef struct _Attr {
    const char	*name;
    const char	*value;
} *Attr;

typedef struct _AttrStack {
    struct _Attr	base[ATTR_STACK_INC];
    Attr		head;	/* current stack */
    Attr		end;	/* stack end */
    Attr		tail;	/* pointer to one past last element name on stack */
} *AttrStack;

inline static void
attr_stack_init(AttrStack stack) {
    stack->head = stack->base;
    stack->end = stack->base + sizeof(stack->base) / sizeof(struct _Attr);
    stack->tail = stack->head;
    stack->head->name = 0;
}

inline static int
attr_stack_empty(AttrStack stack) {
    return (stack->head == stack->base);
}

inline static void
attr_stack_cleanup(AttrStack stack) {
    if (stack->base != stack->head) {
        xfree(stack->head);
	stack->head = stack->base;
    }
}

inline static void
attr_stack_push(AttrStack stack, const char *name, const char *value) {
    if (stack->end <= stack->tail + 1) {
	size_t	len = stack->end - stack->head;
	size_t	toff = stack->tail - stack->head;

	if (stack->base == stack->head) {
	    stack->head = ALLOC_N(struct _Attr, len + ATTR_STACK_INC);
	    memcpy(stack->head, stack->base, sizeof(struct _Attr) * len);
	} else {
	    REALLOC_N(stack->head, struct _Attr, len + ATTR_STACK_INC);
	}
	stack->tail = stack->head + toff;
	stack->end = stack->head + len + ATTR_STACK_INC;
    }
    stack->tail->name = name;
    stack->tail->value = value;
    stack->tail++;
    stack->tail->name = 0; // terminate
}

inline static Attr
attr_stack_peek(AttrStack stack) {
    if (stack->head < stack->tail) {
	return stack->tail - 1;
    }
    return 0;
}

inline static Attr
attr_stack_pop(AttrStack stack) {
    if (stack->head < stack->tail) {
	stack->tail--;
	return stack->tail;
    }
    return 0;
}

#endif /* __OX_ATTR_H__ */
