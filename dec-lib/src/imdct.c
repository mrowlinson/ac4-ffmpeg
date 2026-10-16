/* imdct.c — clean AC-4 IMDCT + KBD window + overlap-add core.
 * Tile-centered lattice; see imdct.h + spec/imdct-clean-e2.md S2.
 * Naive O(N^2) transforms (correctness first; FFT fast path later).
 */
#include "imdct.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double imdct_alpha(int n, int full) {
    static const double rows[5] = {IMDCT_ALPHA_0, IMDCT_ALPHA_1,
                                   IMDCT_ALPHA_2, IMDCT_ALPHA_3,
                                   IMDCT_ALPHA_4};
    if (n <= 0 || full <= 0 || full % n != 0)
        return 0.0;
    int r = 0, f = full;
    while (f > n) {
        if (f & 1)
            return 0.0;
        f >>= 1;
        r++;
    }
    if (r > 4)
        r = 4;
    return rows[r];
}

double imdct_bessel_i0(double x) {
    /* Series: I0(x) = sum (x^2/4)^k / (k!)^2. Converges fast for
     * |x| <= pi*6 ~ 18.9 (max KBD arg). */
    double xx = x * x / 4.0;
    double term = 1.0, sum = 1.0;
    for (int k = 1; k < 60; k++) {
        term *= xx / ((double)k * (double)k);
        sum += term;
        if (term < 1e-18 * sum)
            break;
    }
    return sum;
}

/* Rising KBD taper of length L (L>=1): left half of KBD span 2L.
 * w[0] ~ 0, w[L-1] ~ 1.
 * AC4DEC_KBDNPTS=1 (duet34 window-grid discriminant): L-point kernel
 * grid (i=0..L-1, Tot over L pts; w[L-1] = 1.0 exactly) instead of the
 * default L+1-point textbook grid (i=0..L). Both are PB-exact by kernel
 * symmetry; they differ in endpoint/interior shape. Default unset = L+1. */
static void taper_rise(double *w, int L, double alpha) {
    double denom = imdct_bessel_i0(M_PI * alpha);
    double Tot = 0.0, csum = 0.0;
    int npts = 0;
    {
        const char *np = getenv("AC4DEC_KBDNPTS");
        npts = (np && np[0] == '1');
    }
    int last = npts ? L - 1 : L;
    double span = (npts && L > 1) ? (double)(L - 1) : (double)L;
    for (int i = 0; i <= last; i++) {
        double t = 2.0 * i / span - 1.0;
        Tot += imdct_bessel_i0(M_PI * alpha * sqrt(1.0 - t * t)) / denom;
    }
    for (int j = 0; j < L; j++) {
        double t = 2.0 * j / span - 1.0;
        csum += imdct_bessel_i0(M_PI * alpha * sqrt(1.0 - t * t)) / denom;
        w[j] = sqrt(csum / Tot);
    }
}

void imdct_kbd(double *w, int n, double alpha) {
    taper_rise(w, n, alpha);
    for (int j = 0; j < n; j++)
        w[n + j] = w[n - 1 - j];
}

void imdct_bridge(double *w, int n, int pn, int nn, double alpha,
                  double palpha, double nalpha) {
    if (pn <= 0) {
        pn = n;
        palpha = alpha;
    }
    if (nn <= 0) {
        nn = n;
        nalpha = alpha;
    }
    /* Taper alpha = TAPER-LENGTH row (wave6 e3-pcm, drop e3-pcm-1):
     * both sides of a bridge share alpha(min), exactly power-
     * complementary (1e-15). ANSWER-e2-1 "own row" refuted: mixed
     * alphas give 3.4e-2 complementarity error (the 7e-2 floor).
     * palpha/nalpha are the neighbor-row alphas (contract: caller
     * passes imdct_alpha(pn)/imdct_alpha(nn)); the taper side uses
     * the neighbor's alpha since taper length = neighbor length. */
    /* Left half. */
    if (pn >= n) {
        /* Full rise taper (equal-or-longer prev). */
        taper_rise(w, n, alpha);
    } else {
        /* Short prev: zeros + rise(T=pn, taper-row palpha) + flat. */
        int z = (n - pn) / 2;
        for (int j = 0; j < z; j++)
            w[j] = 0.0;
        taper_rise(w + z, pn, palpha);
        for (int j = z + pn; j < n; j++)
            w[j] = 1.0;
    }
    /* Right half. */
    if (nn >= n) {
        double tmp[IMDCT_MAXN];
        taper_rise(tmp, n, alpha);
        for (int j = 0; j < n; j++)
            w[n + j] = tmp[n - 1 - j];
    } else {
        int z = (n - nn) / 2;
        double tmp[IMDCT_MAXN];
        taper_rise(tmp, nn, nalpha);
        for (int j = 0; j < z; j++)
            w[n + j] = 1.0;
        for (int j = 0; j < nn; j++)
            w[n + z + j] = tmp[nn - 1 - j];
        for (int j = z + nn; j < n; j++)
            w[n + j] = 0.0;
    }
}

