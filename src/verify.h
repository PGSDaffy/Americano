#ifndef VERIFY_H
#define VERIFY_H

#include "pla.h"

// verify F_new is equivalent to F_old on every output.
// DC-set minterms are ignored (any output value is acceptable).
// returns 1 if equivalent, 0 otherwise (prints first mismatch).
int verify_equiv(set_family *F_old, set_family *F_new, set_family *D,
                 int nin, int nout);

#endif
