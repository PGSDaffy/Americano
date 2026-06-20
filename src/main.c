#include "espresso.h"
#include "verify.h"
#include <string.h>

int main(int argc, char **argv)
{
    char *filename = NULL;
    int trace = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-t") == 0)
            trace = 1;
        else
            filename = argv[i];
    }

    if (!filename)
    {
        fprintf(stderr, "usage: %s [-t] <pla file>\n", argv[0]);
        return 1;
    }

    pla_t *p = pla_read(filename);
    if (!p)
        return 1;

    if (trace)
        espresso_set_trace(1);

    set_family *result = espresso_minimize_multi(p->cover, p->nin, p->nout);

    if (!verify_equiv(p->cover, result, p->nin, p->nout))
    {
        fprintf(stderr, "verification failed\n");
        cover_free(result);
        pla_free(p);
        return 1;
    }

    pla_t out = {.nin = p->nin, .nout = p->nout, .cover = result};
    pla_write(&out, stdout);

    cover_free(result);
    pla_free(p);
    return 0;
}
