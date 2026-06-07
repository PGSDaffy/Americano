#ifndef ESPRESSO_H
#define ESPRESSO_H

#include "pla.h"

set_family *espresso_minimize(set_family *F, int nin, int nout);

// 求 ON-set 的补集（OFF-set），暴力枚举所有输入组合
set_family *compute_off_set(set_family *F, int nvars);

// 三个核心步骤
set_family *expand(set_family *F, set_family *R, int nin);
set_family *irredundant(set_family *F, set_family *R);
set_family *reduce(set_family *F, set_family *R, int nin);

#endif
