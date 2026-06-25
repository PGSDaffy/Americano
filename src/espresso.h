#ifndef ESPRESSO_H
#define ESPRESSO_H

#include "pla.h"

// global toggle for trace output
void espresso_set_trace(int on);
void espresso_set_verbose(int on);

set_family *espresso_minimize(set_family *F, set_family *D, int nin, int nout);
set_family *espresso_minimize_multi(set_family *F, set_family *D, int nin, int nout);
set_family *espresso_minimize_auto(set_family *F, set_family *D, int nin, int nout);
set_family *expand(set_family *F, set_family *R, int nin);
set_family *irredundant(set_family *F, set_family *R, int nin);
set_family *reduce(set_family *F, set_family *R, int nin);

#endif
