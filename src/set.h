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

// ── 变量级操作 ────────────────────────────────────
int  set_get_var(pset s, int v, int half);
void set_set_var_dc(pset s, int v, int half);
void set_force_var(pset s, int v, int val, int half);
int  set_var_phase(pset s, int v, int half);

// 检查两个 cube 是否共享至少一个 minterm（逐变量兼容性）
int  set_intersect(pset a, pset b, int nin, int nwords);

// 遍历 cover 里的每个 pset
#define foreach_set(F, last, p)                             \
    for (p = (F)->data, last = p + (F)->count * (F)->wsize; \
         p < last;                                          \
         p += (F)->wsize)

#endif
