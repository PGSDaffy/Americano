#include "pla.h"
#include <string.h>
#include <ctype.h>

#define MAX_LINE 4096

// 逐行读，跳过空白行和注释
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

// 解析一行数据: "01-1 10" → 前半段是输入，后半段是输出
static void parse_term(char *line, pset cube, int nin, int nout, int half)
{
    set_clear(cube, half * 2);

    // 输入部分
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

    // 输出部分
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
        // '0' 或 '-' 都不设置（保持 0 = 不属于这个输出）
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
    int nin = 0, nout = 0, nterms = 0;

    // 第一遍扫描：获取 .i .o
    while (next_line(f, line, MAX_LINE))
    {
        if (line[0] == '.' && line[1] == 'i')
            nin = atoi(line + 2);
        else if (line[0] == '.' && line[1] == 'o')
            nout = atoi(line + 2);
        else if (line[0] == '.' && line[1] != 'i' && line[1] != 'o')
            continue; // 跳过 .p .e 等关键字
        else if (nin > 0 && nout > 0)
            nterms++; // 数据行
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
    pla->cover = cover_new(nwords, nterms > 0 ? nterms : 4);

    // 第二遍：读取数据行
    rewind(f);
    while (next_line(f, line, MAX_LINE))
    {
        if (line[0] == '.')
            continue;

        unsigned int buf[nwords];
        parse_term(line, buf, nin, nout, half);
        cover_add(pla->cover, buf);
    }

    fclose(f);
    return pla;
}

void pla_free(pla_t *p)
{
    cover_free(p->cover);
    free(p);
}
