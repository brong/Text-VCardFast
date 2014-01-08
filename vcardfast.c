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

static const char *_parse_param(const char *src, struct vcardfast_param **paramp)
{
    struct vcardfast_param *param = NULL;
    struct buf key = BUF_INITIALIZER;
    struct buf val = BUF_INITIALIZER;
    const char *p = src;

restart:
    p = _parse_param_key(p, &key);

    /* handle quoted parameters */
    if (*p == '"')
	p = _parse_param_quoted(p+1, &val);

    while (*p) {
        switch (*p) {
	case '\\': /* normal backslash quoting */
	    if (p[1] == 'n' || p[1] == 'N')
		buf_putc(dbuf, "\n");
	    else
		buf_putc(dbuf, p[1]);
	    p += 2;
	    break;
	case '^':
	    if (p[1] == '\'') {
		buf_putc(dbuf, '"');
		p += 2;
	    }
	    else if (p[1] == 'n') {
		buf_putc(dbuf, '\n');
		p += 2;
	    }
	    else {
		buf_putc(dbuf, '^');
		if (p[1] == '^') p += 2;
		else p++;
	    }
	    break;
	case ':':
	    /* done - all parameters parsed */
	    MAKE(param, vcardfast_param);
	    param->name = buf_release(&key);
	    param->value = buf_release(&val);
	    *paramp = param;
	    return p+1;
	    break;
	case ';':
	    /* another parameter to parse */
	    MAKE(param, vcardfast_param);
	    param->name = buf_release(&key);
	    param->value = buf_release(&val);
	    *paramp = param;
	    paramp = &param->next;
	    goto restart;
	case ',':
	    /* multiple values separated by a comma */
	    MAKE(param, vcardfast_param);
	    param->name = strdup(buf_cstring(&key));
	    param->value = buf_release(&val);
	    *paramp = param;
	    paramp = &param->next;
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
	    buf_putc(dbuf, *p++);
	    break;
        }
    }

fail:
    buf_free(&key);
    buf_free(&val);
    return NULL;
}

static const char *_parse_entry_key(const char *src, struct buf dbuf)
{
    const char *p = src;

    while (*p) {
	switch (*p) {
	case ':':
	case ';':
	    return p;
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

/* STATE MAP
 * 0: parsing key
 * 1: param key
 * 2: param value
 */
struct const char *vcardfast_parse(const char *src, struct vcardfast_card *card, int flags)
{
    struct buf buf = BUF_INITIALIZER;
    struct vcardfast_card *sub;
    struct vcardfast_param *param;
    struct vcardfast_entry *entry;
    const char *p;
    int state = 0;

    memset(card, 0, sizeof(struct vcardfast_card));

    while (*p)
	p = _parse_entry_key(p, &buf);
	if (strcmp(buf_cstring(&buf), "BEGIN")) {
	    /* ERROR */
	}
	buf_reset(&buf);
	p = _parse_entry_value(p, &buf);
	if (buf_cstring(
        switch (*p) {
        case ';':
            /* got name into buf, now to parse params */
            MAKE(param, vcardfast_param);
            p = _parse_param(p, param, flags);
            continue;
        case ':':
            /* got entire value */
        }
    }
}

void vcardfast_free(vcardfast_card *card)
{
    struct vcardfast_card *sub, *subnext;;
    struct vcardfast_entry *entry, *entrynext;
    struct vcardfast_param *param, *paramnext;

    free(card->type);

    for (entry = card->properties; entry; entry = entrynext) {
        entrynext = entry->next;
        for (param = entry->params; param; param = paramnext) {
            paramnext = param->next;
            free(param->name);
            free(param->value);
            free(param);
        }
        free(entry->name);
        free(entry->value);
        free(entry);
    }

    for (sub = card->objects; sub; sub = subnext) {
        subnext = sub->next;
        vcardfast_free(sub);
    }

    free(card);
}

#endif /* VCARDFAST_H */

