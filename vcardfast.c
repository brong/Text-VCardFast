/* vcardfast.c : fast vcard parser */

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "vcardfast.h"

/* taken from cyrus, but I wrote the code originally,
   so I can relicence it -- Bron */
static size_t roundup(size_t size)
{
    if (size < 32)
        return 32;
    if (size < 64)
        return 64;
    if (size < 128)
        return 128;
    if (size < 256)
        return 256;
    if (size < 512)
        return 512;
    return ((size + 1024) & ~1023);
}

static void buf_ensure(struct buf *buf, size_t n)
{
    size_t newalloc = roundup(buf->len + n);

    if (newalloc <= buf->alloc)
        return;

    buf->s = realloc(buf->s, newalloc);
    buf->alloc = newalloc;
}

static void buf_putc(struct buf *buf, char c)
{
    buf_ensure(buf, 1);
    buf->s[buf->len++] = c;
}

static char *buf_release(struct buf *buf)
{
    char *ret;
    buf_ensure(buf, 1);
    buf->s[buf->len] = '\0';
    ret = buf->s;
    buf->s = NULL;
    buf->len = buf->alloc = 0;
    return ret;
}

static void buf_free(struct buf *buf)
{
    free(buf->s);
    buf->s = NULL;
    buf->len = buf->alloc = 0;
}


#define NOTESTART() state->itemstart = state->p
#define MAKE(X, Y) X = malloc(sizeof(struct Y)); memset(X, 0, sizeof(struct Y))
#define TAKEVAL() state->val = buf_release(&state->buf)
#define PUTC(C) buf_putc(&state->buf, C)
#define PUTLC(C) PUTC(((C) >= 'A' && (C) <= 'Z') ? (C) - 'A' + 'a' : (C))
#define INC(I) state->p += I

/* just leaves it on the buffer */
static int _parse_param_quoted(struct vcardfast_state *state)
{
    NOTESTART();

    while (*state->p) {
        switch (*state->p) {
        case '"':
	    INC(1);
	    return 0;

	/* normal backslash quoting - NOTE, not strictly RFC complient,
	 * but I figure anyone who generates one PROBABLY meant to escape
	 * the next character because it's so common, and LABEL definitely
	 * allows \n, so we have to handle that anyway */
	case '\\':
	    if (!state->p[1])
		return PE_BACKQUOTE_EOF;
	    if (state->p[1] == 'n' || state->p[1] == 'N')
		PUTC('\n');
	    else
		PUTC(state->p[1]);
	    INC(2);
	    break;

	/* special value quoting for doublequote and endline (RFC 6868) */
	case '^':
	    if (state->p[1] == '\'') {
		PUTC('"');
		INC(2);
	    }
	    else if (state->p[1] == 'n') { /* only lower case per the RFC */
		PUTC('\n');
		INC(2);
	    }
	    else if (state->p[1] == '^') {
		PUTC('^');
		INC(2);
	    }
	    else {
		PUTC('^');
		INC(1); /* treat next char normally */
	    }
	    break;

	case '\r':
	    INC(1);
	    break; /* just skip */
	case '\n':
	    if (state->p[1] != ' ' && state->p[1] != '\t')
		return PE_QSTRING_EOL;
	    INC(2);
	    break;

        default:
	    PUTC(*state->p);
	    INC(1);
	    break;
        }
    }

    return PE_QSTRING_EOF;
}

static int _parse_param_key(struct vcardfast_state *state)
{
    NOTESTART();

    while (*state->p) {
	switch (*state->p) {
	case '=':
	    state->param->name = buf_release(&state->buf);
	    INC(1);
	    return 0;

	case '\r':
	    INC(1);
	    break; /* just skip */
	case '\n':
	    if (state->p[1] != ' ' && state->p[1] != '\t')
		return PE_KEY_EOL;
	    INC(2);
	    break;

	/* XXX - check exact legal set? */
        default:
	    PUTLC(*state->p);
	    INC(1);
	    break;
	}
    }

    return PE_KEY_EOF;
}

static int _parse_entry_params(struct vcardfast_state *state)
{
    struct vcardfast_param **paramp = &state->entry->params;
    int r;

repeat:
    MAKE(state->param, vcardfast_param);

    r = _parse_param_key(state);
    if (r) return r;

    NOTESTART();

    /* now get the value */
    while (*state->p) {
        switch (*state->p) {
	case '\\': /* normal backslash quoting */
	    if (!state->p[1])
		return PE_BACKQUOTE_EOF;
	    if (state->p[1] == 'n' || state->p[1] == 'N')
		PUTC('\n');
	    else
		PUTC(state->p[1]);
	    INC(2);
	    break;

	case '^': /* special value quoting for doublequote (RFC 6868) */
	    if (state->p[1] == '\'') {
		PUTC('"');
		INC(2);
	    }
	    else if (state->p[1] == 'n') {
		PUTC('\n');
		INC(2);
	    }
	    else if (state->p[1] == '^') {
		PUTC('^');
		INC(2);
	    }
	    else {
		PUTC('^');
		INC(1); /* treat next char normally */
	    }
	    break;

	case '"':
	    INC(1);
	    r = _parse_param_quoted(state);
	    if (r) return r;
	    break;

	case ':':
	    /* done - all parameters parsed */
	    state->param->value = buf_release(&state->buf);
	    *paramp = state->param;
	    state->param = NULL;
	    INC(1);
	    return 0;

	case ';':
	    /* another parameter to parse */
	    state->param->value = buf_release(&state->buf);
	    *paramp = state->param;
	    paramp = &state->param->next;
	    INC(1);
	    goto repeat;

	case '\r':
	    INC(1);
	    break; /* just skip */
	case '\n':
	    if (state->p[1] != ' ' && state->p[1] != '\t')
		return PE_PARAMVALUE_EOL;
	    INC(2);
	    break;

	default:
	    PUTC(*state->p);
	    INC(1);
	    break;
        }
    }

    return PE_PARAMVALUE_EOF;
}

