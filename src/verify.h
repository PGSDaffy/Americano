#ifndef VERIFY_H
#define VERIFY_H

#include "pla.h"

// verify F_new is equivalent to F_old on every output.
// returns 1 if equivalent, 0 otherwise (prints first mismatch).
int verify_equiv(set_family *F_old, set_family *F_new, int nin, int nout);

#endif
