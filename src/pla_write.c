#include <stdio.h>
#include "pla.h"

// decode one variable: 01→'0', 10→'1', 11→'-'
static char var_char(pset cube, int v, int half)
{
    int hi = v / 16;
    int lo = hi + half;
    int bit = v % 16;
    int h = (cube[hi] >> bit) & 1;
    int l = (cube[lo] >> bit) & 1;
    if (h == 0 && l == 1)
        return '0';
    if (h == 1 && l == 0)
        return '1';
    return '-';
}

void pla_write(const pla_t *p, FILE *out)
{
    int total = p->nin + p->nout;
    int half = (total + 15) / 16;

    fprintf(out, ".i %d\n", p->nin);
    fprintf(out, ".o %d\n", p->nout);
    if (p->cover->count > 0)
        fprintf(out, ".p %d\n", p->cover->count);

    pset cube, last;
    foreach_set(p->cover, last, cube)
    {
        for (int v = 0; v < p->nin; v++)
            fputc(var_char(cube, v, half), out);
        fputc(' ', out);
        for (int v = 0; v < p->nout; v++)
        {
            int hi = (p->nin + v) / 16;
            int bit = (p->nin + v) % 16;
            int b = (cube[hi] >> bit) & 1;
            fputc(b ? '1' : '0', out);
        }
        fputc('\n', out);
    }

    fprintf(out, ".e\n");
}
