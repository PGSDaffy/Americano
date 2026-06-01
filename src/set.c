#include "set.h"
#include <string.h>

void set_and(pset d, pset a, pset b, int nwords)
{
    for (int i = 0; i < nwords; i++)
        d[i] = a[i] & b[i];
}

void set_or(pset d, pset a, pset b, int nwords)
{
    for (int i = 0; i < nwords; i++)
        d[i] = a[i] | b[i];
}

void set_diff(pset d, pset a, pset b, int nwords)
{
    for (int i = 0; i < nwords; i++)
        d[i] = a[i] & ~b[i];
}

void set_copy(pset d, pset a, int nwords)
{
    memcpy(d, a, nwords * sizeof(unsigned int));
}

void set_clear(pset s, int nwords)
{
    memset(s, 0, nwords * sizeof(unsigned int));
}

int set_equal(pset a, pset b, int nwords)
{
    for (int i = 0; i < nwords; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

int set_implies(pset a, pset b, int nwords)
{
    // a 的所有 1 位，在 b 里也必须是 1
    for (int i = 0; i < nwords; i++)
        if ((a[i] & b[i]) != a[i])
            return 0;
    return 1;
}

int set_disjoint(pset a, pset b, int nwords)
{
    // 有任一位同时为 1 就不算 disjoint
    for (int i = 0; i < nwords; i++)
        if (a[i] & b[i])
            return 0;
    return 1;
}

int set_empty(pset a, int nwords)
{
    for (int i = 0; i < nwords; i++)
        if (a[i] != 0)
            return 0;
    return 1;
}
