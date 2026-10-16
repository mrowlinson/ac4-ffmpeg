/* acpl.c — A-CPL data parse + diff-decode + dequant (duet3-dec-writerB).
 * Own code from ETSI P1 Tables 61/63/64/65 (syntax), Pseudocode 8
 * (book select), Pseudocode 121 (diff decode), Tables 203-206
 * (dequant). Probe phase: parse + print (alpha_dq, beta_dq); QMF
 * module (decorr/ducker/Pseudocode 116) lands next.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <complex.h>
#include <math.h>
#include <stdint.h>

#include "acpl.h"
#include "entropy.h"
#include "huffman_tables.h"
#include "qmf.h"

/* Table 203: fine alpha (alpha_q 0..32 -> ibeta, dq). */
static const int AF_IB[33] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0,
};
static const double AF_DQ[33] = {
    -2.0, -1.809375, -1.6375, -1.484375, -1.35, -1.234375, -1.1375,
    -1.059375, -1.0, -0.940625, -0.8625, -0.765625, -0.65, -0.515625,
    -0.3625, -0.190625, 0.0, 0.190625, 0.3625, 0.515625, 0.65,
    0.765625, 0.8625, 0.940625, 1.0, 1.059375, 1.1375, 1.234375,
    1.35, 1.484375, 1.6375, 1.809375, 2.0,
};

/* Table 204: fine beta (beta_q 0..8 x ibeta 0..8). */
static const double BF_DQ[9][9] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0.2375, 0.2035449, 0.1729297, 0.1456543, 0.1217188, 0.101123,
     0.0838672, 0.0699512, 0.059375},
    {0.55, 0.4713672, 0.4004688, 0.3373047, 0.281875, 0.2341797,
     0.1942188, 0.1619922, 0.1375},
    {0.9375, 0.8034668, 0.6826172, 0.5749512, 0.4804688, 0.3991699,
     0.3310547, 0.276123, 0.234375},
    {1.4, 1.199844, 1.019375, 0.8585938, 0.7175, 0.5960938, 0.494375,
     0.4123438, 0.35},
    {1.9375, 1.660498, 1.410742, 1.1882319, 0.9929688, 0.8249512,
     0.6841797, 0.5706543, 0.484375},
    {2.55, 2.18543, 1.856719, 1.563867, 1.306875, 1.085742, 0.9004688,
     0.7510547, 0.6375},
    {3.2375, 2.7746389, 2.357305, 1.985498, 1.659219, 1.378467,
     1.143242, 0.9535449, 0.809375},
    {4.0, 3.4281249, 2.9124999, 2.453125, 2.05, 1.703125, 1.4125,
     1.178125, 1.0},
};

/* Table 205: coarse alpha (alpha_q 0..16 -> ibeta, dq). */
static const int AC_IB[17] = {0, 1, 2, 3, 4, 3, 2, 1, 0,
                              1, 2, 3, 4, 3, 2, 1, 0};
static const double AC_DQ[17] = {-2.0, -1.6375, -1.35, -1.1375, -1.0,
                                 -0.8625, -0.65, -0.3625, 0.0, 0.3625,
                                 0.65, 0.8625, 1.0, 1.1375, 1.35,
                                 1.6375, 2.0};

/* Table 206: coarse beta (beta_q 0..4 x ibeta 0..4). */
static const double BC_DQ[5][5] = {
    {0, 0, 0, 0, 0},
    {0.55, 0.4004688, 0.281875, 0.1942188, 0.1375},
    {1.4, 1.019375, 0.7175, 0.494375, 0.35},
    {2.55, 1.856719, 1.306875, 0.9004688, 0.6375},
    {4.0, 2.9124999, 2.05, 1.4125, 1.0},
};

/* Pseudocode 121 time-state: prev q per (module, set, band).
 * 4 modules (immersive b_5fronts 0) x ALPHA/BETA x 15 bands.
 * Zero-init per decode (acpl_reset); first-frame DIFF_TIME adds
 * onto zeros (encoder-emitted FREQ first in practice). */