/* ---- cos-kernel cache (pipe1 clean-dec, bit-identical) ----
 * The MDCT/IMDCT kernel cos(M_PI/n*(i+0.5+n/2.0)*(k+0.5)) depends
 * only on (n,i,k): identical across every block of a size, so it is
 * computed once per size and reused. Fill evaluates the EXACT same
 * expression in the same order as the naive loop, and the dot
 * products below keep the naive accumulation order, so cached
 * results are bit-identical to the uncached path (no fast-math:
 * Makefile uses plain -O2). Slots: 4 sizes (streams use 1-2;
 * eviction is round-robin). Sizes above 2048 bypass the cache
 * (footprint cap; correctness unaffected). AC4DEC_NOCOSCACHE=1
 * forces the naive path (identity tests + debugging). The cache
 * is process-lifetime (no free; single-threaded consumers). */
#define IMDCT_COS_NSLOT 4
#define IMDCT_COS_MAXN 2048
static struct {
    int n;
    double *m; /* [2n][n] row-major */
} cos_cache[IMDCT_COS_NSLOT];
static int cos_next = 0;
static int cos_off = -1; /* -1 = read env once */

static const double *imdct_cosmat(int n) {
    if (cos_off < 0) {
        const char *e = getenv("AC4DEC_NOCOSCACHE");
        cos_off = (e && e[0] == '1');
    }
    if (cos_off || n <= 0 || n > IMDCT_COS_MAXN || (n & 1))
        return NULL;
    for (int s = 0; s < IMDCT_COS_NSLOT; s++)
        if (cos_cache[s].n == n && cos_cache[s].m)
            return cos_cache[s].m;
    size_t nn = (size_t)n;
    double *m = malloc(2 * nn * nn * sizeof(double));
    if (!m)
        return NULL; /* OOM: caller falls back to naive */
    for (int i = 0; i < 2 * n; i++)
        for (int k = 0; k < n; k++)
            m[(size_t)i * nn + (size_t)k] =
                cos(M_PI / n * (i + 0.5 + n / 2.0) * (k + 0.5));
    free(cos_cache[cos_next].m);
    cos_cache[cos_next].n = n;
    cos_cache[cos_next].m = m;
    cos_next = (cos_next + 1) % IMDCT_COS_NSLOT;
    return m;
}

void imdct_mdct(const double *x, double *X, int n) {
    const double *C = imdct_cosmat(n);
    for (int k = 0; k < n; k++) {
        double s = 0.0;
        if (C) {
            size_t nn = (size_t)n;
            for (int i = 0; i < 2 * n; i++)
                s += x[i] * C[(size_t)i * nn + (size_t)k];
        } else {
            for (int i = 0; i < 2 * n; i++)
                s += x[i] * cos(M_PI / n * (i + 0.5 + n / 2.0) * (k + 0.5));
        }
        X[k] = s;
    }
}

void imdct_imdct(const double *X, double *y, int n) {
    /* ETSI P62 literal: /N (wave8 e3-dolby; was 2x-hot 2/n.
     * P60-62 kernel check: 1e-15 exact at N=64/256). */
    double sc = 1.0 / n;
    const double *C = imdct_cosmat(n);
    for (int i = 0; i < 2 * n; i++) {
        double s = 0.0;
        if (C) {
            const double *row = C + (size_t)i * (size_t)n;
            for (int k = 0; k < n; k++)
                s += X[k] * row[k];
        } else {
            for (int k = 0; k < n; k++)
                s += X[k] * cos(M_PI / n * (i + 0.5 + n / 2.0) * (k + 0.5));
        }
        y[i] = sc * s;
    }
}

