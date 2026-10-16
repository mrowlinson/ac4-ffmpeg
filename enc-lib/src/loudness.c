/* loudness.c: BS.1770 metering. See loudness.h.
 *
 * Port rules inherited from module 2: builtin sum() of floats is
 * Neumaier (py_sum), round() is ties-to-even (py_round_half_even).
 * Module 3 finding: cmath.exp on pure-imaginary z is libm sincos
 * (port_sincos), NOT separate cos/sin (1 ulp / ~500 apart).
 */
#include "loudness.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


static double
py_sum(const double *v, size_t n)
{
    double s = 0.0, c = 0.0;
    for (size_t i = 0; i < n; i++) {
        double x = v[i];
        double t = s + x;
        if (fabs(s) >= fabs(x))
            c += (s - t) + x;
        else
            c += (x - t) + s;
        s = t;
    }
    return s + c;
}


static void
port_sincos(double x, double *s, double *c)
{
#if defined(__APPLE__)
    extern void __sincos(double, double *, double *);
    __sincos(x, s, c);
#else
    sincos(x, s, c);
#endif
}


static int
py_round_half_even(double x)
{
    double a = fabs(x);
    double f = floor(a);
    double d = a - f;
    double r;
    if (d < 0.5)
        r = f;
    else if (d > 0.5)
        r = f + 1.0;
    else
        r = (fmod(f, 2.0) == 0.0) ? f : f + 1.0;
    return (int)(x < 0 ? -r : r);
}


void
loud_k_coeffs(int sr, double b[5], double a[5])
{
    double f0 = 1681.974450955533;
    double g = 3.999843853973347;
    double q = 0.7071752369554196;
    double k = tan(M_PI * f0 / sr);
    double vh = pow(10.0, g / 20.0);
    double vb = pow(vh, 0.4996667741545416);
    double a0 = 1.0 + k / q + k * k;
    double pb0 = (vh + vb * k / q + k * k) / a0;
    double pb1 = 2.0 * (k * k - vh) / a0;
    double pb2 = (vh - vb * k / q + k * k) / a0;
    double pa1 = 2.0 * (k * k - 1.0) / a0;
    double pa2 = (1.0 - k / q + k * k) / a0;
    f0 = 38.13547087602444;
    q = 0.5003270373238773;
    k = tan(M_PI * f0 / sr);
    double rb0 = 1.0, rb1 = -2.0, rb2 = 1.0;
    double d = 1.0 + k / q + k * k;
    double ra1 = 2.0 * (k * k - 1.0) / d;
    double ra2 = (1.0 - k / q + k * k) / d;
    b[0] = pb0 * rb0;
    b[1] = pb0 * rb1 + pb1 * rb0;
    b[2] = pb0 * rb2 + pb1 * rb1 + pb2 * rb0;
    b[3] = pb1 * rb2 + pb2 * rb1;
    b[4] = pb2 * rb2;
    a[0] = 1.0;
    a[1] = ra1 + pa1;
    a[2] = ra2 + pa1 * ra1 + pa2;
    a[3] = pa1 * ra2 + pa2 * ra1;
    a[4] = pa2 * ra2;
}


void
loud_k_filter(const double *x, int n, const double b[5],
              const double a[5], double *y)
{
    double b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3], b4 = b[4];
    double a1 = a[1], a2 = a[2], a3 = a[3], a4 = a[4];
    for (int i = 0; i < n; i++) {
        double v = b0 * x[i];
        if (i >= 1) {
            v += b1 * x[i - 1] - a1 * y[i - 1];
            if (i >= 2) {
                v += b2 * x[i - 2] - a2 * y[i - 2];
                if (i >= 3) {
                    v += b3 * x[i - 3] - a3 * y[i - 3];
                    if (i >= 4)
                        v += b4 * x[i - 4] - a4 * y[i - 4];
                }
            }
        }
        y[i] = v;
    }
}


