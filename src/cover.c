#include "cover.h"
#include <string.h>

set_family *cover_new(int nwords, int capacity)
{
    set_family *F = malloc(sizeof(set_family));
    F->wsize = nwords;
    F->count = 0;
    F->capacity = capacity;
    F->data = malloc(nwords * capacity * sizeof(unsigned int));
    return F;
}

set_family *cover_add(set_family *F, pset cube)
{
    if (F->count >= F->capacity) {
        F->capacity *= 2;
        F->data = realloc(F->data,
                          F->wsize * F->capacity * sizeof(unsigned int));
    }
    set_copy(GETSET(F, F->count), cube, F->wsize);
    F->count++;
    return F;
}

void cover_free(set_family *F)
{
    free(F->data);
    free(F);
}

set_family *cover_dup(set_family *F)
{
    set_family *N = cover_new(F->wsize, F->capacity);
    N->count = F->count;
    memcpy(N->data, F->data,
           F->wsize * F->count * sizeof(unsigned int));
    return N;
}
