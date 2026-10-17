/*
 * entropy.c -- ASF spectral-frontend decode (see entropy.h).
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

/* entropy.c -- ASF spectral-frontend decode (see entropy.h).
 * Own implementation from the pinned tables; no ETSI prose copied.
 */
#include "entropy.h"

#include <math.h>
#include <string.h>

#include "huffman_tab.h"
#include "sfb_offsets.h"
#include "snf_tab.h"

void ebr_init(ac4_ebr_t *b, const uint8_t *buf, size_t len) {
    b->buf = buf;
    b->nbits = len * 8;
    b->pos = 0;
    b->err = 0;
}

unsigned ebr_u(ac4_ebr_t *b, int n) {
    unsigned v = 0;
    if (n < 0 || n > 32 || b->err) {
        b->err = 1;
        return 0;
    }
    for (int i = 0; i < n; i++) {
        if (b->pos >= b->nbits) {
            b->err = 1;
            return 0;
        }
        size_t p = b->pos++;
        v = (v << 1) | (unsigned)((b->buf[p >> 3] >> (7 - (p & 7))) & 1);
    }
    return v;
}

void ebr_align(ac4_ebr_t *b) {
    b->pos = (b->pos + 7) & ~(size_t)7;
    if (b->pos > b->nbits)
        b->err = 1;
}

/* ---- huffman ---- */

static int ebr_getbit(void *ctx) {
    ac4_ebr_t *b = ctx;
    if (b->pos >= b->nbits) {
        b->err = 1;
        return -1;
    }
    size_t p = b->pos++;
    return (b->buf[p >> 3] >> (7 - (p & 7))) & 1;
}

static int huff_idx(ac4_ebr_t *b, int book) {
    if (book < 0 || book >= HUFFMAN_NBOOKS) {
        b->err = 1;
        return -1;
    }
    return hcb_decode(&huffman_books[book], ebr_getbit, b);
}

/* ---- transform-length maps (Tables 99/100/103, 48k family) ---- */

/* Short-partial length for base>=1536 (Table 100, 48k rows). */
static int short_len_ge1536(int base, int idx) {
    static const int tab[3][4] = {
        {128, 256, 512, 1024}, /* 2048 */
        {120, 240, 480, 960},  /* 1920 */
        {96, 192, 384, 768},   /* 1536 */
    };
    int r = (base == 2048) ? 0 : (base == 1920) ? 1 : (base == 1536) ? 2 : -1;
    if (r < 0 || idx < 0 || idx > 3)
        return 0;
    return tab[r][idx];
}

/* Partial length for base<1536 (Table 103). */
static int short_len_lt1536(int base, int idx) {
    static const int tab[5][4] = {
        {128, 256, 512, 1024}, /* 1024 */
        {120, 240, 480, 960},  /* 960 */
        {96, 192, 384, 768},   /* 768 */
        {128, 256, 512, 0},    /* 512: idx3 banned */
        {96, 192, 384, 0},     /* 384: idx3 banned */
    };
    int r = (base == 1024)   ? 0
            : (base == 960)   ? 1
            : (base == 768)   ? 2
            : (base == 512)   ? 3
            : (base == 384)   ? 4
                              : -1;
    if (r < 0 || idx < 0 || idx > 3)
        return 0;
    return tab[r][idx];
}

/* Table 106 n_msfb_bits ladder (48k), verified first-hand. */
static int msfb_bits(int len) {
    switch (len) {
    case 2048:
    case 1920:
    case 1536:
    case 1024:
    case 960:
    case 768:
    case 512:
    case 480:
    case 384:
        return 6;
    case 256:
    case 240:
    case 192:
        return 5;
    case 128:
    case 120:
    case 96:
        return 4;
    default:
        return 0;
    }
}