static double
py_sum_sq(const double *v, size_t n)
{
    /* sum(v*v ...) under Neumaier, products formed first. */
    double s = 0.0, c = 0.0;
    for (size_t i = 0; i < n; i++) {
        double x = v[i] * v[i];
        double t = s + x;
        if (fabs(s) >= fabs(x))
            c += (s - t) + x;
        else
            c += (x - t) + s;
        s = t;
    }
    return s + c;
}


int
loud_block_ms(const double *y, int n, int size, int step, double *out)
{
    if (n < size) {
        if (n > 0)
            out[0] = py_sum_sq(y, n) / n;
        else
            out[0] = 0.0;
        return 1;
    }
    int nb = 0;
    for (int lo = 0; lo + size <= n; lo += step)
        out[nb++] = py_sum_sq(y + lo, (size_t)size) / size;
    if (nb == 0)
        out[nb++] = 0.0;
    return nb;
}


void
loud_fft(const double complex *x, int n, double complex *out)
{
    assert(n % 2 == 0 || n == 1);
    if (n == 1) {
        out[0] = x[0];
        return;
    }
    int h = n / 2;
    double complex *even = malloc((size_t)h * sizeof *even);
    double complex *odd = malloc((size_t)h * sizeof *odd);
    double complex *fe = malloc((size_t)h * sizeof *fe);
    double complex *fo = malloc((size_t)h * sizeof *fo);
    assert(even && odd && fe && fo);
    for (int i = 0; i < h; i++) {
        even[i] = x[2 * i];
        odd[i] = x[2 * i + 1];
    }
    loud_fft(even, h, fe);
    loud_fft(odd, h, fo);
    for (int k = 0; k < h; k++) {
        /*
         * CPython's cmath.exp on pure-imaginary z IS libm
         * sincos (fuzz-verified 200k/200k exact; separate
         * cos/sin differ ~1ulp/500). Match it exactly.
         */
        double y = -2.0 * M_PI * k / n;
        double s, c;
        port_sincos(y, &s, &c);
        double complex tw = c + I * s;
        double complex t = tw * fo[k];
        out[k] = fe[k] + t;
        out[k + h] = fe[k] - t;
    }
    free(even);
    free(odd);
    free(fe);
    free(fo);
}


