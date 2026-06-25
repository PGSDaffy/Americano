#ifndef PLA_H
#define PLA_H

#include "cover.h"

typedef struct
{
    int nin;  // number of inputs
    int nout; // number of outputs
    set_family *cover;  // ON-set
    set_family *dc;     // don't-care set (NULL if none)
} pla_t;

pla_t *pla_read(const char *filename);
void pla_write(const pla_t *p, FILE *out);
void pla_free(pla_t *p);

#endif
