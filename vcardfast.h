#ifndef VCARDFAST_H
#define VCARDFAST_H

#include <stdlib.h>

struct buf {
    char *s;
    size_t len;
    size_t alloc;
};

#define BUF_INITIALIZER { NULL, 0, 0 }

enum parse_error {
PE_OK = 0,
PE_BACKQUOTE_EOF,
PE_BEGIN_PARAMS,
PE_ENTRY_MULTIGROUP,
PE_FINISHED_EARLY,
PE_KEY_EOF,
PE_KEY_EOL,
PE_MISMATCHED_CARD,
PE_NAME_EOF,
PE_NAME_EOL,
PE_PARAMVALUE_EOF,
PE_PARAMVALUE_EOL,
PE_QSTRING_EOF,
PE_QSTRING_EOL,
PE_NUMERR /* last */
};

struct vcardfast_list {
    char *s;
    struct vcardfast_list *next;
};

struct vcardfast_state {
    struct buf buf;
    const char *base;
    const char *itemstart;
    const char *p;
    struct vcardfast_list *multival;

    /* current items */
    struct vcardfast_card *card;
    struct vcardfast_param *param;
    struct vcardfast_entry *entry;
    struct vcardfast_list *value;
};

struct vcardfast_param {
    char *name;
    char *value;
    struct vcardfast_param *next;
};

struct vcardfast_entry {
    char *group;
    char *name;
    int multivalue;
    union {
	char *value;
	struct vcardfast_list *values;
    } v;
    struct vcardfast_param *params;
    struct vcardfast_entry *next;
};

struct vcardfast_card {
    char *type;
    struct vcardfast_entry *properties;
    struct vcardfast_card *objects;
    struct vcardfast_card *next;
};

struct vcardfast_errorpos {
    int startpos;
    int startline;
    int startchar;
    int errorpos;
    int errorline;
    int errorchar;
};

extern int vcardfast_parse(struct vcardfast_state *state);
extern void vcardfast_free(struct vcardfast_state *state);
extern void vcardfast_fillpos(struct vcardfast_state *state, struct vcardfast_errorpos *pos);
extern const char *vcardfast_errstr(int err);

#endif /* VCARDFAST_H */

