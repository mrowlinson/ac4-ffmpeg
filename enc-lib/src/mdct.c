/* mdct.c: forward MDCT analysis. See mdct.h. */
#include "mdct.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


double
mdct_bessel_i0(double x, int terms)
{
    double s = 1.0, t = 1.0;
    for (int k = 1; k <= terms; k++) {
        t *= (x * x) / (4.0 * k * k);
        s += t;
    }
    return s;
}


static double
py_sum(const double *v, size_t n)
{
    /*
     * CPython 3.12+ builtin sum(): Neumaier compensated
     * summation (fuzz-verified 4000/4000 exact vs sum()).
     * PORT-WIDE RULE: every Python sum() of floats becomes
     * this, never a naive loop (last-ulp parity).
     */
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


void
mdct_kbd_window(int span, double alpha, double *out)
{
    assert(span % 2 == 0);
    int m = span / 2;
    double *kern = malloc((size_t)(m + 1) * sizeof *kern);
    assert(kern != NULL);
    for (int i = 0; i <= m; i++) {
        double u = (2.0 * i / m) - 1.0;
        double v = 1.0 - u * u;
        kern[i] = mdct_bessel_i0(M_PI * alpha * sqrt(v > 0.0 ? v : 0.0),
                                 60);
    }
    double denom = py_sum(kern, (size_t)(m + 1));
    double csum = 0.0;
    for (int i = 0; i < m; i++) {
        csum += kern[i];
        out[i] = sqrt(csum / denom);
    }
    for (int i = 0; i < m; i++)
        out[m + i] = out[m - 1 - i];
    free(kern);
}


void
mdct_rect_window(int span, double *out)
{
    for (int i = 0; i < span; i++)
        out[i] = 1.0;
}


double
mdct_kbd_alpha(int n)
{
    switch (n) {
    case 2048:
        return 3.0;
    case 1024:
        return 4.0;
    case 512:
        return 4.5;
    case 256:
        return 5.0;
    case 128:
        return 6.0;
    default:
        assert(!"bad KBD length");
        return 0.0;
    }
}


void
mdct_bridged_window(int n, int t_left, int t_right, double *out)
{
    double *full = malloc((size_t)(2 * n) * sizeof *full);
    assert(full != NULL);
    mdct_kbd_window(2 * n, mdct_kbd_alpha(n), full);
    memcpy(out, full, (size_t)(2 * n) * sizeof *full);
    free(full);
    const int ts[2] = { t_left, t_right };
    for (int side = 0; side < 2; side++) {
        int t = ts[side];
        if (t == 0 || t >= n)
            continue;
        assert((n - t) % 2 == 0);
        double *taper = malloc((size_t)(2 * t) * sizeof *taper);
        assert(taper != NULL);
        mdct_kbd_window(2 * t, mdct_kbd_alpha(t), taper);
        int ns = (n - t) / 2;
        if (side == 0) {
            for (int i = 0; i < ns; i++)
                out[i] = 0.0;
            memcpy(out + ns, taper, (size_t)t * sizeof *taper);
            for (int i = ns + t; i < n; i++)
                out[i] = 1.0;
        } else {
            for (int i = 0; i < ns; i++)
                out[n + i] = 1.0;
            memcpy(out + n + ns, taper + t,
                   (size_t)t * sizeof *taper);
            for (int i = ns + t; i < n; i++)
                out[n + i] = 0.0;
        }
        free(taper);
    }
}


int
mdct_fft(int n, double complex *a)
{
    if (n <= 0 || (n & (n - 1)) != 0)
        return -1;
    int j = 0;
    for (int i = 1; i < n; i++) {
        int bit = n >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j |= bit;
        if (i < j) {
            double complex tmp = a[i];
            a[i] = a[j];
            a[j] = tmp;
        }
    }
    for (int size = 2; size <= n; size <<= 1) {
        double ang = -2.0 * M_PI / size;
        double complex wspan = cos(ang) + I * sin(ang);
        int half = size / 2;
        for (int start = 0; start < n; start += size) {
            double complex w = 1.0 + 0.0 * I;
            for (int k = 0; k < half; k++) {
                double complex u = a[start + k];
                double complex v = a[start + k + half] * w;
                a[start + k] = u + v;
                a[start + k + half] = u - v;
                w *= wspan;
            }
        }
    }
    return 0;
}


static void
default_window(int ncoeff, double *tmp)
{
    mdct_kbd_window(2 * ncoeff, mdct_kbd_alpha(ncoeff), tmp);
}


void
mdct_forward_n(const double *pcm, int pcm_len, int tile_start,
               int ncoeff, const double *window, double *out)
{
    int m = 2 * ncoeff;
    double *wbuf = NULL;
    if (window == NULL) {
        wbuf = malloc((size_t)m * sizeof *wbuf);
        assert(wbuf != NULL);
        default_window(ncoeff, wbuf);
        window = wbuf;
    }
    double complex *yc = malloc((size_t)m * sizeof *yc);
    assert(yc != NULL);
    for (int n = 0; n < m; n++) {
        int j = tile_start - ncoeff / 2 + n;
        double x = (j >= 0 && j < pcm_len) ? pcm[j] : 0.0;
        double ph = M_PI * n / m;
        double complex rot = cos(ph) - I * sin(ph);
        yc[n] = x * window[n] * rot;
    }
    mdct_fft(m, yc);
    for (int k = 0; k < ncoeff; k++) {
        double ph = M_PI * (ncoeff + 1) * (k + 0.5) / m;
        double complex tw = cos(ph) + I * sin(ph);
        out[k] = creal(tw * conj(yc[k]));
    }
    free(yc);
    free(wbuf);
}


void
mdct_forward_full(const double *pcm, int pcm_len, int tile_start,
                  const double *window, double *out)
{
    mdct_forward_n(pcm, pcm_len, tile_start, MDCT_NCOEFF, window,
                   out);
}


double
mdct_forward_direct_1(const double *pcm, int pcm_len, int tile_start,
                      int ncoeff, int line, const double *window)
{
    double *wbuf = NULL;
    if (window == NULL) {
        wbuf = malloc((size_t)(2 * ncoeff) * sizeof *wbuf);
        assert(wbuf != NULL);
        default_window(ncoeff, wbuf);
        window = wbuf;
    }
    double s = 0.0;
    double ph = M_PI / ncoeff * (line + 0.5);
    for (int i = 0; i < 2 * ncoeff; i++) {
        int j = tile_start - ncoeff / 2 + i;
        double x = (j >= 0 && j < pcm_len) ? pcm[j] : 0.0;
        s += x * window[i] * cos(ph * (i + 0.5 + ncoeff / 2.0));
    }
    free(wbuf);
    return s;
}


double
mdct_frame_bin(const double *pcm, int pcm_len, int start, double w,
               int span, const double *window)
{
    double s = 0.0;
    for (int n = 0; n < span; n++) {
        int j = start + n;
        double x = (j >= 0 && j < pcm_len) ? pcm[j] : 0.0;
        s += x * window[n] * cos(w * n);
    }
    return s;
}


double
mdct_estimate_freq(const double *pcm, int n, double lo_hz,
                   double hi_hz)
{
    int first = -1, last = -1, count = 0;
    for (int i = 1; i < n; i++) {
        int a = pcm[i - 1] < 0;
        int b = pcm[i] < 0;
        if (a != b) {
            if (first < 0)
                first = i;
            last = i;
            count++;
        }
    }
    if (count < 3)
        return 0.0;
    double periods = (last - first) / ((count - 1) / 2.0);
    double f = periods > 0 ? MDCT_SR / periods : 0.0;
    if (!(lo_hz <= f && f <= hi_hz))
        return 0.0;
    return f;
}


static int
py_round_half_even(double x)
{
    /* Python round(): ties to even, on the magnitude. */
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


int
mdct_lines_for_freq(double freq, int neighbors, int *out)
{
    if (freq <= 0)
        return 0;
    int c = py_round_half_even(freq * MDCT_SPAN / MDCT_SR - 0.5);
    int n = 0;
    for (int L = c - neighbors; L <= c + neighbors; L++) {
        if (L >= 0 && L < MDCT_NCOEFF)
            out[n++] = L;
    }
    return n;
}