/* n_grp_bits ladder (Tables 109/110). */
static int grp_bits(int base, int i0, int i1, int is_long) {
    if (is_long && base >= 1536)
        return 0;
    if (base >= 1536) {
        static const int m[4][4] = {
            {15, 10, 8, 7}, {10, 7, 4, 3}, {8, 4, 3, 1}, {7, 3, 1, 1}};
        if (i0 < 0 || i0 > 3 || i1 < 0 || i1 > 3)
            return -1;
        return m[i0][i1];
    }
    int idx = i0;
    if (base == 1024 || base == 960 || base == 768) {
        static const int m[4] = {7, 3, 1, 0};
        return (idx >= 0 && idx <= 3) ? m[idx] : -1;
    }
    if (base == 512 || base == 384) {
        static const int m[4] = {3, 1, 0, 0};
        return (idx >= 0 && idx <= 3) ? m[idx] : -1;
    }
    return -1;
}

int asf_info_parse(ac4_ebr_t *b, int frame_len_base, asf_info_t *info) {
    memset(info, 0, sizeof *info);
    int base = frame_len_base;
    int is_long = 1, t0 = 0, t1 = 0;
    if (base >= 1536) {
        is_long = (int)ebr_u(b, 1) == 0 ? 0 : 1;
        if (!is_long) {
            t0 = (int)ebr_u(b, 2);
            t1 = (int)ebr_u(b, 2);
        }
    } else {
        is_long = 0;
        t0 = t1 = (int)ebr_u(b, 2);
    }
    if (b->err)
        return AC4E_OVERRUN;
    int diff = (base >= 1536 && !is_long && t0 != t1);
    /* max_sfb widths are per-index (4.3.6.2.1 Table 106: "depends on
     * the transform length"): t=3/0 needs 6b/4b -- shared-6b reads
     * ms=49/59 with cap 14 and misaligns. Same-index frames behave
     * exactly as before. */
    int l0 = base, l1 = base;
    if (!is_long) {
        l0 = base >= 1536 ? short_len_ge1536(base, t0)
                          : short_len_lt1536(base, t0);
        l1 = base >= 1536 ? short_len_ge1536(base, t1)
                          : short_len_lt1536(base, t1);
        if (l0 <= 0 || l1 <= 0)
            return AC4E_SYNTAX;
    }
    int mw0 = msfb_bits(l0);
    if (mw0 <= 0) {
        return AC4E_SYNTAX;
    }
    info->max_sfb[0] = (int)ebr_u(b, mw0);
    info->max_sfb[1] = info->max_sfb[0];
    if (diff) {
        int mw1 = msfb_bits(l1);
        if (mw1 <= 0)
            return AC4E_SYNTAX;
        info->max_sfb[1] = (int)ebr_u(b, mw1);
    }
    int ng = grp_bits(base, t0, t1, is_long);
    if (ng < 0)
        return AC4E_SYNTAX;
    for (int i = 0; i < ng; i++)
        info->grouping[i] = (int)ebr_u(b, 1);
    if (b->err)
        return AC4E_OVERRUN;
    /* Pseudocode 3: windows + groups. */
    int nw = 1, ngr = 1;
    info->win_to_group[0] = 0;
    int nw0 = 0;
    if (!is_long) {
        nw = ng + 1;
        if (diff) {
            nw0 = 1 << (3 - t0);
            for (int i = ng; i >= nw0; i--)
                info->grouping[i] = info->grouping[i - 1];
            info->grouping[nw0 - 1] = 0;
            nw++;
        }
        for (int i = 0; i < nw - 1; i++) {
            if (info->grouping[i] == 0)
                ngr++;
            info->win_to_group[i + 1] = ngr - 1;
        }
    }
    if (nw > AC4E_MAXWIN || ngr > AC4E_MAXGRP) {
        return AC4E_SYNTAX;
    }
    /* Pseudocode 2/5: per-group transform index + max_sfb check. */
    for (int g = 0; g < ngr; g++) {
        int tidx;
        if (base >= 1536) {
            if (is_long) {
                tidx = 4;
            } else if (!diff) {
                tidx = t0;
            } else {
                tidx = (g < info->win_to_group[nw0]) ? t0 : t1;
            }
        } else {
            tidx = t0;
        }
        info->grp_tlidx[g] = tidx;
        int ms = (tidx == 4) ? info->max_sfb[0]
                 : (!diff)   ? info->max_sfb[0]
                 : (g < info->win_to_group[nw0]) ? info->max_sfb[0]
                                                : info->max_sfb[1];
        /* ms==0 legal: silence frame, no coded bands; downstream
         * loops are all ms-bounded = no-op. */
        if (ms < 0 || ms >= AC4E_MAXBANDS) {
            return AC4E_SYNTAX;
        }
        int wlen = (tidx == 4) ? base
                   : base >= 1536 ? short_len_ge1536(base, tidx)
                                  : short_len_lt1536(base, tidx);
        if (wlen <= 0 || !sfb_offsets48(wlen)) {
            return AC4E_SYNTAX;
        }
        if (ms > sfb_nbands48(wlen)) {
            return AC4E_SYNTAX;
        }
    }
    for (int w = 0; w < nw; w++) {
        int g = info->win_to_group[w];
        int tidx = info->grp_tlidx[g];
        info->win_len[w] = (tidx == 4) ? base
                           : base >= 1536 ? short_len_ge1536(base, tidx)
                                          : short_len_lt1536(base, tidx);
        info->num_win_in_group[g]++;
    }
    info->b_long = is_long;
    info->tl_idx[0] = t0;
    info->tl_idx[1] = t1;
    info->ngrpbits = ng;
    info->num_windows = nw;
    info->num_groups = ngr;
    (void)nw0;
    return AC4E_OK;
}

