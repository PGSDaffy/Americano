#include "espresso.h"
#include "complement.h"
#include "matrix.h"
#include <time.h>

static int trace_on = 0;

void espresso_set_trace(int on) { trace_on = on; }

// expand each cube as much as possible without touching the OFF-set
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

            // already DC, skip
            if (h == 1 && l == 1)
                continue;

            // try raising to DC
            buf[hi] |= (1u << bit);
            buf[lo] |= (1u << bit);

            // check if we hit the OFF-set (any intersection means hit)
            int ok = 1;
            pset q, qlast;
            foreach_set(R, qlast, q)
            {
                if (set_intersect(q, buf, nin, nwords))
                {
                    ok = 0;
                    break;
                }
            }

            if (!ok)
            {
                // hit the OFF-set, revert
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

// enumerate all ON-set minterms (walk all 2^nin input combos)
static pset *enumerate_on_set(set_family *F, int nin, int *num_on)
{
    int nwords = F->wsize, half = nwords / 2, total = 1 << nin;
    int cap = (total < 256) ? total : 256;
    pset *on = (pset *)malloc((size_t)cap * sizeof(pset));
    int cnt = 0;

    for (int i = 0; i < total; i++)
    {
        /* build minterm i on the stack */
        unsigned int m_buf[128]; /* 128 ints → 1024 variables max */
        pset m = m_buf;
        set_clear(m, nwords);
        for (int v = 0; v < nin; v++)
        {
            int hi = v / 16, lo = hi + half, bit = v % 16;
            if (i & (1 << v))
                m[hi] |= (1u << bit);
            else
                m[lo] |= (1u << bit);
        }

        /* check coverage */
        pset p, last;
        foreach_set(F, last, p)
        {
            if (set_implies(m, p, nwords))
            {
                if (cnt >= cap)
                {
                    cap *= 2;
                    on = (pset *)realloc(on, (size_t)cap * sizeof(pset));
                }
                on[cnt] = (pset)malloc((size_t)nwords * sizeof(unsigned int));
                set_copy(on[cnt], m, nwords);
                cnt++;
                break;
            }
        }
    }
    *num_on = cnt;
    return on;
}

static void free_on_minterms(pset *on, int num_on)
{
    for (int i = 0; i < num_on; i++)
        free(on[i]);
    free(on);
}

// build the sparse covering matrix M[row=minterm][col=cube] = 1 if covered
static sm_matrix *build_cover_matrix(pset *on_minterms, int num_on,
                                     set_family *F)
{
    int nwords = F->wsize;
    sm_matrix *M = matrix_alloc();
    if (num_on > 0 && F->count > 0)
        matrix_resize(M, num_on - 1, F->count - 1);

    for (int i = 0; i < num_on; i++)
    {
        for (int j = 0; j < F->count; j++)
        {
            if (set_implies(on_minterms[i], GETSET(F, j), nwords))
                matrix_insert(M, i, j);
        }
    }
    return M;
}

// can we shrink cube col_j without losing any minterm that depends solely on it?
static int reduce_check(sm_matrix *M, int col_j, pset reduced,
                        pset *on_minterms, int nwords)
{
    sm_col *pcol = matrix_get_col(M, col_j);
    if (pcol == NULL)
        return 1; /* cube already covers nothing */

    sm_element *e;
    sm_foreach_col_element(pcol, e)
    {
        int row_i = e->row_num;
        if (matrix_row_count(M, row_i) == 1)
        {
            /* sole cover — must still match */
            if (!set_implies(on_minterms[row_i], reduced, nwords))
                return 0;
        }
    }
    return 1;
}

// after shrinking cube col_j, remove any minterms it no longer covers
static void update_matrix(sm_matrix *M, int col_j, pset new_cube,
                          pset *on_minterms, int nwords)
{
    sm_col *pcol = matrix_get_col(M, col_j);
    if (pcol == NULL)
        return;

    sm_element *e, *enext;
    for (e = pcol->first_row; e != NULL; e = enext)
    {
        enext = e->next_row;
        if (!set_implies(on_minterms[e->row_num], new_cube, nwords))
            matrix_remove(M, e->row_num, col_j);
    }
}

// irredundant: keep a cube only if it covers a minterm not already covered by larger cubes.
// Cubes are processed largest-first.

set_family *irredundant(set_family *F, set_family *R, int nin)
{
    (void)R;
    int nwords = F->wsize;

    int num_on;
    pset *on_minterms = enumerate_on_set(F, nin, &num_on);
    if (num_on == 0)
    {
        free(on_minterms);
        set_family *result = cover_new(nwords, 1);
        result->count = 0;
        return result;
    }

    sm_matrix *M = build_cover_matrix(on_minterms, num_on, F);

    // sort cubes by coverage size, largest first
    int *order = (int *)malloc((size_t)F->count * sizeof(int));
    for (int j = 0; j < F->count; j++)
        order[j] = j;
    for (int a = 0; a < F->count; a++)
    {
        int best = a;
        for (int b = a + 1; b < F->count; b++)
            if (matrix_col_count(M, order[b]) > matrix_col_count(M, order[best]))
                best = b;
        int tmp = order[a];
        order[a] = order[best];
        order[best] = tmp;
    }

    int *covered_cnt = (int *)calloc((size_t)num_on, sizeof(int));
    int *keep = (int *)calloc((size_t)F->count, sizeof(int));

    set_family *result = cover_new(nwords, F->count > 0 ? F->count : 1);
    result->count = 0;

    for (int idx = 0; idx < F->count; idx++)
    {
        int j = order[idx];
        sm_col *pcol = matrix_get_col(M, j);
        if (pcol == NULL)
            continue; /* cube covers nothing */

        // redundant if every minterm it covers is already covered by a kept cube
        int redundant = 1;
        sm_element *e;
        sm_foreach_col_element(pcol, e)
        {
            if (covered_cnt[e->row_num] == 0)
            {
                redundant = 0;
                break;
            }
        }

        if (!redundant)
        {
            keep[j] = 1;
            cover_add(result, GETSET(F, j));
            sm_foreach_col_element(pcol, e)
                covered_cnt[e->row_num]++;
        }
    }

    free(order);
    free(covered_cnt);
    free(keep);
    matrix_free(M);
    free_on_minterms(on_minterms, num_on);
    return result;
}

// reduce: shrink each cube without losing any ON-set coverage.
// Builds a sparse covering matrix once, then only checks "essential" minterms.

set_family *reduce(set_family *F, set_family *R, int nin)
{
    (void)R;
    int nwords = F->wsize, half = nwords / 2;

    set_family *F_save = cover_dup(F);
    set_family *result = cover_dup(F);

    // 1. enumerate the ON-set and build the coverage matrix
    int num_on;
    pset *on_minterms = enumerate_on_set(F_save, nin, &num_on);
    if (num_on == 0)
    {
        free_on_minterms(on_minterms, num_on);
        cover_free(F_save);
        return result;
    }

    sm_matrix *M = build_cover_matrix(on_minterms, num_on, result);

    // 2. shrink each cube in place
    for (int j = 0; j < result->count; j++)
    {
        pset p = GETSET(result, j);
        unsigned int buf[128];
        set_copy(buf, p, nwords);

        for (int v = 0; v < nin; v++)
        {
            int hi = v / 16, lo = hi + half, bit = v % 16;
            int h = (buf[hi] >> bit) & 1;
            int l = (buf[lo] >> bit) & 1;
            if (!(h == 1 && l == 1))
                continue; // only try to shrink DC variables

            // try → 0  (clear hi, keep lo = 01)
            buf[hi] &= ~(1u << bit);
            if (reduce_check(M, j, buf, on_minterms, nwords))
                goto committed;

            // try → 1  (set hi, clear lo = 10)
            buf[hi] |= (1u << bit);
            buf[lo] &= ~(1u << bit);
            if (reduce_check(M, j, buf, on_minterms, nwords))
                goto committed;

            // neither works, revert to DC (11)
            buf[hi] |= (1u << bit);
            buf[lo] |= (1u << bit);

        committed:;
        }

        // commit: update matrix and write back
        update_matrix(M, j, buf, on_minterms, nwords);
        set_copy(p, buf, nwords);
    }

    matrix_free(M);
    free_on_minterms(on_minterms, num_on);
    cover_free(F_save);
    return result;
}

set_family *espresso_minimize(set_family *F, int nin, int nout)
{
    clock_t t0 = clock();
    set_family *R = complement(F, nin);
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

        tmp = irredundant(best, R, nin);
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

// simple irredundant: single-cube containment, for small problems
static set_family *irredundant_simple(set_family *F)
{
    int nwords = F->wsize;
    set_family *r = cover_new(nwords, F->count);
    pset p, last;
    foreach_set(F, last, p)
    {
        int keep = 1;
        pset q, qlast;
        foreach_set(r, qlast, q)
        {
            if (set_implies(p, q, nwords))
            {
                keep = 0;
                break;
            }
        }
        if (keep)
            cover_add(r, p);
    }
    return r;
}

// brute-force OFF-set: enumerate all minterms not covered by F
static set_family *compute_off_set_bf(set_family *F, int nin)
{
    int half = (nin + 15) / 16, nwords = half * 2, total = 1 << nin;
    set_family *R = cover_new(nwords, total);
    for (int i = 0; i < total; i++)
    {
        unsigned int buf[128];
        pset m = buf;
        set_clear(m, nwords);
        for (int v = 0; v < nin; v++)
        {
            int hi = v / 16, lo = hi + half, bit = v % 16;
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

// small-problem path: brute-force OFF-set + simple irredundant
static set_family *espresso_minimize_small(set_family *F, int nin)
{
    clock_t t0 = clock();
    set_family *R = compute_off_set_bf(F, nin);
    if (trace_on)
        fprintf(stderr, "# OFF-SET     Time was %.2f ms, cost is c=%d\n",
                (double)(clock() - t0) / CLOCKS_PER_SEC * 1000, F->count);
    int nwords = F->wsize, half = nwords / 2, iterations = 0;
    set_family *best = cover_dup(F);
    int last_count;
    for (int iter = 0; iter < 10; iter++)
    {
        last_count = best->count;
        clock_t t1 = clock();
        set_family *E = cover_new(nwords, best->count);
        {
            pset p, last2;
            foreach_set(best, last2, p)
            {
                unsigned int buf[128];
                set_copy(buf, p, nwords);
                for (int v = 0; v < nin; v++)
                {
                    int hi = v / 16, lo = hi + half, bit = v % 16;
                    int h = (buf[hi] >> bit) & 1, l = (buf[lo] >> bit) & 1;
                    if (h == 1 && l == 1)
                        continue;
                    buf[hi] |= (1u << bit);
                    buf[lo] |= (1u << bit);
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
                        buf[hi] &= ~(1u << bit);
                        buf[lo] &= ~(1u << bit);
                        if (h)
                            buf[hi] |= (1u << bit);
                        if (l)
                            buf[lo] |= (1u << bit);
                    }
                }
                cover_add(E, buf);
            }
        }
        cover_free(best);
        best = E;
        set_family *t = irredundant_simple(best);
        cover_free(best);
        best = t;
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

// auto-select: ≤8 inputs & single output → small path, else multi-output
set_family *espresso_minimize_auto(set_family *F, int nin, int nout)
{
    if (nin <= 8 && nout == 1)
        return espresso_minimize_small(F, nin);
    return espresso_minimize_multi(F, nin, nout);
}

// multi-output: minimize each output independently, then merge.

// extract cubes where output out_idx = 1, clear all output bits
static set_family *extract_output_on(set_family *F, int out_idx,
                                     int nin, int nout)
{
    int nwords = F->wsize;
    set_family *result = cover_new(nwords, F->count > 0 ? F->count : 1);
    result->count = 0;

    pset p, last;
    foreach_set(F, last, p)
    {
        if (!set_has_output(p, out_idx, nin))
            continue;

        unsigned int buf[128];
        set_copy(buf, p, nwords);

        /* clear ALL output hi-bits (nin .. nin+nout-1) */
        for (int o = 0; o < nout; o++)
        {
            int ohi = (nin + o) / 16;
            int obit = (nin + o) % 16;
            buf[ohi] &= ~(1u << obit);
        }

        cover_add(result, buf);
    }
    return result;
}

// set output out_idx = 1 on every cube in F
static void cover_set_output(set_family *F, int out_idx, int nin)
{
    pset p, last;
    foreach_set(F, last, p)
    {
        int ohi = (nin + out_idx) / 16;
        int obit = (nin + out_idx) % 16;
        p[ohi] |= (1u << obit);
    }
}

// append all cubes from src into dst (in-place)
static void cover_append(set_family *dst, set_family *src)
{
    pset p, last;
    foreach_set(src, last, p)
        cover_add(dst, p);
}

set_family *espresso_minimize_multi(set_family *F, int nin, int nout)
{
    int nwords = F->wsize;

    /* result will hold merged minimized covers from all outputs */
    set_family *merged = cover_new(nwords, F->count > 0 ? F->count : 1);
    merged->count = 0;

    for (int o = 0; o < nout; o++)
    {
        set_family *on_set = extract_output_on(F, o, nin, nout);
        if (on_set->count == 0)
        {
            /* output is constant 0 — no ON-set cubes */
            cover_free(on_set);
            continue;
        }

        /* minimize this single-output function */
        set_family *minimized = espresso_minimize(on_set, nin, 1);
        cover_free(on_set);

        /* restore the output bit */
        cover_set_output(minimized, o, nin);

        /* merge into final result */
        cover_append(merged, minimized);
        cover_free(minimized);
    }

    return merged;
}
