/* hint.c
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <ruby.h>

#include "sax_hint.h"

static const char	*audio_video_0[] = { "audio", "video", 0 };
static const char	*colgroup_0[] = { "colgroup", 0 };
static const char	*details_0[] = { "details", 0 };
static const char	*dl_0[] = { "dl", 0 };
static const char	*dt_th_0[] = { "dt", "th", 0 };
static const char	*fieldset_0[] = { "fieldset", 0 };
static const char	*figure_0[] = { "figure", 0 };
static const char	*frameset_0[] = { "frameset", 0 };
static const char	*head_0[] = { "head", 0 };
static const char	*html_0[] = { "html", 0 };
static const char	*map_0[] = { "map", 0 };
static const char	*ol_ul_menu_0[] = { "ol", "ul", "menu", 0 };
static const char	*optgroup_select_datalist_0[] = { "optgroup", "select", "datalist", 0 };
static const char	*ruby_0[] = { "ruby", 0 };
static const char	*table_0[] = { "table", 0 };
static const char	*tr_0[] = { "tr", 0 };

static struct _Hint	html_hint_array[] = {
    { "a", false, false, false, true, NULL },
    { "abbr", false, false, false, true, NULL },
    { "acronym", false, false, false, true, NULL },
    { "address", false, false, false, true, NULL },
    { "applet", false, false, false, true, NULL },
    { "area", true, false, false, true, map_0 },
    { "article", false, false, false, true, NULL },
    { "aside", false, false, false, true, NULL },
    { "audio", false, false, false, true, NULL },
    { "b", false, false, false, true, NULL },
    { "base", true, false, false, true, head_0 },
    { "basefont", true, false, false, true, head_0 },
    { "bdi", false, false, false, true, NULL },
    { "bdo", false, true, false, true, NULL },
    { "big", false, false, false, true, NULL },
    { "blockquote", false, false, false, true, NULL },
    { "body", false, false, false, true, html_0 },
    { "br", true, false, false, true, NULL },
    { "button", false, false, false, true, NULL },
    { "canvas", false, false, false, true, NULL },
    { "caption", false, false, false, true, table_0 },
    { "center", false, false, false, true, NULL },
    { "cite", false, false, false, true, NULL },
    { "code", false, false, false, true, NULL },
    { "col", true, false, false, true, colgroup_0 },
    { "colgroup", false, false, false, true, NULL },
    { "command", true, false, false, true, NULL },
    { "datalist", false, false, false, true, NULL },
    { "dd", false, false, false, true, dl_0 },
    { "del", false, false, false, true, NULL },
    { "details", false, false, false, true, NULL },
    { "dfn", false, false, false, true, NULL },
    { "dialog", false, false, false, true, dt_th_0 },
    { "dir", false, false, false, true, NULL },
    { "div", false, true, false, true, NULL },
    { "dl", false, false, false, true, NULL },
    { "dt", false, true, false, true, dl_0 },
    { "em", false, false, false, true, NULL },
    { "embed", true, false, false, true, NULL },
    { "fieldset", false, false, false, true, NULL },
    { "figcaption", false, false, false, true, figure_0 },
    { "figure", false, false, false, true, NULL },
    { "font", false, true, false, true, NULL },
    { "footer", false, false, false, true, NULL },
    { "form", false, false, false, true, NULL },
    { "frame", true, false, false, true, frameset_0 },
    { "frameset", false, false, false, true, NULL },
    { "h1", false, false, false, true, NULL },
    { "h2", false, false, false, true, NULL },
    { "h3", false, false, false, true, NULL },
    { "h4", false, false, false, true, NULL },
    { "h5", false, false, false, true, NULL },
    { "h6", false, false, false, true, NULL },
    { "head", false, false, false, true, html_0 },
    { "header", false, false, false, true, NULL },
    { "hgroup", false, false, false, true, NULL },
    { "hr", true, false, false, true, NULL },
    { "html", false, false, false, true, NULL },
    { "i", false, false, false, true, NULL },
    { "iframe", true, false, false, true, NULL },
    { "img", true, false, false, true, NULL },
    { "input", true, false, false, true, NULL }, // somewhere under a form_0
    { "ins", false, false, false, true, NULL },
    { "kbd", false, false, false, true, NULL },
    { "keygen", true, false, false, true, NULL },
    { "label", false, false, false, true, NULL }, // somewhere under a form_0
    { "legend", false, false, false, true, fieldset_0 },
    { "li", false, false, false, true, ol_ul_menu_0 },
    { "link", true, false, false, true, head_0 },
    { "map", false, false, false, true, NULL },
    { "mark", false, false, false, true, NULL },
    { "menu", false, false, false, true, NULL },
    { "meta", true, false, false, true, head_0 },
    { "meter", false, false, false, true, NULL },
    { "nav", false, false, false, true, NULL },
    { "noframes", false, false, false, true, NULL },
    { "noscript", false, false, false, true, NULL },
    { "object", false, false, false, true, NULL },
    { "ol", false, true, false, true, NULL },
    { "optgroup", false, false, false, true, NULL },
    { "option", false, false, false, true, optgroup_select_datalist_0 },
    { "output", false, false, false, true, NULL },
    { "p", false, false, false, true, NULL },
    { "param", true, false, false, true, NULL },
    { "pre", false, false, false, true, NULL },
    { "progress", false, false, false, true, NULL },
    { "q", false, false, false, true, NULL },
    { "rp", false, false, false, true, ruby_0 },
    { "rt", false, false, false, true, ruby_0 },
    { "ruby", false, false, false, true, NULL },
    { "s", false, false, false, true, NULL },
    { "samp", false, false, false, true, NULL },
    { "script", false, false, true, true, NULL },
    { "section", false, true, false, true, NULL },
    { "select", false, false, false, true, NULL },
    { "small", false, false, false, true, NULL },
    { "source", false, false, false, true, audio_video_0 },
    { "span", false, true, false, true, NULL },
    { "strike", false, false, false, true, NULL },
    { "strong", false, false, false, true, NULL },
    { "style", false, false, false, true, NULL },
    { "sub", false, false, false, true, NULL },
    { "summary", false, false, false, true, details_0 },
    { "sup", false, false, false, true, NULL },
    { "table", false, false, false, true, NULL },
    { "tbody", false, false, false, true, table_0 },
    { "td", false, false, false, true, tr_0 },
    { "textarea", false, false, false, true, NULL },
    { "tfoot", false, false, false, true, table_0 },
    { "th", false, false, false, true, tr_0 },
    { "thead", false, false, false, true, table_0 },
    { "time", false, false, false, true, NULL },
    { "title", false, false, false, true, head_0 },
    { "tr", false, false, false, true, table_0 },
    { "track", true, false, false, true, audio_video_0 },
    { "tt", false, false, false, true, NULL },
    { "u", false, false, false, true, NULL },
    { "ul", false, false, false, true, NULL },
    { "var", false, false, false, true, NULL },
    { "video", false, false, false, true, NULL },
    { "wbr", true, false, false, true, NULL },
};
static struct _Hints	html_hints = {
    "HTML",
    html_hint_array,
    sizeof(html_hint_array) / sizeof(*html_hint_array)
};

Hints
ox_hints_html() {
    return &html_hints;
}

Hints
ox_hints_dup(Hints h) {
    Hints	nh = ALLOC(struct _Hints);

    nh->hints = ALLOC_N(struct _Hint, h->size);
    memcpy(nh->hints, h->hints, sizeof(struct _Hint) * h->size);
    nh->size = h->size;
    nh->name = h->name;
    
    return nh;
}

void
ox_hints_destroy(Hints h) {
    if (NULL != h && &html_hints != h) {
	xfree(h->hints);
	xfree(h);
    }
}

Hint
ox_hint_find(Hints hints, const char *name) {
    if (0 != hints) {
	Hint	lo = hints->hints;
	Hint	hi = hints->hints + hints->size - 1;
	Hint	mid;
	int		res;

	if (0 == (res = strcasecmp(name, lo->name))) {
	    return lo;
	} else if (0 > res) {
	    return 0;
	}
	if (0 == (res = strcasecmp(name, hi->name))) {
	    return hi;
	} else if (0 < res) {
	    return 0;
	}
	while (1 < hi - lo) {
	    mid = lo + (hi - lo) / 2;
	    if (0 == (res = strcasecmp(name, mid->name))) {
		return mid;
	    } else if (0 < res) {
		lo = mid;
	    } else {
		hi = mid;
	    }
	}
    }
    return 0;
}

void
ox_hint_set_active(Hints hints, bool active) {
    int		i;
    Hint	h;

    for (i = hints->size, h = hints->hints; 0 < i; i--, h++) {
	h->active = active;
    }
}
