/* mdct.h: forward MDCT analysis for the clean AC-4 encoder.
 *
 * C port of encoder/mdct.py (C-port phase, module 2). Same E2
 * own-words formulas (Kaiser I0 series, KBD cumsum/sqrt/mirror,
 * DFT-restated MDCT); see tests/test_mdct_parity.py for the
 * bit-parity gate. C11, zero deps beyond libm.
 */
#ifndef AC4_MDCT_H
#define AC4_MDCT_H

#include <complex.h>
#include <stddef.h>

#define MDCT_NCOEFF 2048
#define MDCT_SPAN 4096
#define MDCT_SR 48000

double mdct_bessel_i0(double x, int terms);

/* Full-span KBD window, span taps (span even). */
void mdct_kbd_window(int span, double alpha, double *out);

void mdct_rect_window(int span, double *out);

/* Table-186 row alpha for window length n. */
double mdct_kbd_alpha(int n);

/*
 * 2n-tap KBD window with per-side bridges; t of 0 means a full
 * KBD half on that side.
 */
void mdct_bridged_window(int n, int t_left, int t_right, double *out);

/* In-place radix-2 DIT FFT, n a power of 2. Returns 0 ok, -1 bad n. */
int mdct_fft(int n, double complex *a);

/*
 * N-line MDCT of one tile. pcm has pcm_len samples; window may be
 * NULL for the default Table-186 KBD (2*ncoeff taps); out holds
 * ncoeff lines.
 */
void mdct_forward_n(const double *pcm, int pcm_len, int tile_start,
                    int ncoeff, const double *window, double *out);

void mdct_forward_full(const double *pcm, int pcm_len, int tile_start,
                       const double *window, double *out);

/* Direct O(N) kernel, one line per call (slow reference). */
double mdct_forward_direct_1(const double *pcm, int pcm_len,
                             int tile_start, int ncoeff, int line,
                             const double *window);

/* Legacy rectangular single-bin correlation, one bin per call. */
double mdct_frame_bin(const double *pcm, int pcm_len, int start,
                      double w, int span, const double *window);

double mdct_estimate_freq(const double *pcm, int n, double lo_hz,
                          double hi_hz);

/* Lines covering freq; returns count (<= 2*neighbors+1), fills out. */
int mdct_lines_for_freq(double freq, int neighbors, int *out);

#endif
