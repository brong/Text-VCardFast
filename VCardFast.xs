#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

struct vcard_param {
    char *name;
    char *value;
    struct vcard_param *next;
}

struct vcard_entry {
    char *name;
    char *value;
    struct vcard_param *param;
    struct vcard_entry *next;
}

struct vcard_

MODULE = Text::VCardFast		PACKAGE = Text::VCardFast		

