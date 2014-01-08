/* vcardfast.c : fast vcard parser */

#include <ctype.h>
#include <string.h>

#include "vcardfast.h"

static struct buf {
    char *s;
    size_t len;
    size_t alloc;
}

#define BUF_INITIALIZER { NULL, 0, 0 }

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

static void buf_ensure(size_t n)
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

static const char *buf_cstring(struct buf *buf)
{
    buf_ensure(buf, 1);
    buf->s[buf->len] = '\0';
    return buf->s;
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

static char *buf_strdup(struct buf *buf)
{
    return strdup(buf_cstring(buf));
}

static void buf_free(struct buf *buf)
{
    free(buf->s);
    buf->s = NULL;
    buf->len = buf->alloc = 0;
}

static void buf_reset(struct buf *buf)
{
    buf->len = 0;
}

#define MAKE(X, Y) X = malloc(sizeof(struct Y)); memset(X, 0, sizeof(struct Y))

static const char *_parse_param_quoted(const char *src, struct buf *dbuf)
{
    const char *p = src;

    while (*p) {
        switch (*p) {
        case '"':
            return p+1;
        case '^':
            if (p[1] == '\'') {
                buf_putc(dbuf, '"');
                p += 2;
            }
            else {
                buf_putc(dbuf, '^');
                if (p[1] == '^') p += 2;
                else p++;
            }
            break;

	case '\r':
	    break; /* just skip */
	case '\n':
	    if (p[1] != ' ' && p[1] != '\t')
		goto fail; /* end line without value */
	    p += 2;
	    break;

        default:
            buf_putc(dbuf, *p++);
            break;
        }
    }
fail:
    /* FAILURE - finished before the end of the parameter*/
    buf_reset(dbuf);

    return NULL;
}

static const char *_parse_param_key(const char *src, struct buf *dbuf)
{
    const char *p = src;

    while (*p) {
	switch (*p) {
	case '=':
	    return p+1;

	case '\r':
	    break; /* just skip */
	case '\n':
	    if (p[1] != ' ' && p[1] != '\t')
		goto fail; /* end line without value */
	    p += 2;
	    break;

	default:
	    buf_putc(dbuf, *p++);
	    break;
	}
    }
fail:
    /* FAILURE - finished before the end of the parameter*/
    buf_reset(dbuf);

    return NULL;
}

static const char *_parse_entry_params(const char *src, struct vcardfast_param **paramp)
{
    struct buf key = BUF_INITIALIZER;
    struct buf val = BUF_INITIALIZER;
    const char *p = src;

    p = _parse_param_key(p, &key);
    if (!p) goto fail;

    while (*p) {
        switch (*p) {
	case '\\': /* normal backslash quoting */
	    if (!p[1]) goto fail;
	    if (p[1] == 'n' || p[1] == 'N')
		buf_putc(&val, "\n");
	    else
		buf_putc(&val, p[1]);
	    p += 2;
	    break;
	case '^': /* special value quoting for doublequote (RFC 68xx) */
	    if (p[1] == '\'') {
		buf_putc(&val, '"');
		p += 2;
	    }
	    else if (p[1] == 'n') {
		buf_putc(&val, '\n');
		p += 2;
	    }
	    else {
		buf_putc(&val, '^');
		if (p[1] == '^') p += 2;
		else p++;
	    }
	    break;
	case '"':
	    p = _parse_param_quoted(p+1, &val);
	    if (!p) goto fail;
	    break;

	case ':':
	    /* done - all parameters parsed */
	    if (paramp) {
		struct vcardfast_param *param = NULL;
		MAKE(param, vcardfast_param);
		param->name = buf_release(&key);
		param->value = buf_release(&val);
		*paramp = param;
	    }
	    else {
		buf_free(&key);
		buf_free(&val);
	    }
	    return p+1;
	    break;
	case ';':
	    /* another parameter to parse */
	    if (paramp) {
		struct vcardfast_param *param = NULL;
		MAKE(param, vcardfast_param);
		param->name = buf_release(&key);
		param->value = buf_release(&val);
		*paramp = param;
		paramp = &param->next;
	    }
	    else {
		buf_free(&key);
		buf_free(&val);
	    }
	    p = _parse_param_key(p+1, &key);
	    if (!p) goto fail;
	    break;
	case ',':
	    /* multiple values separated by a comma */
	    if (paramp) {
		struct vcardfast_param *param = NULL;
		MAKE(param, vcardfast_param);
		param->name = buf_strdup(&key);
		param->value = buf_release(&val);
		*paramp = param;
		paramp = &param->next;
	    }
	    else {
		/* keeping key */
		buf_free(&val);
	    }
	    p++;
	    break;

	case '\r':
	    break; /* just skip */
	case '\n':
	    if (p[1] != ' ' && p[1] != '\t')
		goto fail; /* end line without value */
	    p += 2;
	    break;

	default:
	    buf_putc(&val, *p++);
	    break;
        }
    }

fail:
    buf_free(&key);
    buf_free(&val);
    return NULL;
}

static const char *_parse_entry_key(const char *src, struct buf dbuf, struct vcardfast_param **paramp)
{
    const char *p = src;

    while (*p) {
	switch (*p) {
	case ':':
	    return p+1;
	case ';':
	    return _parse_entry_params(p+1, paramp);

	case '\r':
	    break; /* just skip */
	case '\n':
	    if (p[1] == ' ' || p[1] == '\t') /* wrapped line */
		p += 2;
	    else if (!dbuf->len) /* no key yet?  blank intermediate lines are OK */
		p++;
	    else
		goto fail; /* end line without value */
	    break;

	default:
	    buf_putc(dbuf, *p++);
	    break;
	}
    }

fail:
    /* FAILURE - finished before the end of the parameter*/
    buf_reset(dbuf);

    return NULL;
}

static const char *_parse_entry_value(const char *src, struct buf dbuf)
{
    const char *p = src;

    while (*p) {
	switch (*p) {
	/* only one quoting */
	case '\\':
	    if (!p[1]) goto fail;
	    if (p[1] == 'n' || p[1] == 'N') {
		buf_putc(dbuf, '\n');
	    }
	    else {
		buf_putc(dbuf, p[1]);
	    }
	    p += 2;
	    break;

	case '\r':
	    break; /* just skip */
	case '\n':
	    if (p[1] == ' ' || p[1] == '\t') { /* wrapped line */
		p += 2;
		break;
	    }
	    return p + 1;

	default:
	    buf_putc(dbuf, *p++);
	    break;
	}
    }

    /* actually, reaching the end of the file isn't a total failure here */
    return p;

fail:
    /* FAILURE - finished before the end of the parameter*/
    buf_reset(dbuf);

    return NULL;
}

static void _vcardfast_param_free(struct vcardfast_param *param)
{
    struct vcardfast_param *paramnext;
    for (; param; param = paramnext) {
        paramnext = param->next;
        free(param->name);
        free(param->value);
        free(param);
    }
}

static void _vcardfast_entry_free(struct vcardfast_entry *entry)
{
    struct vcardfast_entry *entrynext;

    for (; entry; entry = entrynext) {
        entrynext = entry->next;
	_vcardfast_param_free(entry->params);
        free(entry->name);
        free(entry->value);
        free(entry);
    }
}

void vcardfast_free(vcardfast_card *card)
{
    struct vcardfast_card *sub, *subnext;;

    free(card->type);

    _vcardfast_entry_free(card->properties);

    for (sub = card->objects; sub; sub = subnext) {
        subnext = sub->next;
        vcardfast_free(sub);
    }

    free(card);
}


/* STATE MAP
 * 0: parsing key
 * 1: param key
 * 2: param value
 */
struct const char *vcardfast_parse(const char *src, struct vcardfast_card **cardp, int flags)
{
    struct buf key = BUF_INITIALIZER;
    struct buf val = BUF_INITIALIZER;
    struct vcardfast_param *params = NULL;
    struct vcardfast_entry *entry = NULL;
    const char *p;
    int state = 0;

    while (*p) {
	/* skip blank lines */
	if (*p == '\r' || *p == '\n') {
	    p++;
	    continue;
	}

	p = _parse_entry_key(p, &key, &params);
	p = _parse_entry_value(p, &val);

	if (!strcasecmp(buf_cstring(&key), "BEGIN")) {
	    struct vcardfast_card *card = NULL;
	    /* shouldn't be any params */
	    if (params) {
		_vcardfast_param_free(params);
		goto fail;
	    }
	    MAKE(card, vcardfast_card);
	    card->type = buf_release(&val);
	    if (!p) goto fail;
	    *cardp = card;
	    p = vcardfast_parse(p, &card, flags);
	}
	else if (!strcasecmp(buf_cstring(&key), "END")) {
	    _vcardfast_param_free(params);
	    if (strcasecmp(buf_cstring(&val), card->type))
		goto fail;
	    /* complete :) - XXX free stuff */
	    return p;
	}
	else {
	    /* it's a parameter on this one */
	    MAKE(entry, vcardfast_entry);
	    entry->name = buf_release(&key);
	    entry->value = buf_release(&val);
	    entry->params = params;
	    *entryp = entry;
	    entryp = &entry->next;
	}
    }

fail:
    buf_free(&key);
    buf_free(&val);
    return NULL;
}