void
loud_flatness(const double *pcmL, int nL, const double *pcmR,
              int nR, int tile, double *f, double *tf)
{
    int n = nL > nR ? nL : nR;
    if (n <= 0) {
        *f = 0.0;
        *tf = 0.0;
        return;
    }
    int ntiles = (n + tile - 1) / tile;
    double *energies =
        malloc((size_t)ntiles * sizeof *energies);
    double *mono = malloc((size_t)n * sizeof *mono);
    assert(energies && mono);
    for (int i = 0; i < n; i++) {
        double a = i < nL ? pcmL[i] : 0.0;
        double b = i < nR ? pcmR[i] : 0.0;
        mono[i] = (a + b) / 2.0;
    }
    for (int t = 0; t < ntiles; t++) {
        int lo = t * tile, hi = lo + tile < n ? lo + tile : n;
        energies[t] = py_sum_sq(mono + lo, (size_t)(hi - lo));
    }
    double peak = energies[0];
    for (int t = 1; t < ntiles; t++)
        if (energies[t] > peak)
            peak = energies[t];
    if (!(peak > 0)) {
        *f = 0.0;
        *tf = 0.0;
        free(energies);
        free(mono);
        return;
    }
    int nloud = 0;
    for (int t = 0; t < ntiles; t++)
        if (energies[t] >= peak / 4.0)
            nloud++;
    *tf = (double)nloud / ntiles;
    double complex *spec =
        malloc((size_t)tile * sizeof *spec);
    double complex *spec_o =
        malloc((size_t)tile * sizeof *spec_o);
    double *pw = malloc((size_t)(tile / 2) * sizeof *pw);
    assert(spec && spec_o && pw);
    double fsum = 0.0, fcomp = 0.0;
    int nflats = 0;
    for (int t = 0; t < ntiles; t++) {
        if (energies[t] < peak / 4.0)
            continue;
        int lo = t * tile;
        if (lo + tile > n)
            continue;
        for (int i = 0; i < tile; i++)
            spec[i] = mono[lo + i] + 0.0 * I;
        loud_fft(spec, tile, spec_o);
        int m = tile / 2 - 1;
        if (m <= 0)
            continue;
        for (int i = 0; i < m; i++) {
            double re = creal(spec_o[1 + i]);
            double im = cimag(spec_o[1 + i]);
            pw[i] = (re * re + im * im) / tile;
        }
        double pwmax = pw[0];
        for (int i = 1; i < m; i++)
            if (pw[i] > pwmax)
                pwmax = pw[i];
        if (!(pwmax > 0))
            continue;
        double am = py_sum(pw, (size_t)m) / m;
        double ls = 0.0, lc = 0.0;
        for (int i = 0; i < m; i++) {
            double x = log(pw[i] + 1e-30);
            double tt = ls + x;
            if (fabs(ls) >= fabs(x))
                lc += (ls - tt) + x;
            else
                lc += (x - tt) + ls;
            ls = tt;
        }
        double gm = exp((ls + lc) / m);
        double fl = am > 0 ? gm / am : 0.0;
        double tt = fsum + fl;
        if (fabs(fsum) >= fabs(fl))
            fcomp += (fsum - tt) + fl;
        else
            fcomp += (fl - tt) + fsum;
        fsum = tt;
        nflats++;
    }
    *f = nflats ? (fsum + fcomp) / nflats : 0.0;
    free(energies);
    free(mono);
    free(spec);
    free(spec_o);
    free(pw);
}


double
loud_bake_gain(double m, double f, double tf, double target_db)
{
    int hits = tf < 0.6 && f > 0.5;
    double m_eff = hits && m < -23.0 ? -23.0 : m;
    double t = hits ? -21.11 : 27.24 * f - 30.03;
    return t - m_eff + (target_db + 24.0);
}


static double
chan_sum(double **blocks, const double *gains, int nch, int j)
{
    /* sum(g*z[j] ...) under Neumaier, channel order. */
    double s = 0.0, c = 0.0;
    for (int i = 0; i < nch; i++) {
        double x = gains[i] * blocks[i][j];
        double t = s + x;
        if (fabs(s) >= fabs(x))
            c += (s - t) + x;
        else
            c += (x - t) + s;
        s = t;
    }
    return s + c;
}


static void
integrated_ms(double **blocks, const int *counts, const double *gains,
              int nch, double *lufs, int *nblocks, int *ngated)
{
    int nb = counts[0];
    for (int i = 1; i < nch; i++)
        if (counts[i] < nb)
            nb = counts[i];
    double *lj = malloc((size_t)(nb > 0 ? nb : 1) * sizeof *lj);
    int *gated = malloc((size_t)(nb > 0 ? nb : 1) * sizeof *gated);
    assert(lj && gated);
    for (int j = 0; j < nb; j++) {
        double e = chan_sum(blocks, gains, nch, j);
        lj[j] = e > 0 ? LOUD_K_OFFSET + 10.0 * log10(e) : -INFINITY;
    }
    int ng = 0;
    for (int j = 0; j < nb; j++)
        if (lj[j] >= LOUD_ABS_GATE)
            gated[ng++] = j;
    if (ng == 0) {
        *lufs = -INFINITY;
        *nblocks = nb;
        *ngated = 0;
        free(lj);
        free(gated);
        return;
    }
    double s = 0.0, c = 0.0;
    for (int i = 0; i < ng; i++) {
        double x = chan_sum(blocks, gains, nch, gated[i]);
        double t = s + x;
        if (fabs(s) >= fabs(x))
            c += (s - t) + x;
        else
            c += (x - t) + s;
        s = t;
    }
    double avg = (s + c) / ng;
    double rel = LOUD_K_OFFSET + 10.0 * log10(avg) + LOUD_REL_GATE;
    int ng2 = 0;
    for (int i = 0; i < ng; i++)
        if (lj[gated[i]] >= rel)
            gated[ng2++] = gated[i];
    if (ng2 == 0) {
        *lufs = -INFINITY;
        *nblocks = nb;
        *ngated = 0;
        free(lj);
        free(gated);
        return;
    }
    s = 0.0;
    c = 0.0;
    for (int i = 0; i < ng2; i++) {
        double x = chan_sum(blocks, gains, nch, gated[i]);
        double t = s + x;
        if (fabs(s) >= fabs(x))
            c += (s - t) + x;
        else
            c += (x - t) + s;
        s = t;
    }
    *lufs = LOUD_K_OFFSET + 10.0 * log10((s + c) / ng2);
    *nblocks = nb;
    *ngated = ng2;
    free(lj);
    free(gated);
}