void acpl_reset(acpl_state_t *st) {
    memset(st, 0, sizeof *st);
}

/* ---- Decorrelator + ducker + interpolation (P1 5.7.7.2/5.7.7.3/5.7.7.4,
 * own C port of tools/acpl_decorr_bed.py). ---- */

/* Table 197: sb -> pbid per nbands (cols 15/12/9/7). */
static int sb_to_pb_c(int sb, int nbands) {
    static int8_t map[64][4];
    static int done = 0;
    if (!done) {
        static const int8_t rows[][6] = {
            {0, 0, 0, 0, 0, 0}, {1, 1, 1, 1, 1, 1},
            {2, 2, 2, 2, 2, 2}, {3, 3, 3, 3, 3, 2},
            {4, 4, 4, 4, 3, 3}, {5, 5, 5, 4, 4, 3},
            {6, 6, 6, 5, 4, 3}, {7, 7, 7, 5, 5, 3},
            {8, 8, 8, 6, 5, 4}, {9, 10, 9, 6, 6, 4},
            {11, 13, 10, 7, 6, 4}, {14, 17, 11, 8, 7, 5},
            {18, 22, 12, 9, 7, 5}, {23, 34, 13, 10, 8, 6},
            {35, 63, 14, 11, 8, 6},
        };
        for (size_t r = 0; r < sizeof rows / sizeof rows[0]; r++)
            for (int sb = rows[r][0]; sb <= rows[r][1]; sb++)
                for (int c = 0; c < 4; c++)
                    map[sb][c] = rows[r][2 + c];
        done = 1;
    }
    int c = nbands == 15 ? 0 : nbands == 12 ? 1 : nbands == 9 ? 2 : 3;
    return map[sb][c];
}

static void region_of_c(int sb, int *delay, int *flen) {
    if (sb <= 6) {
        *delay = 7;
        *flen = 7;
    } else if (sb <= 22) {
        *delay = 10;
        *flen = 4;
    } else {
        *delay = 12;
        *flen = 2;
    }
}

/* Tables 199/200/201: a[] per region per dx (a[0] == 1). */
static const double CK0[3][8] = {
    {1.0, 0.5306, -0.4533, -0.6248, 0.0424, 0.4237, 0.4311, 0.1688},
    {1.0, -0.4178, 0.1082, -0.2368, -0.1014, -0.1052, -0.3528, 0.4665},
    {1.0, 0.4007, 0.4747, 0.2611, -0.1211, -0.4248, -0.2989, -0.1932},
};
static const double CK1[3][5] = {
    {1.0, 0.5561, -0.3039, -0.5024, -0.1850},
    {1.0, 0.0425, 0.3235, -0.1556, 0.4958},
    {1.0, -0.4361, 0.0345, 0.5215, -0.4178},
};
static const double CK2[3][3] = {
    {1.0, 0.5773, 0.3321},
    {1.0, 0.2327, -0.3901},
    {1.0, -0.6057, 0.3804},
};

#define DUCK_ALPHA 0.76592833836465
#define DUCK_ALPHA_SMOOTH 0.25
#define DUCK_GAMMA 1.5
#define DUCK_EPSILON 1.0e-9

void acpl_decorr_init(acpl_decorr_t *st, int dx) {
    memset(st, 0, sizeof *st);
    st->dx = dx;
}