/* ---- section + spectral ---- */

static int grp_maxsfb(const asf_info_t *info, int g) {
    int base_ge = info->win_len[0] > 0; /* always true; keep simple */
    (void)base_ge;
    int tidx = info->grp_tlidx[g];
    if (tidx == 4 || info->tl_idx[0] == info->tl_idx[1])
        return info->max_sfb[0];
    /* different framing: group in first half? recompute boundary */
    int nw0 = 1 << (3 - info->tl_idx[0]);
    return (g < info->win_to_group[nw0]) ? info->max_sfb[0]
                                         : info->max_sfb[1];
}

/* ---- SNF noise RNG (clause 5.2.8.3 state/step + 5.1.4.2 reseed) ----
 * Own code from the clause pseudocode numbers; table from
 * snf_tab.h (generated). State bytes wrap mod 256 (masked
 * explicitly). The clause step spells two increments in `x = x++`
 * form; read as plain increments (the bare `x++` lines beside them
 * show the author's increment intent; a store-back reading would
 * make those two lines silent no-ops). */
typedef struct {
    unsigned off_a, off_b, state_idx, cur_idx; /* kept 0..255 */
} snf_rng_t;

static void snf_rng_reset(snf_rng_t *s, unsigned seq) {
    int start = 255 * (int)(seq % 256);
    int off_a = start % 255;
    int off_b = (start / 255) % 256;
    int state_idx = (off_a + 1) * ((off_a + 2) + 2 * off_b) / 2 +
                    255 * off_b * (255 + off_b) / 2;
    if (start % 130560 >= 65280)
        state_idx += 128;
    s->off_a = (unsigned)off_a;
    s->off_b = (unsigned)off_b;
    s->state_idx = (unsigned)(state_idx % 256);
    s->cur_idx =
        (unsigned)((off_a * (off_a + 1) / 2 + off_b * 32386) % 256);
}

static float snf_rng_next(snf_rng_t *s) {
    float f1 = SNF_RANDOM_NOISE[s->cur_idx];
    float f2 = SNF_RANDOM_NOISE[s->state_idx];
    float fres = f1 + f2;
    s->off_a = (s->off_a + 1) & 0xffu;
    s->state_idx = (s->state_idx + 1) & 0xffu;
    if (s->off_a == 255) {
        s->cur_idx = (s->cur_idx + 1) & 0xffu;
        s->off_b = (s->off_b + 1) & 0xffu;
        s->off_a = 0;
    }
    s->cur_idx = (s->cur_idx + s->off_a) & 0xffu;
    s->state_idx = (s->state_idx + s->off_b + s->off_a) & 0xffu;
    return fres;
}

