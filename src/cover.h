#ifndef COVER_H
#define COVER_H

#include "set.h"

// 创建一个空 cover，预分配 capacity 个位置
set_family *cover_new(int nwords, int capacity);

// 往 cover 末尾加一个 cube
set_family *cover_add(set_family *F, pset cube);

// 释放 cover
void cover_free(set_family *F);

// 深拷贝
set_family *cover_dup(set_family *F);

// 释放 cover 但不释放里面的 cube 数据（给别人用）
void cover_free_data(set_family *F);

#endif
