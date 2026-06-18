#include <stdio.h>
#include "verify.h"

// 检查 minterm m 是否被 cover F 覆盖
static int minterm_covered(pset m, set_family *F, int nwords)
{
    pset p, last;
    foreach_set(F, last, p) if (set_implies(m, p, nwords)) return 1;
    return 0;
}

// 打印一个 minterm 的值（只打印输入部分）
static void print_minterm(pset m, int nvars, int half)
{
    for (int v = 0; v < nvars; v++)
    {
        int hi = v / 16;
        int lo = hi + half;
        int bit = v % 16;
        int h = (m[hi] >> bit) & 1;
        int l = (m[lo] >> bit) & 1;
        if (h == 1 && l == 0)
            fputc('1', stderr);
        else
            fputc('0', stderr);
    }
}

int verify_equiv(set_family *F_old, set_family *F_new, int nin)
{
    int nwords = F_old->wsize;
    int half = nwords / 2;
    int total = 1 << nin;

    for (int i = 0; i < total; i++)
    {
        unsigned int m_buf[nwords];
        pset m = m_buf;
        set_clear(m, nwords);
        for (int v = 0; v < nin; v++)
        {
            int hi = v / 16;
            int lo = hi + half;
            int bit = v % 16;
            if (i & (1 << v))
                m[hi] |= (1u << bit);
            else
                m[lo] |= (1u << bit);
        }

        int in_old = minterm_covered(m, F_old, nwords);
        int in_new = minterm_covered(m, F_new, nwords);

        if (in_old != in_new)
        {
            fprintf(stderr, "verify FAIL at minterm %d (", i);
            print_minterm(m, nin, half);
            fprintf(stderr, "): old=%d new=%d\n", in_old, in_new);
            return 0;
        }
    }

    return 1;
}