/* One QMF frame through Dx + ducker. X/Y = [n_ts][64]. */
void acpl_decorr_apply(const double complex (*X)[QMF_NSB], int n_ts,
                       acpl_decorr_t *st, int duck,
                       double complex (*Y)[QMF_NSB]) {
    int dx = st->dx;
    for (int sb = 0; sb < QMF_NSB; sb++) {
        int delay, flen;
        region_of_c(sb, &delay, &flen);
        const double *a =
            flen == 7 ? CK0[dx] : flen == 4 ? CK1[dx] : CK2[dx];
        double complex *xh = st->xh[sb], *yh = st->yh[sb];
        for (int ts = 0; ts < n_ts; ts++) {
            double complex xs = X[ts][sb];
            for (int i = 18; i > 0; i--)
                xh[i] = xh[i - 1];
            xh[0] = xs;
            double complex acc = a[flen] * xh[delay];
            for (int i = 1; i <= flen; i++)
                acc += a[flen - i] * xh[delay + i] - a[i] * yh[i - 1];
            acc /= a[0];
            Y[ts][sb] = acc;
            for (int i = 6; i > 0; i--)
                yh[i] = yh[i - 1];
            yh[0] = acc;
        }
    }
    /* Ducker (pseudos 112/113/114): input-frame band energy, peak-decay
     * + smoothers + gain, applied per subband (nbands 15 rows). */
    double gain[15];
    for (int pb = 0; pb < 15; pb++) {
        double e = 0;
        for (int sb = 0; sb < 64; sb++) {
            if (sb_to_pb_c(sb, 15) != pb)
                continue;
            for (int ts = 0; ts < n_ts; ts++) {
                double complex v = X[ts][sb];
                e += creal(v) * creal(v) + cimag(v) * cimag(v);
            }
        }
        double peak_prev = DUCK_ALPHA * st->p_peak[pb];
        double peak = peak_prev < e ? e : peak_prev;
        st->p_peak[pb] = peak;
        double sm = (1.0 - DUCK_ALPHA_SMOOTH) * st->p_smooth[pb] +
                    DUCK_ALPHA_SMOOTH * e;
        st->p_smooth[pb] = sm;
        double spd = (1.0 - DUCK_ALPHA_SMOOTH) * st->p_spd[pb] +
                     DUCK_ALPHA_SMOOTH * (peak - e);
        st->p_spd[pb] = spd;
        gain[pb] = 1.0;
        if (DUCK_GAMMA * spd > sm)
            gain[pb] = sm / (DUCK_GAMMA * (spd + DUCK_EPSILON));
    }
    if (duck) {
        for (int ts = 0; ts < n_ts; ts++)
            for (int sb = 0; sb < QMF_NSB; sb++)
                Y[ts][sb] *= gain[sb_to_pb_c(sb, 15)];
    }
}

double acpl_interp(const double p[2][15], const double prev[64],
                   int npset, int sb, int ts, int n_ts, int itype,
                   const int timeslot[2], int nbands) {
    int pb = sb_to_pb_c(sb, nbands);
    if (itype == 0) {
        if (npset == 1) {
            double d = p[0][pb] - prev[sb];
            return prev[sb] + (ts + 1) * d / n_ts;
        }
        int h = n_ts / 2;
        if (ts < h) {
            double d = p[0][pb] - prev[sb];
            return prev[sb] + (ts + 1) * d / h;
        }
        double d = p[1][pb] - p[0][pb];
        return p[0][pb] + (ts - h + 1) * d / (n_ts - h);
    }
    if (npset == 1)
        return ts < timeslot[0] ? prev[sb] : p[0][pb];
    if (ts < timeslot[0])
        return prev[sb];
    if (ts < timeslot[1])
        return p[0][pb];
    return p[1][pb];
}

/* Pseudocode 116 module (FULL and PARTIAL qmf_band). */
void acpl_module(const double a[2][15], const double b[2][15],
                 const double pa[64], const double pbv[64], int npset,
                 int nbands, int itype, const int timeslot[2],
                 int qmf_band, const double complex (*X0)[QMF_NSB],
                 const double complex (*X1)[QMF_NSB],
                 const double complex (*Y)[QMF_NSB], int n_ts,
                 double complex (*Z0)[QMF_NSB],
                 double complex (*Z1)[QMF_NSB]) {
    for (int sb = 0; sb < QMF_NSB; sb++) {
        for (int ts = 0; ts < n_ts; ts++) {
            double ia = acpl_interp(a, pa, npset, sb, ts, n_ts, itype,
                                    timeslot, nbands);
            double ib = acpl_interp(b, pbv, npset, sb, ts, n_ts, itype,
                                    timeslot, nbands);
            double complex x0 = X0[ts][sb], x1 = X1[ts][sb], y = Y[ts][sb];
            if (sb < qmf_band) {
                Z0[ts][sb] = 0.5 * (x0 + x1);
                Z1[ts][sb] = 0.5 * (x0 - x1);
            } else {
                Z0[ts][sb] = 0.5 * (x0 * (1 + ia) + y * ib);
                Z1[ts][sb] = 0.5 * (x0 * (1 - ia) - y * ib);
            }
        }
    }
}

