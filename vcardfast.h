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
    struct vcardfast_param *params;
    struct vcardfast_entry *next;
};

struct vcardfast_card {
    char *type;
    struct vcardfast_entry *properties;
    struct vcardfast_card *objects;
    struct vcardfast_card *next;
};

extern struct vcardfast_card *vcardfast_parse(const char *src, int flags);
extern void vcardfast_free(struct vcardfast_card *card);

#endif /* VCARDFAST_H */

