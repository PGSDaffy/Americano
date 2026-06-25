#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pla.h"

#define MAX_LINE 4096

// read next line, skip blanks and comments
static char *next_line(FILE *f, char *buf, int size)
{
    while (fgets(buf, size, f))
    {
        char *p = buf;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p != '\n' && *p != '#' && *p != '\0')
            return buf;
    }
    return NULL;
}

// parse a term like "01-1 10" → input part then output part
static void parse_term(char *line, pset cube, int nin, int nout, int half)
{
    set_clear(cube, half * 2);

    // input part
    for (int v = 0; v < nin && *line; v++)
    {
        while (*line == ' ' || *line == '\t')
            line++;
        int hi = v / 16;
        int lo = hi + half;
        int bit = v % 16;
        if (*line == '0')
        {
            cube[lo] |= (1u << bit);
        }
        else if (*line == '1')
        {
            cube[hi] |= (1u << bit);
        }
        else
        {
            cube[hi] |= (1u << bit);
            cube[lo] |= (1u << bit);
        }
        line++;
    }

    // output part
    for (int v = 0; v < nout && *line; v++)
    {
        while (*line == ' ' || *line == '\t')
            line++;
        if (*line == '\0')
            break;
        int idx = nin + v;
        int hi = idx / 16;
        int bit = idx % 16;
        if (*line == '1')
        {
            cube[hi] |= (1u << bit);
        }
        // '0' or '-' → leave bit 0 (not in this output)
        line++;
    }
}

pla_t *pla_read(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        fprintf(stderr, "cannot open %s\n", filename);
        return NULL;
    }

    char line[MAX_LINE];
    int nin = 0, nout = 0, n_on = 0, n_dc = 0, in_dc = 0;

    // first pass: get .i .o and count ON / DC terms separately
    while (next_line(f, line, MAX_LINE))
    {
        if (line[0] == '.' && line[1] == 'i')
            nin = atoi(line + 2);
        else if (line[0] == '.' && line[1] == 'o')
            nout = atoi(line + 2);
        else if (line[0] == '.' && line[1] == 'd')
            in_dc = 1;
        else if (line[0] != '.')
        {
            if (in_dc) n_dc++; else n_on++;
        }
    }

    if (nin == 0 || nout == 0)
    {
        fprintf(stderr, "missing .i or .o\n");
        fclose(f);
        return NULL;
    }

    int total_vars = nin + nout;
    int half = (total_vars + 15) / 16;
    int nwords = half * 2;

    pla_t *pla = malloc(sizeof(pla_t));
    pla->nin = nin;
    pla->nout = nout;
    pla->cover = cover_new(nwords, n_on > 0 ? n_on : 4);
    pla->dc    = n_dc > 0 ? cover_new(nwords, n_dc) : NULL;

    // second pass: read ON-set and DC-set separately
    rewind(f);
    in_dc = 0;
    while (next_line(f, line, MAX_LINE))
    {
        if (line[0] == '.')
        {
            if (line[1] == 'd') in_dc = 1;
            continue;
        }

        unsigned int buf[nwords];
        parse_term(line, buf, nin, nout, half);
        if (in_dc)
            cover_add(pla->dc, buf);
        else
            cover_add(pla->cover, buf);
    }

    fclose(f);
    return pla;
}

void pla_free(pla_t *p)
{
    cover_free(p->cover);
    if (p->dc) cover_free(p->dc);
    free(p);
}
