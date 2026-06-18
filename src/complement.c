#include "complement.h"
#include <string.h>

// ── 前向声明 ─────────────────────────────────────────
static set_family *complement_rec(set_family *F, int nin, int used_mask);
static set_family *complement_enum(set_family *F, int nin, int used_mask);
static set_family *compl_single_cube(pset p, int nin, int nwords);
static set_family *cover_cofactor(set_family *F, int var, int val, int nwords);
static int binate_split_select(set_family *F, int nin, int used_mask,
                               int nwords, pset cl, pset cr);
static set_family *compl_merge(set_family *L, set_family *R,
                               pset cl, pset cr, int var, int nin, int nwords);
static void compl_d1merge(pset *L1, int nL, pset *R1, int nR,
                          int var, int half, int nwords);
static int  d1_compare(const void *a, const void *b);
static void compl_lift(pset *A1, int nA, set_family *B,
                       pset bcube, int var, int half, int nwords);

// d1_compare 需要的上下文（避免 qsort_r 的移植问题）
static struct { int var; int half; int nwords; } d1_ctx;

// ── 公共入口 ─────────────────────────────────────────

set_family *complement(set_family *F, int nin)
{
    return complement_rec(F, nin, 0);
}

// ── 递归核心 ─────────────────────────────────────────

static set_family *complement_rec(set_family *F, int nin, int used_mask)
{
    int nwords = F->wsize;
    int half   = nwords / 2;

    // ─── 特殊情形 1: 空 cover → 全集 ───
    if (F->count == 0) {
        set_family *R = cover_new(nwords, 1);
        unsigned int buf[nwords];
        pset u = buf;
        set_clear(u, nwords);
        for (int v = 0; v < nin; v++)
            set_set_var_dc(u, v, half);
        cover_add(R, u);
        return R;
    }

    // ─── 特殊情形 2: 单 cube → De Morgan ───
    if (F->count == 1)
        return compl_single_cube(GETSET(F, 0), nin, nwords);

    // ─── 特殊情形 3: 存在全-DC cube → 空集 ───
    {
        pset p, last;
        foreach_set(F, last, p) {
            int all_dc = 1;
            for (int v = 0; v < nin && all_dc; v++)
                if (set_get_var(p, v, half) != DC)
                    all_dc = 0;
            if (all_dc)
                return cover_new(nwords, 1);
        }
    }

    // ─── 选取分裂变量 ───
    unsigned int cl_buf[nwords], cr_buf[nwords];
    pset cl = cl_buf, cr = cr_buf;
    int var = binate_split_select(F, nin, used_mask, nwords, cl, cr);

    if (var < 0) {
        return complement_enum(F, nin, used_mask);
    }

    // ─── 递归分裂 ───
    set_family *F1 = cover_cofactor(F, var, ONE,  nwords);
    set_family *F0 = cover_cofactor(F, var, ZERO, nwords);

    set_family *L = complement_rec(F1, nin, used_mask | (1 << var));
    set_family *R = complement_rec(F0, nin, used_mask | (1 << var));

    cover_free(F1);
    cover_free(F0);

    return compl_merge(L, R, cl, cr, var, nin, nwords);
}

// ── 回退枚举（base case） ──────────────────────────

static set_family *complement_enum(set_family *F, int nin, int used_mask)
{
    int nwords = F->wsize;
    int half   = nwords / 2;

    int active_vars = 0;
    for (int v = 0; v < nin; v++)
        if (!(used_mask & (1 << v)))
            active_vars++;

    int total = 1 << active_vars;
    set_family *R = cover_new(nwords, total > 0 ? total : 1);

    for (int i = 0; i < total; i++) {
        unsigned int buf[nwords];
        pset m = buf;
        set_clear(m, nwords);

        for (int v = 0; v < nin; v++)
            if (used_mask & (1 << v))
                set_set_var_dc(m, v, half);

        int bit_idx = 0;
        for (int v = 0; v < nin; v++) {
            if (used_mask & (1 << v)) continue;
            if (i & (1 << bit_idx))
                set_force_var(m, v, ONE, half);
            else
                set_force_var(m, v, ZERO, half);
            bit_idx++;
        }

        int covered = 0;
        pset p, last;
        foreach_set(F, last, p) {
            if (set_implies(m, p, nwords)) {
                covered = 1;
                break;
            }
        }
        if (!covered)
            cover_add(R, m);
    }

    return R;
}

