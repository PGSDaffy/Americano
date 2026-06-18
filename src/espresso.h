#ifndef ESPRESSO_H
#define ESPRESSO_H

#include "pla.h"

// 全局开关
void espresso_set_trace(int on);

set_family *espresso_minimize(set_family *F, int nin, int nout);
set_family *expand(set_family *F, set_family *R, int nin);
set_family *irredundant(set_family *F, set_family *R);
set_family *reduce(set_family *F, set_family *R, int nin);

#endif
