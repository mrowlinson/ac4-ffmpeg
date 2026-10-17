/*
 * imdct.h -- clean AC-4 IMDCT + KBD window + overlap-add core.
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

/* imdct.h -- clean AC-4 IMDCT + KBD window + overlap-add core.
 *
 * Lane e2, wave5. C11, zero deps. Math from public DSP textbook
 * formulas (tmp/wave05/WEB-e2.md) + clean specs spec/mdct-sizes.md
 * (sizes, KBD alphas) and spec/imdct-probe-beds-flex2w3.md (geometry).
 *
 * LATTICE (derived, see spec/imdct-clean-e2.md S2): blocks own tiles
 * (= their length N; tiles tile the timeline, N coeffs -> N PCM, zero
 * priming). Each block's window spans 2N CENTERED on its tile
 * (extends N/2 each side). Equal neighbors overlap 50% (standard KBD).
 * Unequal neighbors use flat/taper/zero bridge halves (widths
 * (N-T)/2, T, (N-T)/2 with T = min(N, neighbor)); tapers use the
 * block's OWN row alpha (ANSWER-e2-1 settled). This lattice pairs
 * MDCT alias terms structurally at every boundary; the naive
 * window-at-block-start lattice cannot switch short->long (proven).
 *
 * Cold start = zero state (silence before t=0): first tile fades in
 * over N/2; NO fade shaper (ANSWER-e2-3 verdict (b)). Stream end
 * fades symmetrically; flush cuts the trailing ring at the last tile
 * end (oracle emits exact tiles).
 *
 * EXACT (wave6 e3-pcm, drop e3-pcm-1): tapers use the taper-length
 * row alpha, so switch boundaries are exactly PR (1e-12 gated);
 * uniform path 1e-12 exact.
 */
#ifndef AVCODEC_IMDCT_H
#define AVCODEC_IMDCT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max block length: 8192 (192 kHz x4 full, mdct-sizes S3). */
#define IMDCT_MAXN 8192

/* KBD alpha rows, longest -> shortest (mdct-sizes S5). */
#define IMDCT_ALPHA_0 3.0
#define IMDCT_ALPHA_1 4.0
#define IMDCT_ALPHA_2 4.5
#define IMDCT_ALPHA_3 5.0
#define IMDCT_ALPHA_4 6.0

/* Map block length n + family full length full to KBD alpha.
 * row = log2(full/n) clamped 0..4. Returns 0 on unknown family. */
double imdct_alpha(int n, int full);

/* Modified Bessel I0(x), double. */
double imdct_bessel_i0(double x);

/* KBD window of length 2*n into w[0..2n). n in [1, IMDCT_MAXN].
 * Power-complementary: w[j]^2 + w[n-1-j]^2 == 1 (j < n). */
void imdct_kbd(double *w, int n, double alpha);

/* Bridge window for tile length n with prev/next tile lengths pn/nn
 * (pass n for equal/absent edge; <=0 = same as n) and KBD alphas
 * self/prev/next. Tile-centered: left half covers
 * [tile-n/2, tile+n/2), right half [tile+n/2, tile+3n/2).
 * Shorter-neighbor side: flat((n-T)/2) + taper(T) + zeros((n-T)/2),
 * T = min, taper = KBD-2T at the TAPER-LENGTH row alpha (wave6 e3-pcm,
 * drop e3-pcm-1: both sides share alpha(min), 1e-15 complementary;
 * ANSWER-e2-1 own-row refuted at 3.4e-2). palpha/nalpha = neighbor-row
 * alphas, used for the taper on the shorter-neighbor side. Equal sides:
 * plain KBD half. Switch boundaries are exactly PR (gated in test 4). */
void imdct_bridge(double *w, int n, int pn, int nn, double alpha,
                  double palpha, double nalpha);

/* Forward MDCT (test/roundtrip only): x[2n] windowed input -> X[n]. */
void imdct_mdct(const double *x, double *X, int n);

/* Inverse MDCT: X[n] -> y[2n] (unwindowed; caller applies window). */
void imdct_imdct(const double *X, double *y, int n);

/* One-channel OLA state. Large (~260KB); allocate static/global,
 * never on stack. */
typedef struct {
    double pend[4 * IMDCT_MAXN]; /* pending sums; pend[0] = sample base */
    int base;    /* sample index of pend[0] */
    int H;       /* max overhang = max tile/2 (set via imdct_set_max) */
    int T;       /* next tile start */
    int prev_n;  /* prev tile length (0 = cold) */
    int started; /* tiles pushed */
} imdct_state;

/* Set max tile length for the stream (<= IMDCT_MAXN, even) and reset
 * to cold state. Must be >= every pushed n. Default if never called:
 * IMDCT_MAXN (latency max_n/2). */
void imdct_set_max(imdct_state *st, int max_n);

/* Reset to cold (zero) state, keeping H. */
void imdct_reset(imdct_state *st);

/* Push one tile: coeffs X[n], tile length n (even, <= 2*H), prev/next
 * tile lengths pn/nn (<=0 = n), KBD alphas self/prev/next.
 * Appends up to IMDCT_MAXN finalized PCM samples (double) to pcm;
 * returns the count (>= 0), or -1 on contract violation (n odd,
 * n > 2*H, n out of range). Pre-stream samples (t < 0) are cut, so
 * total emitted + flushed over a stream = exact tile sum. */
int imdct_push(imdct_state *st, const double *X, int n, int pn, int nn,
               double alpha, double palpha, double nalpha, double *pcm);

/* Flush at stream end: emits pending [base, stream end), cutting the
 * trailing ring at the last tile end. Returns samples emitted.
 * Resets state (keeps H). */
int imdct_flush(imdct_state *st, double *pcm);

/* Saturating converters. */
short imdct_d2s16(double v); /* oracle rule: lrint(x*32768)+clip (drop e2-3) */
float imdct_d2f32(double v); /* clamp to [-1,1] */

#ifdef __cplusplus
}
#endif

#endif /* AVCODEC_IMDCT_H */
