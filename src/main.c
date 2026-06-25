#include "espresso.h"
#include "verify.h"
#include <string.h>

static int process_one(const char *filename, const char *outfile,
                       int trace, int verbose, int summary, int quiet)
{
    pla_t *p = pla_read(filename);
    if (!p) return 1;

    if (trace)   espresso_set_trace(1);
    if (verbose) espresso_set_verbose(1);

    int terms_before = p->cover->count;
    set_family *result = espresso_minimize_auto(p->cover, p->dc, p->nin, p->nout);
    int terms_after = result->count;

    if (!verify_equiv(p->cover, result, p->dc, p->nin, p->nout))
    {
        fprintf(stderr, "%s: verification failed\n", filename);
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

    if (outfile) fclose(out);

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

int main(int argc, char **argv)
{
    char *outfile = NULL;
    int trace = 0, verbose = 0, summary = 0, quiet = 0;
    int nfiles = 0;

    // collect filenames (non-flag args)
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-t") == 0)
            trace = 1;
        else if (strcmp(argv[i], "-v") == 0)
            verbose = 1;
        else if (strcmp(argv[i], "-s") == 0)
            summary = 1;
        else if (strcmp(argv[i], "-q") == 0)
            quiet = 1;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outfile = argv[++i];
        else
            nfiles++;
    }

    if (nfiles == 0)
    {
        fprintf(stderr, "usage: %s [-t] [-v] [-s] [-q] [-o out] <file.pla>...\n", argv[0]);
        return 1;
    }

    if (outfile && nfiles > 1)
    {
        fprintf(stderr, "warning: -o with multiple files, only last file written to %s\n", outfile);
    }

    int errors = 0;
    for (int i = 1; i < argc; i++)
    {
        // skip flags
        if (argv[i][0] == '-')
        {
            if (strcmp(argv[i], "-o") == 0) i++; // skip value too
            continue;
        }

        const char *out = (nfiles > 1) ? NULL : outfile;
        if (process_one(argv[i], out, trace, verbose, summary, quiet))
            errors++;
    }

    return errors ? 1 : 0;
}