static int _parse_entry_key(struct vcardfast_state *state)
{
    NOTESTART();

    while (*state->p) {
	switch (*state->p) {
	case ':':
	    state->entry->name = buf_release(&state->buf);
	    INC(1);
	    return 0;

	case ';':
	    state->entry->name = buf_release(&state->buf);
	    INC(1);
	    return _parse_entry_params(state);

	case '.':
	    if (state->entry->group)
		return PE_ENTRY_MULTIGROUP;
	    state->entry->group = buf_release(&state->buf);
	    INC(1);
	    break;

	case '\r':
	    INC(1);
	    break; /* just skip */
	case '\n':
	    if (state->p[1] == ' ' || state->p[1] == '\t') /* wrapped line */
		INC(2);
	    else if (!state->buf.len) /* no key yet?  blank intermediate lines are OK */
		INC(1);
	    else
		return PE_NAME_EOL;
	    break;

	default:
	    PUTLC(*state->p);
	    INC(1);
	    break;
	}
    }

    return PE_NAME_EOF;
}

static int _parse_entry_multivalue(struct vcardfast_state *state)
{
    struct vcardfast_list **valp = &state->entry->v.values;

    state->entry->multivalue = 1;

    NOTESTART();

repeat:
    MAKE(state->value, vcardfast_list);

    while (*state->p) {
	switch (*state->p) {
	/* only one type of quoting */
	case '\\':
	    if (!state->p[1])
		return PE_BACKQUOTE_EOF;
	    if (state->p[1] == 'n' || state->p[1] == 'N')
		PUTC('\n');
	    else
		PUTC(state->p[1]);
	    INC(2);
	    break;

	case ';':
	    state->value->s = buf_release(&state->buf);
	    *valp = state->value;
	    valp = &state->value->next;
	    INC(1);
	    goto repeat;

	case '\r':
	    INC(1);
	    break; /* just skip */
	case '\n':
	    if (state->p[1] == ' ' || state->p[1] == '\t') {/* wrapped line */
		INC(2);
		break;
	    }
	    /* otherwise it's the end of the value */
	    INC(1);
	    goto out;

	default:
	    PUTC(*state->p);
	    INC(1);
	    break;
	}
    }

out:
    /* reaching the end of the file isn't a failure here,
     * it's just another type of end-of-value */
    state->value->s = buf_release(&state->buf);
    *valp = state->value;
    state->value = NULL;
    return 0;
}

static int _parse_entry_value(struct vcardfast_state *state)
{
    struct vcardfast_list *item;

    for (item = state->mvproperties; item; item = item->next)
	if (!strcmp(state->entry->name, item->s))
	    return _parse_entry_multivalue(state);

    NOTESTART();

    while (*state->p) {
	switch (*state->p) {
	/* only one type of quoting */
	case '\\':
	    if (!state->p[1])
		return PE_BACKQUOTE_EOF;

	    if (state->p[1] == 'n' || state->p[1] == 'N')
		PUTC('\n');
	    else
		PUTC(state->p[1]);
	    INC(2);
	    break;

	case '\r':
	    INC(1);
	    break; /* just skip */
	case '\n':
	    if (state->p[1] == ' ' || state->p[1] == '\t') {/* wrapped line */
		INC(2);
		break;
	    }
	    /* otherwise it's the end of the value */
	    INC(1);
	    goto out;

	default:
	    PUTC(*state->p);
	    INC(1);
	    break;
	}
    }

out:
    /* reaching the end of the file isn't a failure here,
     * it's just another type of end-of-value */
    state->entry->v.value = buf_release(&state->buf);
    return 0;
}

/* FREE MEMORY */

static void _free_list(struct vcardfast_list *list)
{
    struct vcardfast_list *listnext;

    for (; list; list = listnext) {
	listnext = list->next;
	free(list->s);
	free(list);
    }
}

static void _free_param(struct vcardfast_param *param)
{
    struct vcardfast_param *paramnext;

    for (; param; param = paramnext) {
        paramnext = param->next;
        free(param->name);
        free(param->value);
        free(param);
    }
}

