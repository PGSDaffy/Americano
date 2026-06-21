#include "set.h"
#include <string.h>

void set_and(pset d, pset a, pset b, int nwords)
{
    for (int i = 0; i < nwords; i++)
        d[i] = a[i] & b[i];
}

void set_or(pset d, pset a, pset b, int nwords)
{
    for (int i = 0; i < nwords; i++)
        d[i] = a[i] | b[i];
}

void set_diff(pset d, pset a, pset b, int nwords)
{
    for (int i = 0; i < nwords; i++)
        d[i] = a[i] & ~b[i];
}

void set_copy(pset d, pset a, int nwords)
{
    memcpy(d, a, nwords * sizeof(unsigned int));
}

void set_clear(pset s, int nwords)
{
    memset(s, 0, nwords * sizeof(unsigned int));
}

int set_equal(pset a, pset b, int nwords)
{
    for (int i = 0; i < nwords; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

int set_implies(pset a, pset b, int nwords)
{
    // a 的所有 1 位，在 b 里也必须是 1
    for (int i = 0; i < nwords; i++)
        if ((a[i] & b[i]) != a[i])
            return 0;
    return 1;
}

int set_disjoint(pset a, pset b, int nwords)
{
    for (int i = 0; i < nwords; i++)
        if (a[i] & b[i])
            return 0;
    return 1;
}

int set_empty(pset a, int nwords)
{
    for (int i = 0; i < nwords; i++)
        if (a[i] != 0)
            return 0;
    return 1;
}

// ── 变量级操作 ───────────────────────────────────────

int set_get_var(pset s, int v, int half)
{
    int hi = v / 16, lo = hi + half, bit = v % 16;
    int h = (s[hi] >> bit) & 1;
    int l = (s[lo] >> bit) & 1;
    return (h << 1) | l;
}

void set_set_var_dc(pset s, int v, int half)
{
    int hi = v / 16, lo = hi + half, bit = v % 16;
    s[hi] |= (1u << bit);
    s[lo] |= (1u << bit);
}

void set_force_var(pset s, int v, int val, int half)
{
    int hi = v / 16, lo = hi + half, bit = v % 16;
    s[hi] &= ~(1u << bit);
    s[lo] &= ~(1u << bit);
    if (val == ONE)       s[hi] |= (1u << bit);
    else if (val == ZERO) s[lo] |= (1u << bit);
}

int set_var_phase(pset s, int v, int half)
{
    int hi = v / 16, lo = hi + half, bit = v % 16;
    int h = (s[hi] >> bit) & 1;
    int l = (s[lo] >> bit) & 1;
    return (h ? 1 : 0) | (l ? 2 : 0);
}

int set_intersect(pset a, pset b, int nin, int nwords)
{
    int half = nwords / 2;
    for (int v = 0; v < nin; v++) {
        int hi = v / 16, lo = hi + half, bit = v % 16;
        int ah = (a[hi] >> bit) & 1, al = (a[lo] >> bit) & 1;
        int bh = (b[hi] >> bit) & 1, bl = (b[lo] >> bit) & 1;
        if (!((ah && bh) || (al && bl)))
            return 0;
    }
    return 1;
}

int set_intersect_full(pset a, pset b, int nin, int nout, int nwords)
{
    int half = nwords / 2;

    /* input variables: 2-bit compatibility check */
    for (int v = 0; v < nin; v++) {
        int hi = v / 16, lo = hi + half, bit = v % 16;
        int ah = (a[hi] >> bit) & 1, al = (a[lo] >> bit) & 1;
        int bh = (b[hi] >> bit) & 1, bl = (b[lo] >> bit) & 1;
        if (!((ah && bh) || (al && bl)))
            return 0;
    }

    /* output variables: hi-bit only compatibility */
    for (int o = 0; o < nout; o++) {
        int idx = nin + o;
        int hi = idx / 16, bit = idx % 16;
        int ah = (a[hi] >> bit) & 1;
        int bh = (b[hi] >> bit) & 1;
        /* DC (both 1 for input-like) can't happen here; just check
         * that both have 1 or at least one has 0 (compatible) */
        if (ah && !bh) return 0;  /* a requires 1 but b has 0 → conflict */
        if (!ah && bh) return 0;  /* b requires 1 but a has 0 → conflict */
    }
    return 1;
}

int set_var_phase_output(pset s, int var, int half, int nin)
{
    if (var < nin) {
        /* input variable: use standard 2-bit phase */
        int hi = var / 16, lo = hi + half, bit = var % 16;
        int h = (s[hi] >> bit) & 1;
        int l = (s[lo] >> bit) & 1;
        return (h ? 1 : 0) | (l ? 2 : 0);
    } else {
        /* output variable: hi-bit only → 1=ONE, 0=ZERO (never 0/DC) */
        int hi = var / 16, bit = var % 16;
        return ((s[hi] >> bit) & 1) ? 1 : 2;
    }
}

int set_has_output(pset s, int out_idx, int nin)
{
    int hi = (nin + out_idx) / 16;
    int bit = (nin + out_idx) % 16;
    return (s[hi] >> bit) & 1;
}
