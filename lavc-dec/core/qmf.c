/*
 * qmf.c -- complex QMF analysis/synthesis.
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

/* qmf.c -- complex QMF analysis/synthesis.
 * Own code from P1 pseudocodes 65/66: 64 subbands, 640-tap QWIN
 * prototype, 640-sample analysis memory, 1280-sample synthesis
 * memory. f64 throughout.
 */
#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "qmf.h"
#include "qwin_tab.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void qmf_init(qmf_state_t *s) {
    memset(s, 0, sizeof *s);
}

int qmf_slots(int frame_length) {
    switch (frame_length) {
    case 2048: return 32;
    case 1920: return 30;
    case 1536: return 24;
    case 1024: return 16;
    case 960: return 15;
    case 768: return 12;
    case 512: return 8;
    case 384: return 6;
    default: return -1;
    }
}

/* Precomputed modulation (constant): AN[sb][n] = exp(j w e(n)) with
 * e(0) = -1, e(n) = 2n-1; SY[sb][n] = exp(j w (2n-255))/64. */
static double complex AN[QMF_NSB][2 * QMF_NSB];
static double complex SY[QMF_NSB][2 * QMF_NSB];
static int tab_init = 0;

static void tab_ensure(void) {
    if (tab_init)
        return;
    for (int sb = 0; sb < QMF_NSB; sb++) {
        double w = M_PI / 128.0 * (sb + 0.5);
        for (int n = 0; n < 2 * QMF_NSB; n++) {
            double e = (n == 0) ? -1.0 : (double)(2 * n - 1);
            AN[sb][n] = cexp(I * w * e);
            SY[sb][n] = cexp(I * w * (double)(2 * n - 255)) / 64.0;
        }
    }
    tab_init = 1;
}

void qmf_analysis(const double *pcm, int nslots, qmf_state_t *s,
                  double complex (*Q)[QMF_NSB]) {
    tab_ensure();
    double *filt = s->afilt;
    for (int ts = 0; ts < nslots; ts++) {
        for (int sb = QMF_NW - 1; sb >= QMF_NSB; sb--)
            filt[sb] = filt[sb - QMF_NSB];
        size_t base = (size_t)ts * QMF_NSB;
        for (int sb = QMF_NSB - 1; sb >= 0; sb--)
            filt[sb] = pcm[base + QMF_NSB - 1 - (size_t)sb];
        double u[2 * QMF_NSB];
        for (int n = 0; n < 2 * QMF_NSB; n++)
            u[n] = filt[n] * QWIN_C[n] + filt[n + 128] * QWIN_C[n + 128] +
                   filt[n + 256] * QWIN_C[n + 256] +
                   filt[n + 384] * QWIN_C[n + 384] +
                   filt[n + 512] * QWIN_C[n + 512];
        for (int sb = 0; sb < QMF_NSB; sb++) {
            double complex acc = 0.0;
            for (int n = 0; n < 2 * QMF_NSB; n++)
                acc += u[n] * AN[sb][n];
            Q[ts][sb] = acc;
        }
    }
}

void qmf_synthesis(const double complex (*Q)[QMF_NSB], int nslots,
                   qmf_state_t *s, double *pcm) {
    tab_ensure();
    double *filt = s->sfilt;
    const int NF = 20 * QMF_NSB, NH = 2 * QMF_NSB;
    for (int ts = 0; ts < nslots; ts++) {
        for (int n = NF - 1; n >= NH; n--)
            filt[n] = filt[n - NH];
        for (int n = 0; n < NH; n++) {
            double complex acc = 0.0;
            for (int sb = 0; sb < QMF_NSB; sb++)
                acc += Q[ts][sb] * SY[sb][n];
            filt[n] = creal(acc);
        }
        for (int sb = 0; sb < QMF_NSB; sb++) {
            double acc = 0.0;
            /* All five synthesis prototype sections. */
            for (int k = 0; k < 5; k++)
                acc += filt[256 * k + sb] * QWIN_C[128 * k + sb] +
                       filt[256 * k + 192 + sb] *
                           QWIN_C[128 * k + 64 + sb];
            pcm[(size_t)ts * QMF_NSB + (size_t)sb] = acc;
        }
    }
}