static void _free_entry(struct vcardfast_entry *entry)
{
    struct vcardfast_entry *entrynext;

    for (; entry; entry = entrynext) {
        entrynext = entry->next;
        free(entry->name);
	if (entry->multivalue)
	    _free_list(entry->v.values);
	else
	    free(entry->v.value);
	_free_param(entry->params);
        free(entry);
    }
}

static void _free_card(struct vcardfast_card *card)
{
    struct vcardfast_card *cardnext;

    for (; card; card = cardnext) {
	cardnext = card->next;
	free(card->type);
	_free_entry(card->properties);
	_free_card(card->objects);
	free(card);
    }
}

static void _free_state(struct vcardfast_state *state)
{
    buf_free(&state->buf);
    _free_card(state->card);
    _free_list(state->value);
    _free_entry(state->entry);
    _free_param(state->param);

    memset(state, 0, sizeof(struct vcardfast_state));
}

static int _parse_entry(struct vcardfast_state *state)
{
    int r = _parse_entry_key(state);
    if (r) return r;
    return _parse_entry_value(state);
}

static int _parse_vcard(struct vcardfast_state *state, struct vcardfast_card *card)
{
    struct vcardfast_card **subp = &card->objects;
    struct vcardfast_entry **entryp = &card->properties;
    const char *entrystart;
    int r;

    state->p = state->base;

    while (*state->p) {
	MAKE(state->entry, vcardfast_entry);

	entrystart = state->p;
	r = _parse_entry(state);
	if (r) return r;

	if (!strcmp(state->entry->name, "begin")) {
	    /* shouldn't be any params */
	    if (state->entry->params) {
		state->itemstart = entrystart;
		return PE_BEGIN_PARAMS;
	    }
	    /* only possible if some idiot passes 'begin' as
	     * multivalue field name */
	    if (state->entry->multivalue) {
		state->itemstart = entrystart;
		return PE_BEGIN_PARAMS;
	    }

	    MAKE(state->card, vcardfast_card);
	    state->card->type = strdup(state->entry->v.value);
	    _free_entry(state->entry);
	    state->entry = NULL;
	    /* we must stitch it in first, because state won't hold it */
	    *subp = state->card;
	    subp = &state->card->next;
	    r = _parse_vcard(state, state->card);
	    /* special case mismatched card, the "start" was the start of
	     * the card */
	    if (r == PE_MISMATCHED_CARD)
		state->itemstart = entrystart;
	    if (r) return r;
	}
	else if (!strcmp(state->entry->name, "end")) {
	    /* shouldn't be any params */
	    if (state->entry->params) {
		state->itemstart = entrystart;
		return PE_BEGIN_PARAMS;
	    }
	    /* only possible if some idiot passes 'end' as
	     * multivalue field name */
	    if (state->entry->multivalue) {
		state->itemstart = entrystart;
		return PE_BEGIN_PARAMS;
	    }

	    if (strcasecmp(state->entry->v.value, card->type))
		return PE_MISMATCHED_CARD;

	    _free_entry(state->entry);
	    state->entry = NULL;

	    return 0;
	}
	else {
	    /* it's a parameter on this one */
	    *entryp = state->entry;
	    entryp = &state->entry->next;
	    state->entry = NULL;
	}
    }

    if (card->type)
	return PE_FINISHED_EARLY;

    return 0;
}

/* PUBLIC API */

int vcardfast_parse(struct vcardfast_state *state)
{
    struct vcardfast_card *card = NULL;
    int r;

    MAKE(card, vcardfast_card);

    r = _parse_vcard(state, card);
    state->card = card;

    /* XXX - check for trailing non-whitespace? */

    return r;
}

void vcardfast_free(struct vcardfast_state *state)
{
    _free_state(state);
}

void vcardfast_fillpos(struct vcardfast_state *state, struct vcardfast_errorpos *pos)
{
    memset(pos, 0, sizeof(struct vcardfast_errorpos));
}

const char *vcardfast_errstr(int err)
{
    switch(err) {
    case PE_BACKQUOTE_EOF:
	return "EOF after backslash";
    case PE_BEGIN_PARAMS:
	return "Params on BEGIN field";
    case PE_ENTRY_MULTIGROUP:
	return "Multiple group levels in property name";
    case PE_FINISHED_EARLY:
	return "VCards not completed";
    case PE_KEY_EOF:
	return "End of data while parsing parameter key";
    case PE_KEY_EOL:
	return "End of line while parsing parameter key";
    case PE_MISMATCHED_CARD:
	return "Closed a different card name than opened";
    case PE_NAME_EOF:
	return "End of data while parsing entry name";
    case PE_NAME_EOL:
	return "End of line while parsing entry name";
    case PE_PARAMVALUE_EOF:
	return "End of data while parsing parameter value";
    case PE_PARAMVALUE_EOL:
	return "End of line while parsing parameter value";
    case PE_QSTRING_EOF:
	return "End of data while parsing quoted value";
    case PE_QSTRING_EOL:
	return "End of line while parsing quoted value";
    }
    return "Unknown error";
}
