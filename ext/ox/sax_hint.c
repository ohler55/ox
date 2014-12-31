/* hint.c
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#include <string.h>
#include <stdio.h>

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
    { "a", 0, 0, 0 },
    { "abbr", 0, 0, 0 },
    { "acronym", 0, 0, 0 },
    { "address", 0, 0, 0 },
    { "applet", 0, 0, 0 },
    { "area", 1, 0, map_0 },
    { "article", 0, 0, 0 },
    { "aside", 0, 0, 0 },
    { "audio", 0, 0, 0 },
    { "b", 0, 0, 0 },
    { "base", 1, 0, head_0 },
    { "basefont", 1, 0, head_0 },
    { "bdi", 0, 0, 0 },
    { "bdo", 0, 1, 0 },
    { "big", 0, 0, 0 },
    { "blockquote", 0, 0, 0 },
    { "body", 0, 0, html_0 },
    { "br", 1, 0, 0 },
    { "button", 0, 0, 0 },
    { "canvas", 0, 0, 0 },
    { "caption", 0, 0, table_0 },
    { "center", 0, 0, 0 },
    { "cite", 0, 0, 0 },
    { "code", 0, 0, 0 },
    { "col", 1, 0, colgroup_0 },
    { "colgroup", 0, 0, 0 },
    { "command", 1, 0, 0 },
    { "datalist", 0, 0, 0 },
    { "dd", 0, 0, dl_0 },
    { "del", 0, 0, 0 },
    { "details", 0, 0, 0 },
    { "dfn", 0, 0, 0 },
    { "dialog", 0, 0, dt_th_0 },
    { "dir", 0, 0, 0 },
    { "div", 0, 1, 0 },
    { "dl", 0, 0, 0 },
    { "dt", 0, 1, dl_0 },
    { "em", 0, 0, 0 },
    { "embed", 1, 0, 0 },
    { "fieldset", 0, 0, 0 },
    { "figcaption", 0, 0, figure_0 },
    { "figure", 0, 0, 0 },
    { "font", 0, 1, 0 },
    { "footer", 0, 0, 0 },
    { "form", 0, 0, 0 },
    { "frame", 1, 0, frameset_0 },
    { "frameset", 0, 0, 0 },
    { "h1", 0, 0, 0 },
    { "h2", 0, 0, 0 },
    { "h3", 0, 0, 0 },
    { "h4", 0, 0, 0 },
    { "h5", 0, 0, 0 },
    { "h6", 0, 0, 0 },
    { "head", 0, 0, html_0 },
    { "header", 0, 0, 0 },
    { "hgroup", 0, 0, 0 },
    { "hr", 1, 0, 0 },
    { "html", 0, 0, 0 },
    { "i", 0, 0, 0 },
    { "iframe", 1, 0, 0 },
    { "img", 1, 0, 0 },
    { "input", 1, 0, 0 }, // somewhere under a form_0
    { "ins", 0, 0, 0 },
    { "kbd", 0, 0, 0 },
    { "keygen", 1, 0, 0 },
    { "label", 0, 0, 0 }, // somewhere under a form_0
    { "legend", 0, 0, fieldset_0 },
    { "li", 0, 0, ol_ul_menu_0 },
    { "link", 1, 0, head_0 },
    { "map", 0, 0, 0 },
    { "mark", 0, 0, 0 },
    { "menu", 0, 0, 0 },
    { "meta", 1, 0, head_0 },
    { "meter", 0, 0, 0 },
    { "nav", 0, 0, 0 },
    { "noframes", 0, 0, 0 },
    { "noscript", 0, 0, 0 },
    { "object", 0, 0, 0 },
    { "ol", 0, 1, 0 },
    { "optgroup", 0, 0, 0 },
    { "option", 0, 0, optgroup_select_datalist_0 },
    { "output", 0, 0, 0 },
    { "p", 0, 0, 0 },
    { "param", 1, 0, 0 },
    { "pre", 0, 0, 0 },
    { "progress", 0, 0, 0 },
    { "q", 0, 0, 0 },
    { "rp", 0, 0, ruby_0 },
    { "rt", 0, 0, ruby_0 },
    { "ruby", 0, 0, 0 },
    { "s", 0, 0, 0 },
    { "samp", 0, 0, 0 },
    { "script", 0, 0, 0 },
    { "section", 0, 1, 0 },
    { "select", 0, 0, 0 },
    { "small", 0, 0, 0 },
    { "source", 0, 0, audio_video_0 },
    { "span", 0, 1, 0 },
    { "strike", 0, 0, 0 },
    { "strong", 0, 0, 0 },
    { "style", 0, 0, 0 },
    { "sub", 0, 0, 0 },
    { "summary", 0, 0, details_0 },
    { "sup", 0, 0, 0 },
    { "table", 0, 0, 0 },
    { "tbody", 0, 0, table_0 },
    { "td", 0, 0, tr_0 },
    { "textarea", 0, 0, 0 },
    { "tfoot", 0, 0, table_0 },
    { "th", 0, 0, tr_0 },
    { "thead", 0, 0, table_0 },
    { "time", 0, 0, 0 },
    { "title", 0, 0, head_0 },
    { "tr", 0, 0, table_0 },
    { "track", 1, 0, audio_video_0 },
    { "tt", 0, 0, 0 },
    { "u", 0, 0, 0 },
    { "ul", 0, 0, 0 },
    { "var", 0, 0, 0 },
    { "video", 0, 0, 0 },
    { "wbr", 1, 0, 0 },
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
