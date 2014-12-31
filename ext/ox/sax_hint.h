/* hint.h
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#ifndef __OX_HINT_H__
#define __OX_HINT_H__

typedef struct _Hint {
    const char	*name;
    char	empty;	// must be closed or close auto it, not error
    char	nest;	// nesting allowed
    const char	**parents;
} *Hint;

typedef struct _Hints {
    const char	*name;
    Hint	hints; // array of hints
    int		size;
} *Hints;

extern Hints	ox_hints_html(void);
extern Hint	ox_hint_find(Hints hints, const char *name);

#endif /* __OX_HINT_H__ */
