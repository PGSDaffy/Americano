#ifndef PLA_H
#define PLA_H

#include "cover.h"

typedef struct
{
    int nin;  // 输入变量数
    int nout; // 输出变量数
    set_family *cover;
} pla_t;

pla_t *pla_read(const char *filename);
void pla_free(pla_t *p);

#endif
