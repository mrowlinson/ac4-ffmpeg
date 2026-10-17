/*
 * imdct.c -- clean AC-4 IMDCT + KBD window + overlap-add core.
 * Copyright (c) 2026 Michael Rowlinson
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* imdct.c -- clean AC-4 IMDCT + KBD window + overlap-add core.
 * Tile-centered lattice; see imdct.h.
 * Naive O(N^2) transforms (correctness first; FFT fast path later).
 */
#include "imdct.h"

#include "libavutil/mem.h"

#include <math.h>
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
 * w[0] ~ 0, w[L-1] ~ 1. L+1-point textbook kernel grid (i=0..L). */
static void taper_rise(double *w, int L, double alpha) {
    double denom = imdct_bessel_i0(M_PI * alpha);
    double Tot = 0.0, csum = 0.0;
    int last = L;
    double span = (double)L;
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
    /* Taper alpha = TAPER-LENGTH row: both sides of a bridge share
     * alpha(min), exactly power-complementary (1e-15). Mixed alphas
     * break complementarity (3.4e-2 error), so per-side "own row"
     * alphas are not used. palpha/nalpha are the neighbor-row
     * alphas (contract: caller passes imdct_alpha(pn)/
     * imdct_alpha(nn)); the taper side uses the neighbor's alpha
     * since taper length = neighbor length. */
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

/* ---- cos-kernel cache (bit-identical to the naive path) ----
 * The MDCT/IMDCT kernel cos(M_PI/n*(i+0.5+n/2.0)*(k+0.5)) depends
 * only on (n,i,k): identical across every block of a size, so it is
 * computed once per size and reused. Fill evaluates the EXACT same
 * expression in the same order as the naive loop, and the dot
 * products below keep the naive accumulation order, so cached
 * results are bit-identical to the uncached path (no fast-math).
 * Slots: 4 sizes (streams use 1-2; eviction is round-robin).
 * Sizes above 2048 bypass the cache (footprint cap; correctness
 * unaffected). The cache is process-lifetime (single-threaded
 * consumers; the wrapper serializes all lib calls). */
#define IMDCT_COS_NSLOT 4
#define IMDCT_COS_MAXN 2048
static struct {
    int n;
    double *m; /* [2n][n] row-major */
} cos_cache[IMDCT_COS_NSLOT];
static int cos_next = 0;

static const double *imdct_cosmat(int n) {
    if (n <= 0 || n > IMDCT_COS_MAXN || (n & 1))
        return NULL;
    for (int s = 0; s < IMDCT_COS_NSLOT; s++)
        if (cos_cache[s].n == n && cos_cache[s].m)
            return cos_cache[s].m;
    size_t nn = (size_t)n;
    double *m = av_malloc(2 * nn * nn * sizeof(double));
    if (!m)
        return NULL; /* OOM: caller falls back to naive */
    for (int i = 0; i < 2 * n; i++)
        for (int k = 0; k < n; k++)
            m[(size_t)i * nn + (size_t)k] =
                cos(M_PI / n * (i + 0.5 + n / 2.0) * (k + 0.5));
    av_free(cos_cache[cos_next].m);
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
    /* ETSI P62 literal: /N (P60-62 kernel check: 1e-15 exact at
     * N=64/256). */
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
    /* Accumulate tile-centered span [T-n/2, T+3n/2) into pending. */
    int s0 = st->T - n / 2;
    for (int i = 0; i < 2 * n; i++) {
        int idx = s0 + i - st->base;
        if (idx >= 0 && idx < 4 * IMDCT_MAXN) {
            st->pend[idx] += y[i] * w[i];
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
    /* Scale x32768, round-half-even (lrint), clip to int16.
     * NaN -> 0. */
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
