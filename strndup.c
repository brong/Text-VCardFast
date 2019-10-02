#ifndef HAVE_STRNDUP
#ifdef _WIN32

#include <stdlib.h>
#include <string.h>

char *strndup(const char *s, size_t n)
{
    char* new = malloc(n+1);
    if (new) {
        strncpy(new, s, n);
        new[n] = '\0';
    }
    return new;
}

#endif
#endif /* HAVE_STRNDUP */
