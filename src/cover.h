#ifndef COVER_H
#define COVER_H

#include "set.h"

set_family *cover_new(int nwords, int capacity);
set_family *cover_add(set_family *F, pset cube);
void cover_free(set_family *F);
set_family *cover_dup(set_family *F);

#endif
