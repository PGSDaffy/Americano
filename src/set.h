#ifndef SET_H
#define SET_H

#include "types.h"

// bitwise ops (modify dest)
void set_and(pset d, pset a, pset b, int nwords);
// void set_or(pset d, pset a, pset b, int nwords);
// void set_diff(pset d, pset a, pset b, int nwords);
void set_copy(pset d, pset a, int nwords);
void set_clear(pset s, int nwords);

// comparisons (read-only)
// int set_equal(pset a, pset b, int nwords);
int set_implies(pset a, pset b, int nwords);
// int set_disjoint(pset a, pset b, int nwords);
int set_empty(pset a, int nwords);

// per-variable access
int set_get_var(pset s, int v, int half);
void set_set_var_dc(pset s, int v, int half);
void set_force_var(pset s, int v, int val, int half);
int set_var_phase(pset s, int v, int half);

// do the two cubes share at least one minterm?
int set_intersect(pset a, pset b, int nin, int nwords);

// check if output bit out_idx is 1
int set_has_output(pset s, int out_idx, int nin);

// iterate over every cube in a set_family
#define foreach_set(F, last, p)                             \
    for (p = (F)->data, last = p + (F)->count * (F)->wsize; \
         p < last;                                          \
         p += (F)->wsize)

#endif
