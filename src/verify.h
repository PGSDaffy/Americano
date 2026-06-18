#ifndef VERIFY_H
#define VERIFY_H

#include "pla.h"

// 验证 F_new 和 F_old 在 nin 个输入变量上逻辑等价
// 返回 1 表示等价，0 表示不等价并打印第一个不等价的最小项
int verify_equiv(set_family *F_old, set_family *F_new, int nin);

#endif
