#include <stdio.h>
#include "verify.h"

// 检查 minterm m 是否被 cover F 中某个 cube 覆盖，且该 cube 的输出 out_idx = 1
static int minterm_covered_output(pset m, set_family *F, int out_idx,
                                  int nin, int nwords)
{
    pset p, last;
    foreach_set(F, last, p) {
        if (set_implies(m, p, nwords) && set_has_output(p, out_idx, nin))
            return 1;
    }
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

int verify_equiv(set_family *F_old, set_family *F_new, int nin, int nout)
{
    int nwords = F_old->wsize;
    int half = nwords / 2;
    int total = 1 << nin;

    for (int i = 0; i < total; i++)
    {
        unsigned int m_buf[128];
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

        for (int o = 0; o < nout; o++)
        {
            int in_old = minterm_covered_output(m, F_old, o, nin, nwords);
            int in_new = minterm_covered_output(m, F_new, o, nin, nwords);

            if (in_old != in_new)
            {
                fprintf(stderr, "verify FAIL at minterm %d (", i);
                print_minterm(m, nin, half);
                fprintf(stderr, ") output %d: old=%d new=%d\n", o, in_old, in_new);
                return 0;
            }
        }
    }

    return 1;
}
