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
