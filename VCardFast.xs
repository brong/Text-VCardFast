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

static struct vcardfast_card *_perl2card(HV *hash)
{
    struct vcardfast_card *card;
    SV **item;

    card = malloc(sizeof(struct vcardfast_card));
    memset(card, 0, sizeof(struct vcardfast_card));

    item = hv_fetch(hash, "type", 4, 0);
    if (item)
	card->type = strdup(SvPV_nolen(*item));

    item = hv_fetch(hash, "properties", 10, 0);
    if (item) {
	
    }
}

MODULE = Text::VCardFast		PACKAGE = Text::VCardFast		

SV*
_vcard2hash(src)
	const char *src;
    PROTOTYPE: $
    CODE:
	HV *hash;
	struct vcardfast_card *res;

	res = vcardfast_parse(src);
	hash = _card2perl(res);
	vcardfast_free(res);

	ST(0) = sv_2mortal(newRV_noinc( (SV *) hash));
	XSRETURN(1);