static int ebr_getbit_acpl(void *ctx) {
    ac4_ebr_t *b = ctx;
    if (b->pos >= b->nbits) {
        b->err = 1;
        return -1;
    }
    size_t p = b->pos++;
    return (b->buf[p >> 3] >> (7 - (p & 7))) & 1;
}

/* huff_decode_diff: codeword -> signed diff (F0 = absolute). */
static int huff_diff(ac4_ebr_t *b, int book) {
    if (book < 0 || book >= HUFFMAN_NBOOKS) {
        b->err = 1;
        return 0;
    }
    const huffman_book_t *bk = &huffman_books[book];
    int idx = hcb_decode(bk, ebr_getbit_acpl, b);
    if (idx < 0) {
        b->err = 1;
        return 0;
    }
    return idx - bk->off;
}

/* Parse one acpl_huff_data (Table 65) into raw[0..nbands). */
static int parse_huff(ac4_ebr_t *b, int dt, int qm, int nbands, int *raw,
                      int *diff_type) {
    unsigned d = ebr_u(b, 1);
    if (b->err)
        return -1;
    *diff_type = (int)d;
    if (d == 0) { /* DIFF_FREQ */
        int b0 = huffman_acpl(dt, qm, 0);
        raw[0] = huff_diff(b, b0);
        if (b->err)
            return -1;
        int bd = huffman_acpl(dt, qm, 1);
        for (int i = 1; i < nbands; i++) {
            raw[i] = huff_diff(b, bd);
            if (b->err)
                return -1;
        }
    } else { /* DIFF_TIME */
        int bt = huffman_acpl(dt, qm, 2);
        for (int i = 0; i < nbands; i++) {
            raw[i] = huff_diff(b, bt);
            if (b->err)
                return -1;
        }
    }
    return 0;
}

/* Parse one acpl_data_1ch (Table 61) + diff-decode + dequant into
 * *out. Returns 0 ok, -1 bit error (*note set). */
