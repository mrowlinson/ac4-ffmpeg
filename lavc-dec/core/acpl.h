/*
 * acpl.h - AC-4 decoder core (in-tree port of dec-lib)
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

#ifndef AC4_ACPL_H
#define AC4_ACPL_H

#include <complex.h>

#include "entropy.h"
#include "qmf.h"

/* Per-instance ACPL state (streaming lib: was file-static prev_q /
 * prev_have + IW_* scratch in acpl.c; now lives in the heap ctx).
 * diff = Pseudocode-121 time state; wk = QMF frame scratch. */
typedef struct {
    int prev_q[4][2][15];
    int prev_have[4][2];
} acpl_diff_t;

typedef struct {
    double complex X[4][32][QMF_NSB];
    double complex Y[4][32][QMF_NSB];
    double complex Z[4][2][32][QMF_NSB];
    double complex ZERO[32][QMF_NSB];
    double T[4][2048];
    double O[6][2048];
} acpl_work_t;

typedef struct {
    acpl_diff_t diff;
    acpl_work_t wk;
} acpl_state_t;

/* A-CPL data parse + diff-decode + dequant (probe phase). */
void acpl_reset(acpl_state_t *st);

/* One parsed acpl_data_1ch: dequant (a,b) per (pset, pb). */
typedef struct {
    double a[2][15];
    double b[2][15];
    int npset;
    int nbands;
    int itype;
    int timeslot[2];
    int adt[2];
    int bdt[2];
} acpl_mod_t;

int acpl_parse_1ch(ac4_ebr_t *b, int nbands, int fine, int module,
                   acpl_mod_t *out, const char **note, acpl_state_t *st);
int acpl_probe_1ch(ac4_ebr_t *b, int nbands, int fine, int module,
                   long frameno, const char **note, acpl_state_t *st);

/* Decorrelator instance (one per module input; dx selects Tables
 * 199/200/201 coef rows). Zero-init via acpl_decorr_init. */
typedef struct {
    int dx;
    double complex xh[64][19];
    double complex yh[64][7];
    double p_smooth[15];
    double p_peak[15];
    double p_spd[15];
} acpl_decorr_t;

void acpl_decorr_init(acpl_decorr_t *st, int dx);
void acpl_decorr_apply(const double complex (*X)[QMF_NSB], int n_ts,
                       acpl_decorr_t *st, int duck,
                       double complex (*Y)[QMF_NSB]);
double acpl_interp(const double p[2][15], const double prev[64],
                   int npset, int sb, int ts, int n_ts, int itype,
                   const int timeslot[2], int nbands);

/* Pseudocode 116 module. a/b = [pset][pb] dequant params; pa/pbv =
 * prev-frame tails; X1 may alias a zero matrix (ACPL_2). */
void acpl_module(const double a[2][15], const double b[2][15],
                 const double pa[64], const double pbv[64], int npset,
                 int nbands, int itype, const int timeslot[2],
                 int qmf_band, const double complex (*X0)[QMF_NSB],
                 const double complex (*X1)[QMF_NSB],
                 const double complex (*Y)[QMF_NSB], int n_ts,
                 double complex (*Z0)[QMF_NSB],
                 double complex (*Z1)[QMF_NSB]);

/* Immersive (P2 5.5.2, ACPL_2, b_5fronts 0) post-pass state. */
typedef struct {
    qmf_state_t qa[4];
    qmf_state_t qs[6];
    acpl_decorr_t dd[4];
    double pa[4][64];
    double pbv[4][64];
    int init;
    int dump_frame;
} acpl_imm_t;

void acpl_imm_init(acpl_imm_t *s);

/* 5x (ASPX_ACPL_2) post-pass state: 2 modules ((2xL,0,y0-D0),
 * (2xR,0,y1-D1)), straight outputs (AB-12, NO fold). */
typedef struct {
    qmf_state_t qa[2];
    qmf_state_t qs[4];
    acpl_decorr_t dd[2];
    double pa[2][64];
    double pbv[2][64];
    int init;
} acpl_5x_t;

void acpl_5x_init(acpl_5x_t *s);
/* One frame: D,E,F,G time (spf) + params -> Ls,Rs time (spf).
 * x9/x10 = F/G (track reuse, first-hand: m2/m3 live inputs).
 * Fold: Ls = z5 + (z9+z10)/fold_div (fold_div = sqrt(2) at
 * height-gain index 1 (-3dB); 1.0 at index 0 (0dB); 2.0 at
 * index 2 (-6dB); duet6-dec-writerB Finding 12 ladder).
 * H (NULL-able): 4xspf height out (AB-20: Tfl=z9 Tfr=z11 Tbl=z10
 * Tbr=z12, post-sqrt(2) synth, NO fold) for the separate heights
 * artifact. NULL skips (mains path unchanged). */
void acpl_imm_frame(acpl_imm_t *s, const double *D, const double *E,
                    const double *F, const double *G,
                    const acpl_mod_t *P, int spf, double fold_div,
                    double *Ls, double *Rs, double *H, acpl_state_t *st);
/* One frame: L,R time (spf) + 2 params -> L,R,Ls,Rs time (spf).
 * (z0,z1) = Module(2xL,0,y0); (z2,z3) = Module(2xR,0,y1).
 * Outputs straight (AB-12): L=z0, R=z2, Ls=z1*sqrt(2),
 * Rs=z3*sqrt(2). C = x2 passthrough (caller/skeleton). */
void acpl_5x_frame(acpl_5x_t *s, const double *L, const double *R,
                   const acpl_mod_t *P, int spf, double *oL,
                   double *oR, double *oLs, double *oRs, acpl_state_t *st);

#endif
