#ifndef SET_H
#define SET_H

#include "types.h"

// 位操作（修改目标 set）
void set_and(pset d, pset a, pset b, int nwords);
void set_or(pset d, pset a, pset b, int nwords);
void set_diff(pset d, pset a, pset b, int nwords);
void set_copy(pset d, pset a, int nwords);
void set_clear(pset s, int nwords);

// 比较（不修改原 set）
int set_equal(pset a, pset b, int nwords);
int set_implies(pset a, pset b, int nwords);
int set_disjoint(pset a, pset b, int nwords);
int set_empty(pset a, int nwords);

// 遍历 cover 里的每个 pset
#define foreach_set(F, last, p)                             \
    for (p = (F)->data, last = p + (F)->count * (F)->wsize; \
         p < last;                                          \
         p += (F)->wsize)

#endif