int acpl_parse_1ch(ac4_ebr_t *b, int nbands, int fine, int module,
                   acpl_mod_t *out, const char **note, acpl_state_t *st) {
    unsigned interp = ebr_u(b, 1);
    unsigned cod = ebr_u(b, 1);
    if (b->err) {
        *note = "acpl-trunc";
        return -1;
    }
    int npset = (int)cod + 1;
    out->itype = (int)interp;
    out->npset = npset;
    out->nbands = nbands;
    out->timeslot[0] = out->timeslot[1] = 0;
    if (interp) {
        for (int ps = 0; ps < npset; ps++)
            out->timeslot[ps] = (int)ebr_u(b, 5);
        if (b->err) {
            *note = "acpl-trunc";
            return -1;
        }
    }
    if (module < 0 || module > 3)
        module = 0;
    for (int ps = 0; ps < npset; ps++) {
        int araw[15], braw[15], adt = 0, bdt = 0;
        if (parse_huff(b, 0, fine, nbands, araw, &adt) ||
            parse_huff(b, 1, fine, nbands, braw, &bdt)) {
            *note = "acpl-huff";
            return -1;
        }
        /* Pseudocode 121 diff-decode. */
        int aq[15], bq[15];
        if (adt == 0) {
            aq[0] = araw[0];
            for (int i = 1; i < nbands; i++)
                aq[i] = aq[i - 1] + araw[i];
        } else {
            for (int i = 0; i < nbands; i++)
                aq[i] = st->diff.prev_q[module][0][i] + araw[i];
        }
        if (bdt == 0) {
            bq[0] = braw[0];
            for (int i = 1; i < nbands; i++)
                bq[i] = bq[i - 1] + braw[i];
        } else {
            for (int i = 0; i < nbands; i++)
                bq[i] = st->diff.prev_q[module][1][i] + braw[i];
        }
        for (int i = 0; i < nbands; i++) {
            st->diff.prev_q[module][0][i] = aq[i];
            st->diff.prev_q[module][1][i] = bq[i];
        }
        st->diff.prev_have[module][0] = st->diff.prev_have[module][1] = 1;
        if (getenv("AC4DEC_ACPLPROBE")) {
            fprintf(stderr, "AQ m=%d ps=%d aq:", module, ps);
            for (int i = 0; i < nbands; i++)
                fprintf(stderr, " %d", aq[i]);
            fprintf(stderr, "\nAQ m=%d ps=%d bq:", module, ps);
            for (int i = 0; i < nbands; i++)
                fprintf(stderr, " %d", bq[i]);
            fprintf(stderr, "\n");
        }
        int amax = fine ? 32 : 16, bmax = fine ? 8 : 4;
        for (int i = 0; i < 15; i++) {
            out->a[ps][i] = 0;
            out->b[ps][i] = 0;
        }
        for (int i = 0; i < nbands; i++) {
            int ib = 0;
            if (aq[i] >= 0 && aq[i] <= amax) {
                out->a[ps][i] = fine ? AF_DQ[aq[i]] : AC_DQ[aq[i]];
                ib = fine ? AF_IB[aq[i]] : AC_IB[aq[i]];
            }
            if (bq[i] >= 0 && bq[i] <= bmax)
                out->b[ps][i] =
                    fine ? BF_DQ[bq[i]][ib] : BC_DQ[bq[i]][ib];
        }
        out->adt[ps] = adt;
        out->bdt[ps] = bdt;
    }
    return 0;
}

int acpl_probe_1ch(ac4_ebr_t *b, int nbands, int fine, int module,
                   long frameno, const char **note, acpl_state_t *st) {
    acpl_mod_t m;
    if (acpl_parse_1ch(b, nbands, fine, module, &m, note, st))
        return -1;
    for (int ps = 0; ps < m.npset; ps++) {
        fprintf(stderr, "ACPL f=%ld m=%d ps=%d nb=%d %s adt=%d bdt=%d\n",
                frameno, module, ps, nbands, fine ? "FINE" : "COARSE",
                m.adt[ps], m.bdt[ps]);
        fprintf(stderr, "  a:");
        for (int i = 0; i < nbands; i++)
            fprintf(stderr, " %.3f", m.a[ps][i]);
        fprintf(stderr, "\n  b:");
        for (int i = 0; i < nbands; i++)
            fprintf(stderr, " %.4f", m.b[ps][i]);
        fprintf(stderr, "\n");
    }
    return 0;
}

void acpl_imm_init(acpl_imm_t *s) {
    memset(s, 0, sizeof *s);
    for (int i = 0; i < 4; i++)
        qmf_init(&s->qa[i]);
    for (int i = 0; i < 6; i++)
        qmf_init(&s->qs[i]);
    static const int dx[4] = {0, 0, 1, 1};
    for (int i = 0; i < 4; i++)
        acpl_decorr_init(&s->dd[i], dx[i]);
    s->init = 1;
    s->dump_frame = 0;
}

/* QMF work buffers: per-instance acpl_state_t work area (param). */

