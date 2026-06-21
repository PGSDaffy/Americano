#ifndef COMPLEMENT_H
#define COMPLEMENT_H

#include "cover.h"

// recursive complement via Shannon expansion: returns OFF-set of F
// nin = number of input variables (indices 0..nin-1)
set_family *complement(set_family *F, int nin);

#endif
