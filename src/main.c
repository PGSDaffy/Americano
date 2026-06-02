#include "pla.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: %s <pla file>\n", argv[0]);
        return 1;
    }

    pla_t *p = pla_read(argv[1]);
    if (!p)
        return 1;

    printf("inputs: %d, outputs: %d, terms: %d\n",
           p->nin, p->nout, p->cover->count);

    pla_free(p);
    return 0;
}