void acpl_imm_frame(acpl_imm_t *s, const double *D, const double *E,
                    const double *F, const double *G,
                    const acpl_mod_t *P, int spf, double fold_div,
                    double *Ls, double *Rs, double *H, acpl_state_t *st) {
    const double sq2 = 1.4142135623730951;
    int nslots = qmf_slots(spf);
    if (nslots < 0 || nslots > 32 || spf > 2048)
        return;
    const double *IN[4];
    IN[0] = D;
    IN[1] = E;
    IN[2] = F;
    IN[3] = G;
    /* Pre x2 (time) + QMF analysis. */
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < spf; j++)
            st->wk.T[i][j] = 2.0 * IN[i][j];
        qmf_analysis(st->wk.T[i], nslots, &s->qa[i], st->wk.X[i]);
    }
    /* y0..y3 = duck(decorr(xNin)). */
    for (int i = 0; i < 4; i++)
        acpl_decorr_apply(st->wk.X[i], nslots, &s->dd[i], 1, st->wk.Y[i]);
    /* Modules (ACPL_2: in1 = 0; FULL: qmf_band = 0). */
    for (int m = 0; m < 4; m++)
        acpl_module(P[m].a, P[m].b, s->pa[m], s->pbv[m], P[m].npset,
                    P[m].nbands, P[m].itype, P[m].timeslot, 0, st->wk.X[m],
                    st->wk.ZERO, st->wk.Y[m], nslots, st->wk.Z[m][0], st->wk.Z[m][1]);
    /* Post sqrt(2) on z5,z7,z9,z10,z11,z12 (skip z6/z8: backs). */
    static const int ZM[6] = {0, 1, 2, 2, 3, 3};
    static const int ZO[6] = {0, 0, 0, 1, 0, 1};
    for (int i = 0; i < 6; i++) {
        for (int ts = 0; ts < nslots; ts++)
            for (int sb = 0; sb < QMF_NSB; sb++)
                st->wk.Z[ZM[i]][ZO[i]][ts][sb] *= sq2;
        qmf_synthesis(st->wk.Z[ZM[i]][ZO[i]], nslots, &s->qs[i], st->wk.O[i]);
    }
    /* Fold: Ls = z5 + (z9+z10)/fold_div (gain-indexed). */
    if (!(fold_div > 0.0))
        fold_div = sq2;
    for (int j = 0; j < spf; j++) {
        Ls[j] = st->wk.O[0][j] + (st->wk.O[2][j] + st->wk.O[3][j]) / fold_div;
        Rs[j] = st->wk.O[1][j] + (st->wk.O[4][j] + st->wk.O[5][j]) / fold_div;
    }
    if (H) {
        /* AB-20 heights: Tfl=z9 Tfr=z11 Tbl=z10 Tbr=z12 (CROSSED:
         * z10=back-left, z11=front-right). st->wk.O[2..5] = z9,z10,z11,z12
         * post-sqrt(2) synth (ZM/ZO order). NO fold. */
        for (int j = 0; j < spf; j++) {
            H[0 * spf + j] = st->wk.O[2][j];
            H[1 * spf + j] = st->wk.O[4][j];
            H[2 * spf + j] = st->wk.O[3][j];
            H[3 * spf + j] = st->wk.O[5][j];
        }
    }
    { /* ZDUMP (H-SPLIT forensics): z-synth time + y @ sb2/3. */
        const char *zp = getenv("AC4DEC_ZDUMP");
        if (zp) {
            char p1[1024], p2[1024];
            snprintf(p1, sizeof p1, "%s.z.f64", zp);
            snprintf(p2, sizeof p2, "%s.y2.f64", zp);
            FILE *f1 = fopen(p1, "ab");
            if (f1) {
                fwrite(st->wk.O[2], sizeof(double), (size_t)spf, f1);
                fwrite(st->wk.O[3], sizeof(double), (size_t)spf, f1);
                fwrite(st->wk.O[4], sizeof(double), (size_t)spf, f1);
                fwrite(st->wk.O[5], sizeof(double), (size_t)spf, f1);
                fclose(f1);
            }
            FILE *f2 = fopen(p2, "ab");
            if (f2) {
                for (int m = 2; m < 4; m++)
                    for (int sb = 2; sb < 4; sb++)
                        for (int ts = 0; ts < nslots; ts++) {
                            double re = creal(st->wk.Y[m][ts][sb]);
                            double im = cimag(st->wk.Y[m][ts][sb]);
                            double xe = creal(st->wk.X[m][ts][sb]);
                            double xi = cimag(st->wk.X[m][ts][sb]);
                            fwrite(&re, sizeof re, 1, f2);
                            fwrite(&im, sizeof im, 1, f2);
                            fwrite(&xe, sizeof xe, 1, f2);
                            fwrite(&xi, sizeof xi, 1, f2);
                        }
                fclose(f2);
            }
        }
        s->dump_frame++;
    }
    /* Interp prev tails (Pseudocode 110). */
    for (int m = 0; m < 4; m++) {
        int lp = P[m].npset - 1;
        for (int sb = 0; sb < 64; sb++) {
            int pb = sb_to_pb_c(sb, P[m].nbands);
            s->pa[m][sb] = P[m].a[lp][pb];
            s->pbv[m][sb] = P[m].b[lp][pb];
        }
    }
}