void imdct_set_max(imdct_state *st, int max_n) {
    if (max_n <= 0 || max_n > IMDCT_MAXN || (max_n & 1))
        max_n = IMDCT_MAXN;
    st->H = max_n / 2;
    imdct_reset(st);
}

void imdct_reset(imdct_state *st) {
    int H = st->H > 0 ? st->H : IMDCT_MAXN / 2;
    memset(st->pend, 0, sizeof(st->pend));
    st->base = -H;
    st->H = H;
    st->T = 0;
    st->prev_n = 0;
    st->started = 0;
}

/* FXPOLA fixed-point-OLA probe (wave19 edge-adapt, EXPERIMENTAL).
 * ETSI TS 103 190-1 V1.4.1 clause 5.5.2 (own-words): fixed-point
 * overlap/add SHALL saturate (no wrap-around); output can exceed
 * 100% even if input did not. Pend units are LSB (d2s16 x32768),
 * so 100% = 32768. Env gates (default OFF, zero behavior change):
 * AC4DEC_OLAPEEK=1 -> per-state peak/adds report at flush only.
 * AC4DEC_FXPOLA=1  -> clamp every pend add to +-32768 + count sats.
 * Claimed states keyed by pointer (<=8 channels); first-seen order
 * matches push order (frame-major ch0..ch5, CODUMP-pinned). */
static struct {
    const imdct_state *st;
    double peak;
    long sat;
    long adds;
} ola_stats[8];
static int ola_nst = 0;

static void ola_note(const imdct_state *st, double v, int saturated) {
    int k = -1;
    for (int i = 0; i < ola_nst; i++)
        if (ola_stats[i].st == st) {
            k = i;
            break;
        }
    if (k < 0) {
        if (ola_nst >= 8)
            return;
        k = ola_nst++;
        ola_stats[k].st = st;
        ola_stats[k].peak = 0.0;
        ola_stats[k].sat = 0;
        ola_stats[k].adds = 0;
    }
    double a = v < 0.0 ? -v : v;
    if (a > ola_stats[k].peak)
        ola_stats[k].peak = a;
    ola_stats[k].adds++;
    if (saturated)
        ola_stats[k].sat++;
}