int asf_chan_decode(ac4_ebr_t *b, const asf_info_t *info, int frame_len_base,
                    unsigned seq, asf_chan_t *out) {
    (void)frame_len_base;
    memset(out, 0, sizeof *out);
    int ngr = info->num_groups;
    /* Per-group scratch: section books, band books, peaks. */
    static int sec_book[AC4E_MAXGRP][AC4E_MAXBANDS];
    static int sec_start[AC4E_MAXGRP][AC4E_MAXBANDS];
    static int sec_end[AC4E_MAXGRP][AC4E_MAXBANDS];
    static int nsec_lsf[AC4E_MAXGRP];
    static int nsec_full[AC4E_MAXGRP];
    static int band_book[AC4E_MAXGRP][AC4E_MAXBANDS];
    static int band_peak[AC4E_MAXGRP][AC4E_MAXBANDS];
    /* Bitstream-order quantized lines, per group. */
    static int qspec[AC4E_MAXGRP][AC4E_MAXLINES];
    static double gainv[AC4E_MAXGRP][AC4E_MAXBANDS];
    memset(sec_book, 0, sizeof sec_book);
    memset(sec_start, 0, sizeof sec_start);
    memset(sec_end, 0, sizeof sec_end);
    memset(nsec_lsf, 0, sizeof nsec_lsf);
    memset(nsec_full, 0, sizeof nsec_full);
    memset(band_book, 0, sizeof band_book);
    memset(band_peak, 0, sizeof band_peak);
    memset(qspec, 0, sizeof qspec);
    memset(gainv, 0, sizeof gainv);

    /* Table 39: sections. */
    for (int g = 0; g < ngr; g++) {
        int ms = grp_maxsfb(info, g);
        int tidx = info->grp_tlidx[g];
        int nsectb = (tidx <= 2) ? 3 : 5;
        int esc = (nsectb == 3) ? 7 : 31;
        int wlen = info->win_len[0];
        for (int w = 0; w < info->num_windows; w++)
            if (info->win_to_group[w] == g)
                wlen = info->win_len[w];
        int cut = sfb_nbands48(wlen); /* num_sfb_48 split */
        int k = 0, i = 0;
        nsec_lsf[g] = 0;
        while (k < ms) {
            int book = (int)ebr_u(b, 4);
            if (b->err)
                return AC4E_OVERRUN;
            if (book > 11)
                return AC4E_SYNTAX;
            int slen = 1, incr;
            do {
                incr = (int)ebr_u(b, nsectb);
                slen += incr;
            } while (incr == esc);
            if (b->err)
                return AC4E_OVERRUN;
            if (k + slen > ms || i + 1 >= AC4E_MAXBANDS)
                return AC4E_SYNTAX;
            int s0 = k, e0 = k + slen;
            if (s0 < cut && e0 >= cut) {
                nsec_lsf[g] = i + 1;
                if (e0 > cut) {
                    sec_book[g][i] = book;
                    sec_start[g][i] = s0;
                    sec_end[g][i] = cut;
                    i++;
                    sec_book[g][i] = book;
                    sec_start[g][i] = cut;
                    sec_end[g][i] = e0;
                    i++;
                } else {
                    sec_book[g][i] = book;
                    sec_start[g][i] = s0;
                    sec_end[g][i] = e0;
                    i++;
                }
            } else {
                sec_book[g][i] = book;
                sec_start[g][i] = s0;
                sec_end[g][i] = e0;
                i++;
            }
            for (int s = s0; s < e0; s++)
                band_book[g][s] = book;
            k = e0;
        }
        int nsec = i;
        nsec_full[g] = nsec;
        if (nsec_lsf[g] == 0)
            nsec_lsf[g] = nsec;
    }

    /* Table 40: spectral. */
    for (int g = 0; g < ngr; g++) {
        int wlen = info->win_len[0], nw = 0;
        for (int w = 0; w < info->num_windows; w++)
            if (info->win_to_group[w] == g) {
                wlen = info->win_len[w];
                nw++;
            }
        const int *so = sfb_offsets48(wlen);
        int goff = 0; /* group line-base in bitstream order */
        for (int gg = 0; gg < g; gg++) {
            int gl = info->win_len[0], gn = 0;
            for (int w = 0; w < info->num_windows; w++)
                if (info->win_to_group[w] == gg) {
                    gl = info->win_len[w];
                    gn++;
                }
            goff += gn * sfb_offsets48(gl)[grp_maxsfb(info, gg)];
        }
        for (int i = 0; i < nsec_lsf[g]; i++) {
            int book = sec_book[g][i];
            if (book == 0)
                continue;
            int dim = (book <= 4) ? 4 : 2;
            int sgned = (book == 1 || book == 2 || book == 5 || book == 6);
            int l0 = goff + nw * so[sec_start[g][i]];
            int l1 = goff + nw * so[sec_end[g][i]];
            int k = l0;
            while (k < l1) {
                int idx = huff_idx(b, book - 1);
                if (b->err)
                    return AC4E_OVERRUN;
                if (idx < 0)
                    return AC4E_HUFF;
                int m[4];
                if (dim == 4) {
                    /* Pseudocode 19: line1 = MSB. */
                    int r = idx;
                    m[3] = r % 3;
                    m[2] = (r / 3) % 3;
                    m[1] = (r / 9) % 3;
                    m[0] = r / 27;
                    int off = (book <= 2) ? 1 : 0;
                    for (int d = 0; d < 4; d++)
                        m[d] -= off;
                } else {
                    int mod = (book <= 6)   ? 9
                              : (book <= 8) ? 8
                              : (book <= 10) ? 13
                                             : 17;
                    int off = (book <= 6) ? 4 : 0;
                    m[1] = (idx % mod) - off;
                    m[0] = (idx / mod) - off;
                }
                if (!sgned) {
                    for (int d = 0; d < dim; d++) {
                        if (m[d] == 0)
                            continue;
                        int s = (int)ebr_u(b, 1);
                        if (b->err)
                            return AC4E_OVERRUN;
                        if (s)
                            m[d] = -m[d];
                    }
                }
                if (book == 11) {
                    for (int d = 0; d < 2; d++) {
                        if (m[d] == 16 || m[d] == -16) {
                            int nn = 0;
                            for (;;) {
                                int u = (int)ebr_u(b, 1);
                                if (b->err)
                                    return AC4E_OVERRUN;
                                if (!u)
                                    break;
                                if (++nn > 8)
                                    return AC4E_SYNTAX;
                            }
                            unsigned pay = ebr_u(b, nn + 4);
                            if (b->err)
                                return AC4E_OVERRUN;
                            int e = (int)((1u << (nn + 4)) + pay);
                            m[d] = (m[d] < 0) ? -e : e;
                        }
                    }
                }
                for (int d = 0; d < dim && k < l1; d++, k++)
                    qspec[g][k - goff] = m[d];
            }
        }
        /* exact peaks per band. */
        for (int s = 0; s < grp_maxsfb(info, g); s++)
            band_peak[g][s] = 0;
        for (int i = 0; i < nsec_lsf[g]; i++) {
            int l0 = goff + nw * so[sec_start[g][i]];
            int l1 = goff + nw * so[sec_end[g][i]];
            for (int k = l0; k < l1; k++) {
                int am = qspec[g][k - goff];
                am = am < 0 ? -am : am;
                int sb = sec_start[g][i];
                for (int s = sec_start[g][i]; s < sec_end[g][i]; s++) {
                    int bl0 = goff + nw * so[s];
                    int bl1 = goff + nw * so[s + 1];
                    if (k >= bl0 && k < bl1) {
                        sb = s;
                        break;
                    }
                }
                if (am > band_peak[g][sb])
                    band_peak[g][sb] = am;
            }
        }
    }
    /* Table 41 + Pseudocode 21: scalefactors + scale. */
    int sf = (int)ebr_u(b, 8);
    if (b->err)
        return AC4E_OVERRUN;
    int first = 0;
    static double scaled[AC4E_MAXGRP][AC4E_MAXLINES];
    memset(scaled, 0, sizeof scaled);
    for (int g = 0; g < ngr; g++) {
        int wlen = info->win_len[0], nw = 0;
        for (int w = 0; w < info->num_windows; w++)
            if (info->win_to_group[w] == g) {
                wlen = info->win_len[w];
                nw++;
            }
        const int *so = sfb_offsets48(wlen);
        int ms = grp_maxsfb(info, g);
        int cap = sfb_nbands48(wlen);
        if (ms > cap)
            ms = cap;
        for (int s = 0; s < ms; s++) {
            if (band_book[g][s] == 0 || band_peak[g][s] == 0)
                continue;
            if (first) {
                int idx = huff_idx(b, HUFFMAN_SCALE_FAC);
                if (b->err)
                    return AC4E_OVERRUN;
                if (idx < 0)
                    return AC4E_HUFF;
                sf += idx - 60;
            } else {
                first = 1;
            }
            if (sf < 0 || sf > 255)
                return AC4E_SYNTAX;
            double gv = pow(2.0, 0.25 * ((double)sf - 100.0));
            gainv[g][s] = gv;
            int bl0 = nw * so[s], bl1 = nw * so[s + 1];
            for (int k = bl0; k < bl1; k++) {
                int q = qspec[g][k];
                double aq = q < 0 ? -(double)q : (double)q;
                double rec = (q < 0 ? -1.0 : 1.0) * pow(aq, 4.0 / 3.0);
                scaled[g][k] = gv * rec;
            }
        }
    }

    /* Table 42 + clause 5.1.4.2: spectral noise fill. Zero bands
     * (book 0 or peak 0, the mirror of the scalefactor condition)
     * each carry one SNF delta codeword; nonzero bands refresh the
     * running log-level from their own energy. Parse order and
     * application order are the same groups x bands scan, fused
     * here. Levels live in log2(mean-square) units; fill lines get
     * unit-variance RNG draws scaled by 2^(level/2). RNG reseeds
     * per ASF channel section from the TOC sequence counter. */
    int snf = (int)ebr_u(b, 1);
    if (b->err)
        return AC4E_OVERRUN;
    if (snf) {
        snf_rng_t rng;
        snf_rng_reset(&rng, seq);
        double prev = -1000.0; /* no reference band seen yet */
        for (int g = 0; g < ngr; g++) {
            int wlen = info->win_len[0], nw = 0;
            for (int w = 0; w < info->num_windows; w++)
                if (info->win_to_group[w] == g) {
                    wlen = info->win_len[w];
                    nw++;
                }
            const int *so = sfb_offsets48(wlen);
            int ms = grp_maxsfb(info, g);
            int cap = sfb_nbands48(wlen);
            if (ms > cap)
                ms = cap;
            for (int s = 0; s < ms; s++) {
                int bl0 = nw * so[s], bl1 = nw * so[s + 1];
                if (band_book[g][s] != 0 && band_peak[g][s] != 0) {
                    double e = 0.0;
                    for (int k = bl0; k < bl1; k++)
                        e += scaled[g][k] * scaled[g][k];
                    if (e > 0.0)
                        prev = 1.44269504 * log(e / (double)(bl1 - bl0));
                    continue;
                }
                int idx = huff_idx(b, HUFFMAN_SNF);
                if (b->err)
                    return AC4E_OVERRUN;
                if (idx < 0)
                    return AC4E_HUFF;
                int delta = idx - 17;
                if (delta == -17)
                    continue; /* no-fill escape: level untouched */
                prev += (double)delta;
                double lin = pow(2.0, 0.5 * prev);
                for (int k = bl0; k < bl1; k++)
                    scaled[g][k] = (double)snf_rng_next(&rng) * lin;
            }
        }
    }

    /* Pseudocode 25: ungroup into per-window spectra. */
    out->nwin = info->num_windows;
    int win = 0;
    for (int g = 0; g < ngr; g++) {
        int wlen = info->win_len[0], nw = 0;
        for (int w = 0; w < info->num_windows; w++)
            if (info->win_to_group[w] == g) {
                wlen = info->win_len[w];
                nw++;
            }
        const int *so = sfb_offsets48(wlen);
        int ms = grp_maxsfb(info, g);
        int k = 0;
        for (int s = 0; s < ms; s++) {
            int bw = so[s + 1] - so[s];
            for (int w = 0; w < nw; w++) {
                for (int l = so[s]; l < so[s + 1]; l++) {
                    out->co[win + w][l] = scaled[g][k++];
                    /* qspec fill order (Pseudocode 25, 5.1.5.2) is band-outer,
                     * window-interleaved: t = w*bw + (l-so[s]). */
                    out->qw[win + w][l] =
                        qspec[g][nw * so[s] + w * bw + (l - so[s])];
                }
            }
        }
        for (int w = 0; w < nw; w++)
            out->win_len[win + w] = wlen;
        win += nw;
    }
    return AC4E_OK;
}