static void
integrated_nch(double const *const *chs, const int *lens, int nch,
               const double *gains, int sr, double *lufs,
               int *nblocks, int *ngated)
{
    int n = 0;
    for (int i = 0; i < nch; i++)
        if (lens[i] > n)
            n = lens[i];
    if (n <= 0) {
        *lufs = -INFINITY;
        *nblocks = 0;
        *ngated = 0;
        return;
    }
    double b[5], a[5];
    loud_k_coeffs(sr, b, a);
    int size = py_round_half_even(LOUD_BLOCK_S * sr);
    int step =
        py_round_half_even(LOUD_BLOCK_S * (1.0 - LOUD_OVERLAP) * sr);
    double **blocks = malloc((size_t)nch * sizeof *blocks);
    int *counts = malloc((size_t)nch * sizeof *counts);
    double *cc = malloc((size_t)n * sizeof *cc);
    double *xn = malloc((size_t)n * sizeof *xn);
    double *yn = malloc((size_t)n * sizeof *yn);
    assert(blocks && counts && cc && xn && yn);
    int cap = n / (step > 0 ? step : 1) + 2;
    for (int i = 0; i < nch; i++) {
        blocks[i] = malloc((size_t)cap * sizeof *blocks[i]);
        assert(blocks[i]);
        for (int j = 0; j < n; j++)
            cc[j] = j < lens[i] ? chs[i][j] : 0.0;
        for (int j = 0; j < n; j++)
            xn[j] = cc[j] / 32768.0;
        loud_k_filter(xn, n, b, a, yn);
        counts[i] = loud_block_ms(yn, n, size, step, blocks[i]);
    }
    integrated_ms(blocks, counts, gains, nch, lufs, nblocks,
                  ngated);
    for (int i = 0; i < nch; i++)
        free(blocks[i]);
    free(blocks);
    free(counts);
    free(cc);
    free(xn);
    free(yn);
}


void
loud_integrated(const double *pcmL, int nL, const double *pcmR,
                int nR, int sr, double *lufs, int *nblocks,
                int *ngated)
{
    const double *chs[2] = { pcmL, pcmR };
    const int lens[2] = { nL, nR };
    const double gains[2] = { 1.0, 1.0 };
    integrated_nch(chs, lens, 2, gains, sr, lufs, nblocks, ngated);
}


void
loud_integrated_mc(double const *const *chs, const int *lens,
                   int sr, double *lufs, int *nblocks, int *ngated)
{
    if (lens == NULL)
        return;
    static const double gains[6] = { 1.0, 1.0, 1.0, 0.0, 1.4125,
                                     1.4125 };
    integrated_nch(chs, lens, 6, gains, sr, lufs, nblocks, ngated);
}
