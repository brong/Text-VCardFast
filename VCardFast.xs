#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "vcardfast.h"

// hv_store to array, create if not exists - from XML::Fast 0.11
#define hv_store_aa( hv, kv, kl, sv ) \
	STMT_START { \
		SV **exists; \
		if( ( exists = hv_fetch(hv, kv, kl, 0) ) && SvROK(*exists) && (SvTYPE( SvRV(*exists) ) == SVt_PVAV) ) { \
			AV *av = (AV *) SvRV( *exists ); \
			av_push( av, sv ); \
		} \
		else { \
			AV *av   = newAV(); \
			av_push( av, sv ); \
			(void) hv_store( hv, kv, kl, newRV_noinc( (SV *) av ), 0 ); \
		} \
	} STMT_END


static HV *_card2perl(struct vcardfast_card *card)
{
    struct vcardfast_card *sub;
    struct vcardfast_entry *entry;
    HV *res = newHV();
    HV *prophash = newHV();

    if (card->type) {
	hv_store(res, "type", 4, newSVpv(card->type, 0), 0);
	hv_store(res, "properties", 10, newRV_noinc( (SV *) prophash), 0);
    }

    if (card->objects) {
	AV *objarray = newAV();
	hv_store(res, "objects", 7, newRV_noinc( (SV *) objarray), 0);
	for (sub = card->objects; sub; sub = sub->next) {
	    HV *child = _card2perl(sub);
	    av_push(objarray, newRV_noinc( (SV *) child));
	}
    }

    for (entry = card->properties; entry; entry = entry->next) {
	HV *item = newHV();
	size_t len = strlen(entry->name);
	hv_store(item, "value", 5, newSVpv(entry->v.value, 0), 0);
	if (entry->params) {
	    struct vcardfast_param *param;
	    HV *prop = newHV();
	    for (param = entry->params; param; param = param->next) {
		hv_store(prop, param->name, strlen(param->name), newSVpv(param->value, 0), 0);
	    }
	    hv_store(item, "param", 5, newRV_noinc( (SV *) prop), 0);
	}
	hv_store_aa(prophash, entry->name, len, newRV_noinc( (SV *) item));
    }

    return res;
}

static void _die_error(struct vcardfast_state *state, int err)
{
    struct vcardfast_errorpos pos;
    const char *src = state->base;

    vcardfast_fillpos(state, &pos);
    /* everything points into src now */
    vcardfast_free(state);

    if (pos.startpos <= 60) {
	int len = pos.errorpos - pos.startpos;
	croak("error %s at line %d char %d: %.*s ---> %.*s <---",
	  vcardfast_errstr(err), pos.errorline, pos.errorchar,
	  pos.startpos, src, len, src + pos.startpos);
    }
    if (pos.errorpos - pos.startpos < 40) {
	int len = pos.errorpos - pos.startpos;
	croak("error %s at line %d char %d: ... %.*s ---> %.*s <---",
	  vcardfast_errstr(err), pos.errorline, pos.errorchar,
	  40 - len, src + pos.errorpos - 40,
	  len, src + pos.startpos);
    }
    croak("error %s at line %d char %d: %.*s ... %.*s <--- (started at line %d char %d)",
	  vcardfast_errstr(err), pos.errorline, pos.errorchar,
	  20, src + pos.startpos,
	  20, src + pos.errorpos - 20,
	  pos.startline, pos.startchar);
}

MODULE = Text::VCardFast		PACKAGE = Text::VCardFast		

SV*
_vcard2hash(src)
	const char *src;
    PROTOTYPE: $
    CODE:
	HV *hash;
	struct vcardfast_state parser;
	int r;

	memset(&parser, 0, sizeof(struct vcardfast_state));

	parser.base = src;

	r = vcardfast_parse(&parser);

	if (r)
	    _die_error(&parser, r);

	hash = _card2perl(parser.card);
	vcardfast_free(&parser);

	ST(0) = sv_2mortal(newRV_noinc( (SV *) hash));
	XSRETURN(1);