/* ---- SAP: chparam_info parse (Tables 47/48) + Pseudocode 59 ---- */

static void sap_ident(double m[4]) {
    m[0] = 1.0;
    m[1] = 0.0;
    m[2] = 0.0;
    m[3] = 1.0;
}

static void sap_ms(double m[4]) {
    m[0] = 1.0;
    m[1] = 1.0;
    m[2] = 1.0;
    m[3] = -1.0;
}

int ac4e_chparam_parse(ac4_ebr_t *b, const asf_info_t *info,
                       ac4e_sap_t *sap) {
    memset(sap, 0, sizeof *sap);
    int ngr = info->num_groups;
    if (ngr <= 0 || ngr > AC4E_MAXGRP)
        return AC4E_SYNTAX;
    for (int g = 0; g < ngr; g++)
        for (int s = 0; s < AC4E_MAXBANDS; s++)
            sap_ident(sap->mat[g][s]);
    int mode = (int)ebr_u(b, 2);
    if (b->err)
        return AC4E_OVERRUN;
    sap->sap_mode = mode;
    if (mode == 0) {
        return AC4E_OK; /* all identity */
    }
    if (mode == 1) {
        for (int g = 0; g < ngr; g++) {
            int ms = grp_maxsfb(info, g);
            if (ms < 0 || ms > AC4E_MAXBANDS)
                return AC4E_SYNTAX;
            for (int s = 0; s < ms; s++) {
                int u = (int)ebr_u(b, 1);
                if (b->err)
                    return AC4E_OVERRUN;
                if (u)
                    sap_ms(sap->mat[g][s]);
            }
        }
        return AC4E_OK;
    }
    if (mode == 2) {
        for (int g = 0; g < ngr; g++) {
            int ms = grp_maxsfb(info, g);
            if (ms < 0 || ms > AC4E_MAXBANDS)
                return AC4E_SYNTAX;
            for (int s = 0; s < ms; s++)
                sap_ms(sap->mat[g][s]);
        }
        return AC4E_OK;
    }
    /* mode 3: sap_data. */
    static int used[AC4E_MAXGRP][AC4E_MAXBANDS];
    static int alpha[AC4E_MAXGRP][AC4E_MAXBANDS];
    memset(used, 0, sizeof used);
    memset(alpha, 0, sizeof alpha);
    int all = (int)ebr_u(b, 1);
    if (b->err)
        return AC4E_OVERRUN;
    for (int g = 0; g < ngr; g++) {
        int ms = grp_maxsfb(info, g);
        if (ms < 0 || ms > AC4E_MAXBANDS)
            return AC4E_SYNTAX;
        if (all) {
            for (int s = 0; s < ms; s++)
                used[g][s] = 1;
        } else {
            for (int s = 0; s < ms; s += 2) {
                int u = (int)ebr_u(b, 1);
                if (b->err)
                    return AC4E_OVERRUN;
                used[g][s] = u;
                if (s + 1 < ms)
                    used[g][s + 1] = u;
            }
        }
    }
    int dct = 0;
    if (ngr != 1) {
        dct = (int)ebr_u(b, 1);
        if (b->err)
            return AC4E_OVERRUN;
    }
    for (int g = 0; g < ngr; g++) {
        int ms = grp_maxsfb(info, g);
        for (int s = 0; s < ms; s += 2) {
            if (!used[g][s])
                continue;
            int dq = huff_idx(b, HUFFMAN_SCALE_FAC);
            if (b->err)
                return AC4E_OVERRUN;
            if (dq < 0)
                return AC4E_HUFF;
            int delta = dq - 60;
            /* max_sfb_prev: group 0's max at start, then the running
             * previous group's max (Pseudocode 59 loop tail). */
            int prev = (g == 0) ? ms : grp_maxsfb(info, g - 1);
            int cd = (g == 0 || ms != prev) ? 0 : dct;
            if (cd) {
                alpha[g][s] = alpha[g - 1][s] + delta;
            } else if (s == 0) {
                alpha[g][s] = delta;
            } else {
                alpha[g][s] = alpha[g][s - 2] + delta;
            }
            if (s + 1 < ms)
                alpha[g][s + 1] = alpha[g][s];
        }
    }
    /* Pseudocode 59 tail: alpha -> gain -> matrix. */
    for (int g = 0; g < ngr; g++) {
        int ms = grp_maxsfb(info, g);
        for (int s = 0; s < ms; s++) {
            if (!used[g][s])
                continue;
            double gain = (double)alpha[g][s] * 0.1;
            sap->mat[g][s][0] = 1.0 + gain;
            sap->mat[g][s][1] = 1.0;
            sap->mat[g][s][2] = 1.0 - gain;
            sap->mat[g][s][3] = -1.0;
        }
    }
    return AC4E_OK;
}

