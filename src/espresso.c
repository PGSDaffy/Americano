#include "espresso.h"
#include <time.h>

static int trace_on = 0;

void espresso_set_trace(int on) { trace_on = on; }

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
    (void)R;
    int nwords = F->wsize;
    set_family *result = cover_new(nwords, F->count);

    pset p, last;
    foreach_set(F, last, p)
    {
        // 检查 p 是否已被 result 中某个 cube 完全包含
        int keep = 1;
        pset q, qlast;
        foreach_set(result, qlast, q)
        {
            if (set_implies(p, q, nwords))
            {
                keep = 0;
                break;
            }
        }
        if (keep)
            cover_add(result, p);
    }

    return result;
}

// 检查 minterm m 是否被 F_save 中除 exclude 外的 cube 覆盖
static int covered_by_others(pset m, set_family *F_save, pset exclude, int nwords)
{
    pset q, qlast;
    foreach_set(F_save, qlast, q)
    {
        if (q == exclude)
            continue;
        if (set_implies(m, q, nwords))
            return 1;
    }
    return 0;
}

// REDUCE: 缩小每个 cube，但保证整个函数的覆盖不丢失
set_family *reduce(set_family *F, set_family *R, int nin)
{
    (void)R;
    int nwords = F->wsize;
    int half = nwords / 2;
    int total = 1 << nin;

    // 保留原始 F 的一份（后续会修改 result 中的 cube，但需要原始 F 来做覆盖检查）
    set_family *F_save = cover_dup(F);
    set_family *result = cover_dup(F);

    pset p, last;
    foreach_set(result, last, p)
    {
        unsigned int buf[nwords];
        set_copy(buf, p, nwords);

        for (int v = 0; v < nin; v++)
        {
            int hi = v / 16;
            int lo = hi + half;
            int bit = v % 16;
            int h = (buf[hi] >> bit) & 1;
            int l = (buf[lo] >> bit) & 1;

            if (!(h == 1 && l == 1))
                continue;

            // 尝试改成 0
            buf[hi] &= ~(1u << bit);
            int ok = 1;
            for (int i = 0; i < total && ok; i++)
            {
                unsigned int m_buf[nwords];
                pset m = m_buf;
                set_clear(m, nwords);
                for (int j = 0; j < nin; j++)
                {
                    int hh = j / 16, ll = hh + half, bb = j % 16;
                    if (i & (1 << j))
                        m[hh] |= (1u << bb);
                    else
                        m[ll] |= (1u << bb);
                }
                // m 在原始函数里吗？
                if (!covered_by_others(m, F_save, NULL, nwords))
                    continue;
                // m 在新的函数里吗？
                if (!covered_by_others(m, result, p, nwords) &&
                    !set_implies(m, buf, nwords))
                    ok = 0;
            }
            if (ok)
                continue;

            // 尝试改成 1
            buf[hi] |= (1u << bit);
            buf[lo] &= ~(1u << bit);
            ok = 1;
            for (int i = 0; i < total && ok; i++)
            {
                unsigned int m_buf[nwords];
                pset m = m_buf;
                set_clear(m, nwords);
                for (int j = 0; j < nin; j++)
                {
                    int hh = j / 16, ll = hh + half, bb = j % 16;
                    if (i & (1 << j))
                        m[hh] |= (1u << bb);
                    else
                        m[ll] |= (1u << bb);
                }
                if (!covered_by_others(m, F_save, NULL, nwords))
                    continue;
                if (!covered_by_others(m, result, p, nwords) &&
                    !set_implies(m, buf, nwords))
                    ok = 0;
            }
            if (ok)
                continue;

            // 都不行，改回 -
            buf[hi] |= (1u << bit);
            buf[lo] |= (1u << bit);
        }

        set_copy(p, buf, nwords);
    }

    cover_free(F_save);
    return result;
}

set_family *espresso_minimize(set_family *F, int nin, int nout)
{
    clock_t t0 = clock();
    set_family *R = compute_off_set(F, nin);
    (void)nout;

    if (trace_on)
        fprintf(stderr, "# OFF-SET     Time was %.2f ms, cost is c=%d\n",
                (double)(clock() - t0) / CLOCKS_PER_SEC * 1000, F->count);

    set_family *best = cover_dup(F);
    int last_count, iterations = 0;

    for (int iter = 0; iter < 10; iter++)
    {
        last_count = best->count;
        clock_t t1 = clock();

        set_family *tmp;
        tmp = expand(best, R, nin);
        cover_free(best);
        best = tmp;

        tmp = irredundant(best, R);
        cover_free(best);
        best = tmp;

        tmp = reduce(best, R, nin);
        cover_free(best);
        best = tmp;

        iterations++;

        if (trace_on)
            fprintf(stderr, "# ITER %d      Time was %.2f ms, cost is c=%d\n",
                    iter + 1, (double)(clock() - t1) / CLOCKS_PER_SEC * 1000, best->count);

        if (best->count == last_count)
            break;
    }

    if (trace_on)
    {
        fprintf(stderr, "# TOTAL       Time was %.2f ms, iterations=%d\n",
                (double)(clock() - t0) / CLOCKS_PER_SEC * 1000, iterations);
        fprintf(stderr, "# FINAL       cost is c=%d\n", best->count);
    }

    cover_free(R);
    return best;
}
