/*
 * ac4_src.h -- s5.11 output-sample-count rules (N5 rate-semantics fix).
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

/* ac4_src.h -- s5.11 output-sample-count rules (N5 rate-semantics fix).
 *
 * The decode core runs on the internal grid (e4_spf[rate] samples per
 * frame); the s5.11 sample-rate converter maps each internal frame to
 * base_fs x sf_mult PCM. Exact output samples per frame:
 *
 *   L = base*sf*fps_den/fps_num        (fri 0-12; fps rows exact)
 *   L = 2048*sf                        (fri 13 free-run; A0 r2 rule)
 *
 * (Table E.1 sample_delta at sf=1; the sf factor is the natural
 * generalization -- E.1 tabulates base 48 kHz only. See AMBIGUITY.)
 *
 * Fractional rows emit integer counts straddling L, locked to the
 * sequence_counter (s5.11: "The decoder SHALL link the number of
 * output samples per frame to the sequence_counter", Table 70):
 *
 *   n(out) = floor((eff+1)*L) - floor(eff*L)
 *
 * with eff = seq, except a mid-stream seq==0 (splicer-marked splice
 * frame, never emitted by an encoder mid-run) advances the phase from
 * the previous frame instead of resetting it (s5.11 phi_t equation,
 * middle clause). The pattern repeats with period P = den/gcd(num,den)
 * (5 for Table 70's 29.97/59.94/119.88 rows); arithmetic runs in the
 * reduced residue so no unbounded counter can overflow.
 *
 * This rule reproduces Table 70 exactly (29.97: 1601,1602,1601,1602,
 * 1602 over eff 0-4; likewise the 800/801 and 400/401 rows) and
 * extends it consistently to every other fractional row.
 *
 * AMBIGUITY (spec, reported with the N5 fix): Table E.1 reserves the
 * 44.1 kHz rows 0-12 and shows no sf>1 column; clause 4.8.8 operates
 * fri!=13 at external 48/96/192 kHz only. The implemented rule follows
 * A0 (base_fs x sf_mult on all rows), which N5 judged spec-correct;
 * counts for fs=0/fri!=13 and sf>1 are the consistent extension, not
 * a tabulated value.
 */
#ifndef AVCODEC_AC4_SRC_H
#define AVCODEC_AC4_SRC_H

/* Max per-frame output count over all (fri, fs, sf): fri13/sf4 8192. */
#define AC4_SRC_OMAX 8192

/* Header-internal linkage: not every includer uses every helper
 * (LIB_ONLY skips the VER fps table row); stay -Wall-clean. */
#if defined(__GNUC__) || defined(__clang__)
#define AC4_SRC_UNUSED __attribute__((unused))
#else
#define AC4_SRC_UNUSED
#endif

/* Frame-rate rows 0-12, exact num/den fps (Table E.1 frame_rate). */
static const long AC4_SRC_UNUSED ac4_src_fps_num[13] = {
    24000, 24, 25, 30000, 30, 48000, 48,
    50, 60000, 60, 100, 120000, 120,
};
static const long AC4_SRC_UNUSED ac4_src_fps_den[13] = {
    1001, 1, 1, 1001, 1, 1001, 1,
    1, 1001, 1, 1, 1001, 1,
};

static long AC4_SRC_UNUSED ac4_src_gcd(long a, long b) {
    while (b != 0) {
        long t = a % b;
        a = b;
        b = t;
    }
    return a < 0 ? -a : a;
}

/* Exact output samples/frame L = num/den + pattern period per. fri is
 * 0-13, fs 0/1, sf 1/2/4 (callers validate; same trust as e4_spf[]). */
static void AC4_SRC_UNUSED ac4_src_params(int fri, int fs, int sf, long *num, long *den,
                           long *per) {
    long n, d;
    if (fri == 13) {
        n = 2048L * (long)sf;
        d = 1;
    } else {
        long base = fs ? 48000L : 44100L;
        n = base * (long)sf * ac4_src_fps_den[fri];
        d = ac4_src_fps_num[fri];
    }
    *num = n;
    *den = d;
    *per = d / ac4_src_gcd(n, d);
}

/* Output count for one frame. eff is the reduced phase residue
 * (0 <= eff < per); all terms non-negative so / truncates = floor. */
static long AC4_SRC_UNUSED ac4_src_nout(long num, long den, long eff) {
    return ((eff + 1) * num) / den - (eff * num) / den;
}

/* Reduced phase residue for this frame's sequence_counter. first is
 * nonzero for the first fed frame of the decoding session. */
static long AC4_SRC_UNUSED ac4_src_eff(long per, long prev, unsigned seq, int first) {
    if (seq == 0 && !first)
        return (prev + 1) % per;
    return (long)(seq % (unsigned)per);
}

/* ceil(num/den): max per-frame output count (buffer sizing). */
static long AC4_SRC_UNUSED ac4_src_ceil(long num, long den) {
    return (num + den - 1) / den;
}

/* Exact row frame rate (VER display). fri13 free-runs base/2048. */
static double AC4_SRC_UNUSED ac4_src_fps(int fri, int fs) {
    if (fri == 13)
        return (fs ? 48000.0 : 44100.0) / 2048.0;
    return (double)ac4_src_fps_num[fri] / (double)ac4_src_fps_den[fri];
}

#undef AC4_SRC_UNUSED

#endif /* AVCODEC_AC4_SRC_H */
