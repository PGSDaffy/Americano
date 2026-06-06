#include "espresso.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <pla file>\n", argv[0]);
        return 1;
    }

    pla_t *p = pla_read(argv[1]);
    if (!p)
        return 1;

    set_family *result = espresso_minimize(p->cover, p->nin, p->nout);

    pla_t out = {.nin = p->nin, .nout = p->nout, .cover = result};
    pla_write(&out, stdout);

    cover_free(result);
    pla_free(p);
    return 0;
}
