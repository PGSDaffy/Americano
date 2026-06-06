#include "espresso.h"

// 枚举所有输入组合，不在 F 里的加入 R
set_family *compute_off_set(set_family *F, int nvars)
{
    int half = (nvars + 15) / 16;
    int nwords = half * 2;
    int total = 1 << nvars;

    set_family *R = cover_new(nwords, total);

    for (int i = 0; i < total; i++)
    {
        unsigned int buf[nwords];
        pset m = buf;
        set_clear(m, nwords);

        // 输入变量
        for (int v = 0; v < nvars; v++)
        {
            int hi = v / 16;
            int lo = hi + half;
            int bit = v % 16;
            if (i & (1 << v))
                m[hi] |= (1u << bit);
            else
                m[lo] |= (1u << bit);
        }

        int covered = 0;
        pset p, last;
        foreach_set(F, last, p)
        {
            if (set_implies(m, p, nwords))
            {
                covered = 1;
                break;
            }
        }
        if (!covered)
            cover_add(R, m);
    }

    return R;
}

// EXPAND: 把每个 cube 尽量变大，但不碰到 OFF-set
set_family *expand(set_family *F, set_family *R, int nin)
{
    int nwords = F->wsize;
    set_family *result = cover_new(nwords, F->count);

    pset p, last;
    foreach_set(F, last, p)
    {
        unsigned int buf[nwords];
        set_copy(buf, p, nwords);

        int half = nwords / 2;
        for (int v = 0; v < nin; v++)
        {
            int hi = v / 16;
            int lo = hi + half;
            int bit = v % 16;
            int h = (buf[hi] >> bit) & 1;
            int l = (buf[lo] >> bit) & 1;

            // 已经是 -（11）就跳过
            if (h == 1 && l == 1)
                continue;

            // 尝试改成 -
            buf[hi] |= (1u << bit);
            buf[lo] |= (1u << bit);

            // 检查是否碰到了 OFF-set
            int ok = 1;
            pset q, qlast;
            foreach_set(R, qlast, q)
            {
                if (set_implies(q, buf, nwords))
                {
                    ok = 0;
                    break;
                }
            }

            if (!ok)
            {
                // 碰到了，改回去
                buf[hi] &= ~(1u << bit);
                buf[lo] &= ~(1u << bit);
                if (h)
                    buf[hi] |= (1u << bit);
                if (l)
                    buf[lo] |= (1u << bit);
            }
        }

        cover_add(result, buf);
    }

    return result;
}

set_family *irredundant(set_family *F, set_family *R)
{
    return cover_dup(F);
}

set_family *reduce(set_family *F, set_family *R)
{
    return cover_dup(F);
}

set_family *espresso_minimize(set_family *F, int nin, int nout)
{
    set_family *R = compute_off_set(F, nin);

    set_family *best = cover_dup(F);
    int last_count;

    for (int iter = 0; iter < 10; iter++)
    {
        last_count = best->count;

        set_family *tmp;
        tmp = expand(best, R, nin);
        cover_free(best);
        best = tmp;

        tmp = irredundant(best, R);
        cover_free(best);
        best = tmp;

        tmp = reduce(best, R);
        cover_free(best);
        best = tmp;

        if (best->count == last_count)
            break;
    }

    cover_free(R);
    return best;
}