int imdct_push(imdct_state *st, const double *X, int n, int pn, int nn,
               double alpha, double palpha, double nalpha, double *pcm) {
    static double y[2 * IMDCT_MAXN];
    static double w[2 * IMDCT_MAXN];
    int H = st->H > 0 ? st->H : IMDCT_MAXN / 2;
    if (n <= 0 || n > IMDCT_MAXN || (n & 1) || n > 2 * H)
        return -1;
    if (pn <= 0) {
        pn = (st->prev_n > 0) ? st->prev_n : n;
        if (st->prev_n <= 0)
            palpha = alpha;
    }
    imdct_imdct(X, y, n);
    imdct_bridge(w, n, pn, nn, alpha, palpha, nalpha);
    /* AC4DEC_EDGEA (duet26 Track-I; s52: REVERTED to probe-gated,
     * default OFF — the land-ea01 default-ON regressed the banked
     * bit-exact DONE-gate (514 mains + 51 mains pins, duet15B);
     * first-push re-taper perturbs frames 0-1 on beds. The IMS
     * cold-start heal (IMS-f0ts00 239->87) stays available
     * explicitly; re-land scoped, never global-default, per the
     * c49ce11 precedent. Unset/0 = legacy (own-alpha). */
    {
        const char *ep = getenv("AC4DEC_EDGEA");
        double ea = 0.1;
        int eaon = 0;
        if (ep && ep[0]) {
            double v = strtod(ep, NULL);
            if (v == 0.0)
                eaon = 0;
            else if (v > 0.0 && v < 20.0) {
                eaon = 1;
                ea = v;
            }
        }
        if (eaon && st->started == 0 && n > 0 && n <= IMDCT_MAXN) {
            if (ea > 0.0 && ea < 20.0) {
                static double ew[2 * IMDCT_MAXN];
                static double aw[2 * IMDCT_MAXN];
                imdct_kbd(ew, n, ea);
                imdct_kbd(aw, n, alpha);
                for (int i = 0; i < n; i++)
                    if (aw[i] > 1e-12)
                        w[i] = w[i] / aw[i] * ew[i];
            }
        }
    }
    /* OLAW (duet34 divergence-localizer, temporary): applied window
     * (post-EDGEA) + IMDCT output, env AC4DEC_OLADUMP=1. Pairs with
     * OLAP (same push order). */
    if (getenv("AC4DEC_OLADUMP")) {
        fprintf(stderr, "OLAW n=%d T=%d base=%d W=", n, st->T, st->base);
        for (int i = 0; i < 2 * n; i++)
            fprintf(stderr, "%.17g%c", w[i], i + 1 < 2 * n ? ',' : '\n');
        fprintf(stderr, "OLAY n=%d Y=", n);
        for (int i = 0; i < 2 * n; i++)
            fprintf(stderr, "%.17g%c", y[i], i + 1 < 2 * n ? ',' : '\n');
    }
    /* Accumulate tile-centered span [T-n/2, T+3n/2) into pending. */
    int fxpola = 0, olapeek = 0;
    {
        const char *fp = getenv("AC4DEC_FXPOLA");
        const char *pp = getenv("AC4DEC_OLAPEEK");
        fxpola = (fp && fp[0] == '1');
        olapeek = (pp && pp[0] == '1') || fxpola;
    }
    int s0 = st->T - n / 2;
    for (int i = 0; i < 2 * n; i++) {
        int idx = s0 + i - st->base;
        if (idx >= 0 && idx < 4 * IMDCT_MAXN) {
            st->pend[idx] += y[i] * w[i];
            if (fxpola || olapeek) {
                int sat = 0;
                if (fxpola) {
                    if (st->pend[idx] > 32768.0) {
                        st->pend[idx] = 32768.0;
                        sat = 1;
                    } else if (st->pend[idx] < -32768.0) {
                        st->pend[idx] = -32768.0;
                        sat = 1;
                    }
                }
                ola_note(st, st->pend[idx], sat);
            }
        } else
            return -1; /* H violated (caller bug); state kept */
    }
    /* Finalize t <= T-H: all covering blocks pushed. Cut t < 0. */
    int k = 0;
    int lim = st->T - H;
    if (lim >= st->base) {
        int adv = lim - st->base + 1;
        for (int i = 0; i < adv; i++)
            if (st->base + i >= 0)
                pcm[k++] = st->pend[i];
        int left = 4 * IMDCT_MAXN - adv;
        memmove(st->pend, st->pend + adv, (size_t)left * sizeof(double));
        memset(st->pend + left, 0, (size_t)adv * sizeof(double));
        st->base += adv;
    }
    st->T += n;
    st->prev_n = n;
    st->started++;
    return k;
}

int imdct_flush(imdct_state *st, double *pcm) {
    { /* FXPOLA/OLAPEEK report (env-gated, stderr only). */
        const char *fp = getenv("AC4DEC_FXPOLA");
        const char *pp = getenv("AC4DEC_OLAPEEK");
        if ((fp && fp[0] == '1') || (pp && pp[0] == '1')) {
            for (int i = 0; i < ola_nst; i++)
                if (ola_stats[i].st == st)
                    fprintf(stderr, "OLAPEEK st%d peak=%.6g sat=%ld adds=%ld\n",
                            i, ola_stats[i].peak, ola_stats[i].sat,
                            ola_stats[i].adds);
        }
    }
    /* Emit [base, T): exact tiles; cut t < 0 and ring past T. */
    int k = 0;
    if (st->T > st->base) {
        int adv = st->T - st->base;
        for (int i = 0; i < adv; i++)
            if (st->base + i >= 0)
                pcm[k++] = st->pend[i];
    }
    imdct_reset(st);
    return k;
}

short imdct_d2s16(double v) {
    /* Oracle-pinned (drop e2-3, 22/22 probe): scale x32768,
     * round-half-even (lrint), clip to int16. NaN -> 0. */
    if (isnan(v))
        return 0;
    long q = lrint(v * 32768.0);
    if (q > 32767)
        q = 32767;
    if (q < -32768)
        q = -32768;
    return (short)q;
}

float imdct_d2f32(double v) {
    if (v > 1.0)
        v = 1.0;
    if (v < -1.0)
        v = -1.0;
    return (float)v;
}