// ── De Morgan: 单 cube 求补 ────────────────────────

static set_family *compl_single_cube(pset p, int nin, int nwords)
{
    int half = nwords / 2;

    int nterms = 0;
    for (int v = 0; v < nin; v++)
        if (set_get_var(p, v, half) != DC)
            nterms++;

    set_family *R = cover_new(nwords, nterms > 0 ? nterms : 1);

    if (nterms == 0) {
        return R;
    }

    for (int v = 0; v < nin; v++) {
        int val = set_get_var(p, v, half);
        if (val == DC) continue;

        unsigned int buf[nwords];
        pset q = buf;
        set_clear(q, nwords);

        for (int u = 0; u < nin; u++)
            set_set_var_dc(q, u, half);

        if (val == ONE)
            set_force_var(q, v, ZERO, half);
        else
            set_force_var(q, v, ONE, half);

        cover_add(R, q);
    }

    return R;
}

// ── Shannon 余因子 ─────────────────────────────────

static set_family *cover_cofactor(set_family *F, int var, int val, int nwords)
{
    int half = nwords / 2;
    set_family *result = cover_new(nwords, F->count > 0 ? F->count : 1);

    pset p, last;
    foreach_set(F, last, p) {
        int phase = set_var_phase(p, var, half);
        if (val == ONE  && !(phase & 1)) continue;
        if (val == ZERO && !(phase & 2)) continue;

        unsigned int buf[nwords];
        set_copy(buf, p, nwords);
        set_set_var_dc(buf, var, half);
        cover_add(result, buf);
    }

    return result;
}

// ── 选取最 binate 的分裂变量 ───────────────────────

static int binate_split_select(set_family *F, int nin, int used_mask,
                               int nwords, pset cl, pset cr)
{
    int half = nwords / 2;
    int best_var = -1;
    int best_balance = 999999;

    set_clear(cl, nwords);
    set_clear(cr, nwords);
    for (int v = 0; v < nin; v++) {
        set_set_var_dc(cl, v, half);
        set_set_var_dc(cr, v, half);
    }

    for (int v = 0; v < nin; v++) {
        if (used_mask & (1 << v)) continue;

        int cnt1 = 0, cnt0 = 0;
        pset p, last;
        foreach_set(F, last, p) {
            int ph = set_var_phase(p, v, half);
            if (ph & 1) cnt1++;
            if (ph & 2) cnt0++;
        }

        if (cnt1 == 0 || cnt0 == 0) continue;

        int balance = (cnt1 > cnt0) ? (cnt1 - cnt0) : (cnt0 - cnt1);
        if (balance < best_balance) {
            best_balance = balance;
            best_var = v;
        }
    }

    if (best_var < 0)
        return -1;

    for (int v = 0; v < nin; v++) {
        set_set_var_dc(cl, v, half);
        set_set_var_dc(cr, v, half);
    }
    set_force_var(cl, best_var, ONE,  half);
    set_force_var(cr, best_var, ZERO, half);

    return best_var;
}

// ── 合并左右 cofactor 的补集 ───────────────────────

