#ifndef VCARDFAST_H
#define VCARDFAST_H

#include <stdlib.h>

struct vcardfast_param {
    char *name;
    char *value;
    struct vcardfast_param *next;
};

struct vcardfast_entry {
    char *name;
    char *value;
    struct vcardfast_param *param;
    struct vcardfast_entry *next;
};

struct vcardfast_card {
    char *type;
    struct vcardfast_entry *properties;
    struct vcardfast_card *objects;
};

extern struct vcard_card *vcardfast_parse(const char *src);
extern void vcardfast_free(vcard_card *card);

#endif /* VCARDFAST_H */

