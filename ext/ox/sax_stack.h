/* sax_stack.h
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#ifndef OX_SAX_STACK_H
#define OX_SAX_STACK_H

#include "sax_hint.h"

#define STACK_INC	32

typedef struct _nv {
    const char	*name;
    VALUE	val;
    int		childCnt;
    Hint	hint;
} *Nv;

typedef struct _nStack {
    struct _nv	base[STACK_INC];
    Nv		head;	/* current stack */
    Nv		end;	/* stack end */
    Nv		tail;	/* pointer to one past last element name on stack */
} *NStack;

inline static void
stack_init(NStack stack) {
    stack->head = stack->base;
    stack->end = stack->base + sizeof(stack->base) / sizeof(struct _nv);
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
	    stack->head = ALLOC_N(struct _nv, len + STACK_INC);
	    memcpy(stack->head, stack->base, sizeof(struct _nv) * len);
	} else {
	    REALLOC_N(stack->head, struct _nv, len + STACK_INC);
	}
	stack->tail = stack->head + toff;
	stack->end = stack->head + len + STACK_INC;
    }
    stack->tail->name = name;
    stack->tail->val = val;
    stack->tail->hint = hint;
    stack->tail->childCnt = 0;
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

#endif /* OX_SAX_STACK_H */
