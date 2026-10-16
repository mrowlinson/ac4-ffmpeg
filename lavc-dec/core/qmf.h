/*
 * qmf.h - AC-4 decoder core (in-tree port of dec-lib)
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

#ifndef AC4_QMF_H
#define AC4_QMF_H

#include <complex.h>

/* Complex QMF analysis/synthesis (P1 5.7.1-5.7.4, pseudocodes 65/66).
 * Own C port of tools/qmf_bed.py (same structure, f64). Tables from
 * src/qwin_tables.c (P1 Annex D.3). Modulation matrices precomputed
 * once (constant). Single-threaded use. */

#define QMF_NSB 64
#define QMF_NW 640

typedef struct {
    double afilt[QMF_NW];
    double sfilt[20 * QMF_NSB];
} qmf_state_t;

void qmf_init(qmf_state_t *s); /* zero state (startup rule) */
/* Table 189 slots for frame_length, or -1 (reject). */
int qmf_slots(int frame_length);
/* Time frame -> QMF matrix. pcm[slots*64]; Q[slots][64]. */
void qmf_analysis(const double *pcm, int nslots, qmf_state_t *s,
                  double complex (*Q)[QMF_NSB]);
/* QMF matrix -> time frame. Q[slots][64]; pcm[slots*64]. */
void qmf_synthesis(const double complex (*Q)[QMF_NSB], int nslots,
                   qmf_state_t *s, double *pcm);

#endif
