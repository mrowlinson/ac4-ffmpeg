/* loudness.h: ITU-R BS.1770 loudness metering (K-weighting + gates).
 *
 * C port of encoder/loudness.py (C-port phase, module 3). Bit-parity
 * gate: tests/test_loudness_parity.py. C11, libm only.
 */
#ifndef AC4_LOUDNESS_H
#define AC4_LOUDNESS_H

#include <complex.h>

#define LOUD_ABS_GATE -70.0
#define LOUD_REL_GATE -10.0
#define LOUD_BLOCK_S 0.400
#define LOUD_OVERLAP 0.75
#define LOUD_K_OFFSET -0.691

void loud_k_coeffs(int sr, double b[5], double a[5]);

void loud_k_filter(const double *x, int n, const double b[5],
                   const double a[5], double *y);

/* Mean square per block; returns the block count. */
int loud_block_ms(const double *y, int n, int size, int step,
                  double *out);

/*
 * Recursive radix-2 DIT FFT, same op order as the Python twin
 * (out-of-place; n must be a power of 2).
 */
void loud_fft(const double complex *x, int n, double complex *out);

void loud_flatness(const double *pcmL, int nL, const double *pcmR,
                   int nR, int tile, double *f, double *tf);

double loud_bake_gain(double m, double f, double tf, double target_db);

void loud_integrated(const double *pcmL, int nL, const double *pcmR,
                     int nR, int sr, double *lufs, int *nblocks,
                     int *ngated);

/* chs/lens hold 6 channels in WAV order (LFE excluded, +1.5 dB surr). */
void loud_integrated_mc(double const *const *chs, const int *lens,
                        int sr, double *lufs, int *nblocks, int *ngated);

#endif
