/* vcardfast.c : fast vcard parser */

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "vcardfast.h"

struct buf {
    char *s;
    size_t len;
    size_t alloc;
};

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

static void buf_appendmap(struct buf *buf, const char *base, size_t len)
{
    if (len) {
        buf_ensure(buf, len);
        memcpy(buf->s + buf->len, base, len);
        buf->len += len;
    }
}

static void buf_appendcstr(struct buf *buf, const char *str)
{
    buf_appendmap(buf, str, strlen(str));
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
	    p++;
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
	    p++;
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
		buf_putc(&val, '\n');
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
	    p++;
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

static const char *_parse_entry_key(const char *src, struct buf *dbuf, struct vcardfast_param **paramp)
{
    const char *p = src;

    buf_reset(dbuf);
    *paramp = NULL;

    while (*p) {
	switch (*p) {
	case ':':
	    return p+1;
	case ';':
	    return _parse_entry_params(p+1, paramp);

	case '\r':
	    p++;
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

static const char *_parse_entry_value(const char *src, struct buf *dbuf)
{
    const char *p = src;

    buf_reset(dbuf);

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
	    p++;
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

void vcardfast_free(struct vcardfast_card *card)
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

static const char *_parse_vcard(const char *src, struct vcardfast_card *card, int flags)
{
    struct buf key = BUF_INITIALIZER;
    struct buf val = BUF_INITIALIZER;
    struct vcardfast_param *params = NULL;
    struct vcardfast_card **subp = &card->objects;
    struct vcardfast_entry **entryp = &card->properties;
    const char *p = src;

    while (*p) {
	/* skip blank lines */
	if (*p == '\r' || *p == '\n') {
	    p++;
	    continue;
	}

	p = _parse_entry_key(p, &key, &params);
	if (!p) goto fail;
	p = _parse_entry_value(p, &val);
	if (!p) goto fail;

	if (!strcasecmp(buf_cstring(&key), "BEGIN")) {
	    struct vcardfast_card *sub = NULL;
	    /* shouldn't be any params */
	    if (params) goto fail;
	    MAKE(sub, vcardfast_card);
	    sub->type = buf_release(&val);
	    p = _parse_vcard(p, sub, flags);
	    if (!p) goto fail;
	    *subp = sub;
	    subp = &sub->next;
	}
	else if (!strcasecmp(buf_cstring(&key), "END")) {
	    if (params) goto fail;
	    if (strcasecmp(buf_cstring(&val), card->type))
		goto fail;
	    /* complete :) - XXX free stuff */
	    buf_free(&key);
	    buf_free(&val);
	    return p;
	}
	else {
	    struct vcardfast_entry *entry = NULL;
	    /* it's a parameter on this one */
	    MAKE(entry, vcardfast_entry);
	    entry->name = buf_release(&key);
	    entry->params = params;
	    params = NULL;
	    entry->value = buf_release(&val);
	    *entryp = entry;
	    entryp = &entry->next;
	}
    }

    if (!card->type) {
	/* we're done! */
	buf_free(&key);
	buf_free(&val);
	return p;
    }

fail:
    _vcardfast_param_free(params);
    buf_free(&key);
    buf_free(&val);
    return NULL;
}

struct vcardfast_card *vcardfast_parse(const char *src, int flags)
{
    const char *p = src;
    struct vcardfast_card *card = NULL;
    MAKE(card, vcardfast_card);

    p = _parse_vcard(src, card, flags);
    if (!p) goto fail;

    /* XXX - check for trailing non-whitespace? */

    return card;

fail:
    vcardfast_free(card);
    return NULL;
}

static int _key2buf(struct buf *dbuf, int *linestartp, const char *src)
{
    const char *p;
    size_t len = strlen(src);

    /* wrap early if a key would be broken during the output */
    if (len < 68 && len + dbuf->len - *linestartp > 70) {
	buf_putc(dbuf, '\n');
	*linestartp = dbuf->len;
	buf_putc(dbuf, ' ');
    }

    for (p = src; *p; p++) {
	if (dbuf->len - *linestartp > 70 && (*p >= 0xc0 || *p < 0x80)) {
	    /* not a continuation character */
	    buf_putc(dbuf, '\n');
	    *linestartp = dbuf->len;
	    buf_putc(dbuf, ' ');
	}
	if (*p >= 'a' && *p <= 'z') {
	    buf_putc(dbuf, *p - 'a' + 'A');
	}
	else {
	    buf_putc(dbuf, *p); /* no quoting in keys, they're alphanum */
	}
    }
}

static int _value2buf(struct buf *dbuf, int *linestartp, const char *src)
{
    const char *p;

    for (p = src; *p; p++) {
	if (dbuf->len - *linestartp > 70 && (*p >= 0xc0 || *p < 0x80)) {
	    /* not a continuation character */
	    buf_putc(dbuf, '\n');
	    *linestartp = dbuf->len;
	    buf_putc(dbuf, ' ');
	}
	switch (*p) {
	case '\r':
	    break; /* we just don't create these at all */
	case '\n':
	    buf_putc(dbuf, '\\');
	    buf_putc(dbuf, 'n'); /* we have to handle \N, but we */
	    break;               /* don't have to generate it    */
	case '\\':
	case ',':
	    buf_putc(dbuf, '\\');
	    /* fall through */
	default:
	    buf_putc(dbuf, *p);
	    break;
	}
    }
}

static int needsquote(const char *val)
{
    const char *p;

    for (p = val; *p; p++) {
	switch (*p) {
	case ',':
	case ';':
	case ':':
	    return 1;
	default:
	    break;
	}
    }

    return 0;
}

static int _attrval2buf(struct buf *dbuf, int *linestartp, const char *src)
{
    const char *p;
    int isquote = needsquote(src);

    if (isquote)
	buf_putc(dbuf, '"');

    for (p = src; *p; p++) {
	if (dbuf->len - *linestartp > 70 && (*p >= 0xc0 || *p < 0x80)) {
	    /* not a continuation character */
	    buf_putc(dbuf, '\n');
	    *linestartp = dbuf->len;
	    buf_putc(dbuf, ' ');
	}
	switch (*p) {
	case '\r':
	    break; /* we just don't create these at all */
	case '\n':
	    buf_putc(dbuf, '^');
	    buf_putc(dbuf, 'n');
	    break;
	case '^':
	    buf_putc(dbuf, '^');
	    buf_putc(dbuf, '^');
	    break;
	case '"':
	    buf_putc(dbuf, '^');
	    buf_putc(dbuf, '\'');
	    break;
	default:
	    buf_putc(dbuf, *p);
	    break;
	}
    }

    if (isquote)
	buf_putc(dbuf, '"');

    return 0;
}

static int _param2buf(struct buf *dbuf, int *linestartp, struct vcardfast_param *param)
{
    int r = 0;

    buf_putc(dbuf, ';');
    r = _key2buf(dbuf, linestartp, param->name);
    if (r) return r;
    buf_putc(dbuf, '=');
    r = _attrval2buf(dbuf, linestartp, param->value);
    if (r) return r;

    return 0;
}

static int _entry2buf(struct buf *dbuf, struct vcardfast_entry *entry, int flags)
{
    struct buf entrybuf = BUF_INITIALIZER;
    struct vcardfast_param *param;
    int linestart = dbuf->len;
    int r = 0;

    r = _key2buf(dbuf, &linestart, entry->name);
    if (r) return r;

    for (param = entry->params; param; param = param->next) {
	r = _param2buf(dbuf, &linestart, param);
	if (r) return r;
    }

    buf_putc(&entrybuf, ':');
    r = _value2buf(dbuf, &linestart, entry->value);
    if (r) return r;
    buf_putc(dbuf, '\n');

    return r;
}

static int _card2buf(struct buf *dbuf, struct vcardfast_card *card, int flags)
{
    struct vcardfast_entry *entry;
    struct vcardfast_card *sub;
    int r = 0;

    buf_appendmap(dbuf, "BEGIN:", 6);
    buf_appendcstr(dbuf, card->type);
    buf_putc(dbuf, '\n');

    for (entry = card->properties; entry; entry = entry->next) {
	r = _entry2buf(dbuf, entry, flags);
	if (r) goto done;
    }

    for (sub = card->objects; sub; sub = sub->next) {
	r = _card2buf(dbuf, sub, flags);
	if (r) goto done;
    }

    buf_appendmap(dbuf, "END:", 4);
    buf_appendcstr(dbuf, card->type);
    buf_putc(dbuf, '\n');

done:
    return r;
}

char *vcardfast_gen(struct vcardfast_card *card, int flags)
{
    struct buf buf = BUF_INITIALIZER;

    int r = _card2buf(&buf, card->objects, flags);

    return buf_release(&buf);
}
