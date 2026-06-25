#include "espresso.h"
#include "verify.h"
#include <string.h>

int main(int argc, char **argv)
{
    char *filename = NULL;
    char *outfile = NULL;
    int trace = 0;
    int summary = 0;
    int quiet = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-t") == 0)
            trace = 1;
        else if (strcmp(argv[i], "-s") == 0)
            summary = 1;
        else if (strcmp(argv[i], "-q") == 0)
            quiet = 1;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outfile = argv[++i];
        else
            filename = argv[i];
    }

    if (!filename)
    {
        fprintf(stderr, "usage: %s [-t] [-s] [-q] [-o out] <pla file>\n", argv[0]);
        return 1;
    }

    pla_t *p = pla_read(filename);
    if (!p)
        return 1;

    if (trace)
        espresso_set_trace(1);

    int terms_before = p->cover->count;
    set_family *result = espresso_minimize_auto(p->cover, p->nin, p->nout);
    int terms_after = result->count;

    if (!verify_equiv(p->cover, result, p->nin, p->nout))
    {
        fprintf(stderr, "verification failed\n");
        cover_free(result);
        pla_free(p);
        return 1;
    }

    FILE *out = stdout;
    if (outfile)
    {
        out = fopen(outfile, "w");
        if (!out)
        {
            fprintf(stderr, "cannot open %s\n", outfile);
            cover_free(result);
            pla_free(p);
            return 1;
        }
    }

    if (!quiet)
    {
        pla_t out_pla = {.nin = p->nin, .nout = p->nout, .cover = result};
        pla_write(&out_pla, out);
    }

    if (outfile)
        fclose(out);

    if (summary)
    {
        double pct = terms_before ? 100.0 * (terms_before - terms_after) / terms_before : 0.0;
        fprintf(stderr, "# %s: %d terms -> %d terms (%.0f%% reduced)\n",
                filename, terms_before, terms_after, pct);
    }

    cover_free(result);
    pla_free(p);
    return 0;
}