void acpl_5x_init(acpl_5x_t *s) {
    memset(s, 0, sizeof *s);
    for (int i = 0; i < 2; i++)
        qmf_init(&s->qa[i]);
    for (int i = 0; i < 4; i++)
        qmf_init(&s->qs[i]);
    static const int dx[2] = {0, 1};
    for (int i = 0; i < 2; i++)
        acpl_decorr_init(&s->dd[i], dx[i]);
    s->init = 1;
}

void acpl_5x_frame(acpl_5x_t *s, const double *L, const double *R,
                   const acpl_mod_t *P, int spf, double *oL,
                   double *oR, double *oLs, double *oRs, acpl_state_t *st) {
    /* (was: shared file-static IW_* scratch; now per-instance via st) */
    /* Mirror of acpl_imm_frame for 2 modules + straight outputs
     * (AB-12, NO fold). Uses the per-instance work area (first-2 /
     * first-4 subsets; paths never interleave within one decode). */
    const double sq2 = 1.4142135623730951;
    int nslots = qmf_slots(spf);
    if (nslots < 0 || nslots > 32 || spf > 2048)
        return;
    const double *IN[2];
    IN[0] = L;
    IN[1] = R;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < spf; j++)
            st->wk.T[i][j] = 2.0 * IN[i][j];
        qmf_analysis(st->wk.T[i], nslots, &s->qa[i], st->wk.X[i]);
    }
    for (int i = 0; i < 2; i++)
        acpl_decorr_apply(st->wk.X[i], nslots, &s->dd[i], 1, st->wk.Y[i]);
    for (int m = 0; m < 2; m++)
        acpl_module(P[m].a, P[m].b, s->pa[m], s->pbv[m], P[m].npset,
                    P[m].nbands, P[m].itype, P[m].timeslot, 0, st->wk.X[m],
                    st->wk.ZERO, st->wk.Y[m], nslots, st->wk.Z[m][0], st->wk.Z[m][1]);
    /* Post sqrt(2) on z1,z3 only (AB-12 straight map). */
    for (int ts = 0; ts < nslots; ts++)
        for (int sb = 0; sb < QMF_NSB; sb++) {
            st->wk.Z[0][1][ts][sb] *= sq2;
            st->wk.Z[1][1][ts][sb] *= sq2;
        }
    qmf_synthesis(st->wk.Z[0][0], nslots, &s->qs[0], st->wk.O[0]);
    qmf_synthesis(st->wk.Z[1][0], nslots, &s->qs[1], st->wk.O[1]);
    qmf_synthesis(st->wk.Z[0][1], nslots, &s->qs[2], st->wk.O[2]);
    qmf_synthesis(st->wk.Z[1][1], nslots, &s->qs[3], st->wk.O[3]);
    for (int j = 0; j < spf; j++) {
        oL[j] = st->wk.O[0][j];
        oR[j] = st->wk.O[1][j];
        oLs[j] = st->wk.O[2][j];
        oRs[j] = st->wk.O[3][j];
    }
    for (int m = 0; m < 2; m++) {
        int lp = P[m].npset - 1;
        for (int sb = 0; sb < 64; sb++) {
            int pb = sb_to_pb_c(sb, P[m].nbands);
            s->pa[m][sb] = P[m].a[lp][pb];
            s->pbv[m][sb] = P[m].b[lp][pb];
        }
    }
}
