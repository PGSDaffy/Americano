#ifndef COMPLEMENT_H
#define COMPLEMENT_H

#include "cover.h"

// 递归分治求补：返回 ON-set F 的补集（OFF-set）
// nin = 输入变量数（0..nin-1）
set_family *complement(set_family *F, int nin);

#endif