/* Clause 5.3.3.2: O0 = a*I0 + b*I1, O1 = c*I0 + d*I1 per tile.
 * Tiles address ungrouped window lines via the same (group, band)
 * walk as the ungroup step: window base accumulates group sizes. */
void ac4e_sap_apply(const asf_info_t *info, const ac4e_sap_t *sap,
                    asf_chan_t *ch0, asf_chan_t *ch1) {
    int win = 0;
    for (int g = 0; g < info->num_groups; g++) {
        int wlen = info->win_len[0], nw = 0;
        for (int w = 0; w < info->num_windows; w++)
            if (info->win_to_group[w] == g) {
                wlen = info->win_len[w];
                nw++;
            }
        const int *so = sfb_offsets48(wlen);
        if (!so)
            return;
        int ms = grp_maxsfb(info, g);
        if (ms < 0 || ms >= AC4E_MAXBANDS)
            return;
        for (int s = 0; s < ms; s++) {
            const double *m = sap->mat[g][s];
            if (m[0] == 1.0 && m[1] == 0.0 && m[2] == 0.0 &&
                m[3] == 1.0)
                continue;
            for (int w = 0; w < nw; w++) {
                for (int l = so[s]; l < so[s + 1]; l++) {
                    double i0 = ch0->co[win + w][l];
                    double i1 = ch1->co[win + w][l];
                    ch0->co[win + w][l] = m[0] * i0 + m[1] * i1;
                    ch1->co[win + w][l] = m[2] * i0 + m[3] * i1;
                }
            }
        }
        win += nw;
    }
}
