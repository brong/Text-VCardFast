#ifndef VCARDFAST_H
#define VCARDFAST_H

#include <stdlib.h>

struct buf {
    char *s;
    size_t len;
    size_t alloc;
};

#define BUF_INITIALIZER { NULL, 0, 0 }

struct vcardfast_state {
    struct buf buf;
    const char *base;
    const char *itemstart;
    const char *p;
    char *key;
    char *val;
};

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

extern struct vcardfast_card *vcardfast_parse(struct vcardfast_state *state, int flags);
extern char *vcardfast_gen(const struct vcardfast_card *src, int flags);
extern void vcardfast_free(struct vcardfast_card *card);

#define MAKE(X, Y) X = malloc(sizeof(struct Y)); memset(X, 0, sizeof(struct Y))

#endif /* VCARDFAST_H */

