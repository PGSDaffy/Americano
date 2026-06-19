#include "espresso.h"
#include "complement.h"
#include "matrix.h"
#include <time.h>

static int trace_on = 0;

void espresso_set_trace(int on) { trace_on = on; }

// ── EXPAND: 把每个 cube 尽量变大，但不碰到 OFF-set ──
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

            // 检查是否碰到了 OFF-set（与任何 OFF-set cube 有交集即为碰到）
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

// ── helpers: enumerate all ON-set minterms ─────────────────────────

/* walk all 2^nin input combinations; return every minterm covered by F.
 * caller receives a freshly-malloc'd array of pset and the count.     */
static pset *enumerate_on_set(set_family *F, int nin, int *num_on)
{
    int nwords = F->wsize, half = nwords / 2, total = 1 << nin;
    int cap = (total < 256) ? total : 256;
    pset *on = (pset *) malloc((size_t)cap * sizeof(pset));
    int cnt = 0;

    for (int i = 0; i < total; i++) {
        /* build minterm i on the stack */
        unsigned int m_buf[128];          /* 128 ints → 1024 variables max */
        pset m = m_buf;
        set_clear(m, nwords);
        for (int v = 0; v < nin; v++) {
            int hi = v / 16, lo = hi + half, bit = v % 16;
            if (i & (1 << v))
                m[hi] |= (1u << bit);
            else
                m[lo] |= (1u << bit);
        }

        /* check coverage */
        pset p, last;
        foreach_set(F, last, p) {
            if (set_implies(m, p, nwords)) {
                if (cnt >= cap) { cap *= 2;
                    on = (pset *) realloc(on, (size_t)cap * sizeof(pset)); }
                on[cnt] = (pset) malloc((size_t)nwords * sizeof(unsigned int));
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
    for (int i = 0; i < num_on; i++) free(on[i]);
    free(on);
}

/* ── helpers: coverage matrix ────────────────────────────────────── */

/* build sparse matrix M[row=minterm_idx][col=cube_idx] = 1 if covered */
static sm_matrix *build_cover_matrix(pset *on_minterms, int num_on,
                                     set_family *F)
{
    int nwords = F->wsize;
    sm_matrix *M = matrix_alloc();
    if (num_on > 0 && F->count > 0)
        matrix_resize(M, num_on - 1, F->count - 1);

    for (int i = 0; i < num_on; i++) {
        for (int j = 0; j < F->count; j++) {
            if (set_implies(on_minterms[i], GETSET(F, j), nwords))
                matrix_insert(M, i, j);
        }
    }
    return M;
}

/* check whether reducing cube col_j to 'reduced' still covers every
 * minterm for which col_j is the ONE AND ONLY cover in M.
 * Returns 1 = safe, 0 = coverage lost.                                 */
static int reduce_check(sm_matrix *M, int col_j, pset reduced,
                        pset *on_minterms, int nwords)
{
    sm_col *pcol = matrix_get_col(M, col_j);
    if (pcol == NULL) return 1;          /* cube already covers nothing */

    sm_element *e;
    sm_foreach_col_element(pcol, e) {
        int row_i = e->row_num;
        if (matrix_row_count(M, row_i) == 1) {
            /* sole cover — must still match */
            if (!set_implies(on_minterms[row_i], reduced, nwords))
                return 0;
        }
    }
    return 1;
}

/* after cube col_j is successfully reduced, remove from M any minterm
 * that the new, smaller cube no longer covers.                         */
static void update_matrix(sm_matrix *M, int col_j, pset new_cube,
                          pset *on_minterms, int nwords)
{
    sm_col *pcol = matrix_get_col(M, col_j);
    if (pcol == NULL) return;

    sm_element *e, *enext;
    for (e = pcol->first_row; e != NULL; e = enext) {
        enext = e->next_row;
        if (!set_implies(on_minterms[e->row_num], new_cube, nwords))
            matrix_remove(M, e->row_num, col_j);
    }
}


/* ── IRREDUNDANT: union-based redundancy detection via covering matrix ──
 *
 *  Previous version only checked p ⊆ q (single-cube containment).
 *  This version builds the full covering matrix and keeps a cube iff
 *  it covers at least one minterm NOT already covered by previously
 *  kept (larger) cubes.  Cubes are processed largest-first.             */

set_family *irredundant(set_family *F, set_family *R, int nin)
{
    (void)R;
    int nwords = F->wsize;

    int num_on;
    pset *on_minterms = enumerate_on_set(F, nin, &num_on);
    if (num_on == 0) {
        free(on_minterms);
        set_family *result = cover_new(nwords, 1);
        result->count = 0;
        return result;
    }

    sm_matrix *M = build_cover_matrix(on_minterms, num_on, F);

    /* sort cube indices by coverage size descending (largest first) */
    int *order = (int *) malloc((size_t)F->count * sizeof(int));
    for (int j = 0; j < F->count; j++) order[j] = j;
    for (int a = 0; a < F->count; a++) {
        int best = a;
        for (int b = a + 1; b < F->count; b++)
            if (matrix_col_count(M, order[b]) > matrix_col_count(M, order[best]))
                best = b;
        int tmp = order[a]; order[a] = order[best]; order[best] = tmp;
    }

    int *covered_cnt = (int *) calloc((size_t)num_on, sizeof(int));
    int *keep        = (int *) calloc((size_t)F->count, sizeof(int));

    set_family *result = cover_new(nwords, F->count > 0 ? F->count : 1);
    result->count = 0;

    for (int idx = 0; idx < F->count; idx++) {
        int j = order[idx];
        sm_col *pcol = matrix_get_col(M, j);
        if (pcol == NULL) continue;       /* cube covers nothing */

        /* is j redundant?  yes if every minterm it covers has
         * covered_cnt > 0 (i.e. already covered by a kept cube). */
        int redundant = 1;
        sm_element *e;
        sm_foreach_col_element(pcol, e) {
            if (covered_cnt[e->row_num] == 0) {
                redundant = 0;
                break;
            }
        }

        if (!redundant) {
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


/* ── REDUCE: shrink each cube, never losing ON-set coverage ────────
 *
 *  OLD: iterated all 2^nin minterms per literal per cube – O(c·v·2^n).
 *  NEW: builds a sparse covering matrix once, then only checks the
 *        "essential" minterms (those covered solely by this cube).    */

set_family *reduce(set_family *F, set_family *R, int nin)
{
    (void)R;
    int nwords = F->wsize, half = nwords / 2;

    set_family *F_save = cover_dup(F);
    set_family *result = cover_dup(F);

    /* ── 1. enumerate ON-set & build coverage matrix ── */
    int num_on;
    pset *on_minterms = enumerate_on_set(F_save, nin, &num_on);
    if (num_on == 0) {
        free_on_minterms(on_minterms, num_on);
        cover_free(F_save);
        return result;
    }

    sm_matrix *M = build_cover_matrix(on_minterms, num_on, result);

    /* ── 2. reduce each cube in place ── */
    for (int j = 0; j < result->count; j++) {
        pset p = GETSET(result, j);
        unsigned int buf[128];
        set_copy(buf, p, nwords);

        for (int v = 0; v < nin; v++) {
            int hi = v / 16, lo = hi + half, bit = v % 16;
            int h = (buf[hi] >> bit) & 1;
            int l = (buf[lo] >> bit) & 1;
            if (!(h == 1 && l == 1)) continue;   /* only shrink DC vars */

            /* try → 0  (clear hi-bit, keep lo-bit = 1 → encoding 01) */
            buf[hi] &= ~(1u << bit);
            if (reduce_check(M, j, buf, on_minterms, nwords))
                goto committed;

            /* try → 1  (set hi-bit, clear lo-bit → encoding 10) */
            buf[hi] |= (1u << bit);
            buf[lo] &= ~(1u << bit);
            if (reduce_check(M, j, buf, on_minterms, nwords))
                goto committed;

            /* neither works — revert to DC */
            buf[hi] |= (1u << bit);
            buf[lo] |= (1u << bit);

            committed:;
        }

        /* ── commit: update matrix and write back ── */
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