static set_family *compl_merge(set_family *L, set_family *R,
                               pset cl, pset cr, int var, int nin, int nwords)
{
    (void)nin;
    int half = nwords / 2;

    // Phase 1: 交集
    {
        pset p, last;
        foreach_set(L, last, p) set_and(p, p, cl, nwords);
        foreach_set(R, last, p) set_and(p, p, cr, nwords);
    }

    // Phase 2: 转为指针数组并排序
    int nL = L->count, nR = R->count;
    pset *L1 = malloc((nL + 1) * sizeof(pset));
    pset *R1 = malloc((nR + 1) * sizeof(pset));

    {
        int i = 0; pset p, last;
        foreach_set(L, last, p) L1[i++] = p;
        L1[nL] = NULL;
    }
    {
        int i = 0; pset p, last;
        foreach_set(R, last, p) R1[i++] = p;
        R1[nR] = NULL;
    }

    d1_ctx.var = var; d1_ctx.half = half; d1_ctx.nwords = nwords;
    qsort(L1, nL, sizeof(pset), d1_compare);
    qsort(R1, nR, sizeof(pset), d1_compare);

    // Phase 3: Distance-1 merge
    compl_d1merge(L1, nL, R1, nR, var, half, nwords);

    // Phase 4: Lifting
    compl_lift(L1, nL, R, cr, var, half, nwords);
    compl_lift(R1, nR, L, cl, var, half, nwords);

    // Phase 5: 收集结果
    set_family *Tbar = cover_new(nwords, nL + nR);
    for (int i = 0; i < nL; i++) {
        cover_add(Tbar, L1[i]);
    }
    for (int i = 0; i < nR; i++) {
        if (!set_empty(R1[i], nwords))
            cover_add(Tbar, R1[i]);
    }

    free(L1);
    free(R1);
    cover_free(L);
    cover_free(R);
    return Tbar;
}

// ── Distance-1 合并 ────────────────────────────────

static int d1_compare(const void *a, const void *b)
{
    pset pa = *(pset *)a;
    pset pb = *(pset *)b;
    int var    = d1_ctx.var;
    int half   = d1_ctx.half;
    int nwords = d1_ctx.nwords;

    for (int i = 0; i < nwords; i++) {
        int hi = var / 16;
        int lo = hi + half;
        int bit = var % 16;
        unsigned int mask = ~(1u << bit);

        unsigned int va, vb;
        if (i == hi || i == lo) {
            va = pa[i] & mask;
            vb = pb[i] & mask;
        } else {
            va = pa[i];
            vb = pb[i];
        }
        if (va < vb) return -1;
        if (va > vb) return 1;
    }
    return 0;
}

static void compl_d1merge(pset *L1, int nL, pset *R1, int nR,
                          int var, int half, int nwords)
{
    int i = 0, j = 0;
    while (i < nL && j < nR) {
        pset pl = L1[i], pr = R1[j];

        int cmp = 0;
        {
            int hi = var / 16, lo = hi + half, bit = var % 16;
            unsigned int mask = ~(1u << bit);
            for (int k = 0; k < nwords && cmp == 0; k++) {
                unsigned int va, vb;
                if (k == hi || k == lo) {
                    va = pl[k] & mask;
                    vb = pr[k] & mask;
                } else {
                    va = pl[k];
                    vb = pr[k];
                }
                if (va < vb) cmp = -1;
                else if (va > vb) cmp = 1;
            }
        }

        if (cmp == 0) {
            set_set_var_dc(pl, var, half);
            set_clear(pr, nwords);
            i++; j++;
        } else if (cmp < 0) {
            i++;
        } else {
            j++;
        }
    }
}

// ── Lifting：跨分支提升 ────────────────────────────

static void compl_lift(pset *A1, int nA, set_family *B,
                       pset bcube, int var, int half, int nwords)
{
    for (int i = 0; i < nA; i++) {
        pset a = A1[i];
        if (set_empty(a, nwords)) continue;

        unsigned int lift_buf[nwords];
        pset lift = lift_buf;
        set_copy(lift, a, nwords);

        int bcube_val = set_get_var(bcube, var, half);
        if (bcube_val == ONE)
            set_force_var(lift, var, ONE, half);
        else
            set_force_var(lift, var, ZERO, half);

        pset q, last;
        foreach_set(B, last, q) {
            if (set_implies(lift, q, nwords)) {
                set_set_var_dc(a, var, half);
                break;
            }
        }
    }
}
