/* pycompat.c: CPython scalar semantics. See pycompat.h. */
#include "pycompat.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>


double
pycompat_sum(const double *v, size_t n)
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


int
pycompat_round_half_even(double x)
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


int
pycompat_round_half_even_ll(double x, long long *out)
{
    if (out == NULL)
        return -1;
    if (isnan(x) || isinf(x))
        return -1;
    double a = fabs(x);
    if (a >= 18446744073709551616.0)
        return -1;
    double f = floor(a);
    double d = a - f;
    double r;
    if (d < 0.5)
        r = f;
    else if (d > 0.5)
        r = f + 1.0;
    else
        r = (fmod(f, 2.0) == 0.0) ? f : f + 1.0;
    /* r < 2^63 exactly representable; negate in double. */
    double v = x < 0 ? -r : r;
    if (v < -9223372036854775808.0 || v >= 9223372036854775808.0)
        return -1;
    *out = (long long)v;
    return 0;
}


/*
 * Strict UTF-8 decode of one codepoint at p (p < end). Returns the
 * length 1..4 (*cp set), or 0 for invalid bytes (bad continuation,
 * overlong, surrogate, > U+10FFFF, truncated).
 */
static size_t
utf8_dec(const unsigned char *p, const unsigned char *end, unsigned *cp)
{
    unsigned c = p[0];
    size_t need;
    unsigned min, v;
    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    if ((c & 0xE0) == 0xC0) {
        need = 2;
        min = 0x80;
        v = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        need = 3;
        min = 0x800;
        v = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        need = 4;
        min = 0x10000;
        v = c & 0x07;
    } else {
        return 0;
    }
    if ((size_t)(end - p) < need)
        return 0;
    for (size_t i = 1; i < need; i++) {
        if ((p[i] & 0xC0) != 0x80)
            return 0;
        v = (v << 6) | (p[i] & 0x3F);
    }
    if (v < min || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF))
        return 0;
    *cp = v;
    return need;
}


/*
 * The 25 codepoints CPython int()/float() strip (Unicode 16.0;
 * probed first-hand: excludes U+001C-001F despite isspace()).
 */
static int
is_py_space(unsigned cp)
{
    static const unsigned ws[] = {
        0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x0020, 0x0085, 0x00A0,
        0x1680, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
        0x2007, 0x2008, 0x2009, 0x200A, 0x2028, 0x2029, 0x202F, 0x205F,
        0x3000,
    };
    for (size_t i = 0; i < sizeof ws / sizeof ws[0]; i++)
        if (ws[i] == cp)
            return 1;
    return 0;
}


/*
 * Unicode decimal value of cp, or -1. The 76 blocks probed
 * first-hand (== unicodedata.decimal set); each is 0..9 contiguous.
 */
static int
py_digit(unsigned cp)
{
    static const unsigned starts[] = {
        0x0030, 0x0660, 0x06F0, 0x07C0, 0x0966, 0x09E6, 0x0A66, 0x0AE6,
        0x0B66, 0x0BE6, 0x0C66, 0x0CE6, 0x0D66, 0x0DE6, 0x0E50, 0x0ED0,
        0x0F20, 0x1040, 0x1090, 0x17E0, 0x1810, 0x1946, 0x19D0, 0x1A80,
        0x1A90, 0x1B50, 0x1BB0, 0x1C40, 0x1C50, 0xA620, 0xA8D0, 0xA900,
        0xA9D0, 0xA9F0, 0xAA50, 0xABF0, 0xFF10, 0x104A0, 0x10D30, 0x10D40,
        0x11066, 0x110F0, 0x11136, 0x111D0, 0x112F0, 0x11450, 0x114D0, 0x11650,
        0x116C0, 0x116D0, 0x116DA, 0x11730, 0x118E0, 0x11950, 0x11BF0, 0x11C50,
        0x11D50, 0x11DA0, 0x11F50, 0x16130, 0x16A60, 0x16AC0, 0x16B50, 0x16D70,
        0x1CCF0, 0x1D7CE, 0x1D7D8, 0x1D7E2, 0x1D7EC, 0x1D7F6, 0x1E140, 0x1E2F0,
        0x1E4F0, 0x1E5F1, 0x1E950, 0x1FBF0,
    };
    size_t lo = 0, hi = sizeof starts / sizeof starts[0];
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (starts[mid] <= cp)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == 0)
        return -1;
    {
        unsigned d = cp - starts[lo - 1];
        if (d < 10)
            return (int)d;
    }
    return -1;
}


/* Skip leading/trailing CPython whitespace; -1 on invalid UTF-8. */
static int
skip_ws(const unsigned char **pp, const unsigned char *end)
{
    const unsigned char *p = *pp;
    while (p < end) {
        unsigned cp;
        size_t L = utf8_dec(p, end, &cp);
        if (L == 0)
            return -1;
        if (!is_py_space(cp))
            break;
        p += L;
    }
    *pp = p;
    return 0;
}


static int
int_core(const unsigned char *s, size_t n, long long *out, int *overflow)
{
    const unsigned char *end = s + n;
    const unsigned char *p = s;
    int neg = 0;
    int has = 0, last_us = 1, sat = 0;
    unsigned long long acc = 0;
    if (skip_ws(&p, end))
        return -1;
    if (p < end && (*p == '+' || *p == '-')) {
        neg = *p == '-';
        p++;
    }
    /* digits (any script, may mix) with underscores strictly
     * between digits. */
    while (p < end) {
        unsigned cp;
        size_t L = utf8_dec(p, end, &cp);
        int d;
        if (L == 0)
            return -1;
        if (cp == (unsigned)'_') {
            if (last_us)
                return -1;
            last_us = 1;
            p += L;
            continue;
        }
        d = py_digit(cp);
        if (d < 0)
            break;
        last_us = 0;
        has = 1;
        if (acc > (unsigned long long)LLONG_MAX / 10 ||
            (acc == (unsigned long long)LLONG_MAX / 10 &&
             (unsigned)d >
                 (unsigned)(LLONG_MAX % 10) + (unsigned)neg)) {
            sat = 1;
        } else if (!sat) {
            acc = acc * 10 + (unsigned)d;
        }
        p += L;
    }
    if (!has || last_us)
        return -1;
    if (skip_ws(&p, end))
        return -1;
    if (p != end)
        return -1;
    if (overflow != NULL)
        *overflow = sat;
    if (sat) {
        *out = neg ? LLONG_MIN : LLONG_MAX;
        return 0;
    }
    if (neg) {
        if (acc == (unsigned long long)LLONG_MAX + 1)
            *out = LLONG_MIN;
        else
            *out = -(long long)acc;
    } else {
        *out = (long long)acc;
    }
    return 0;
}


static int
big_core(const unsigned char *s, size_t n, char **out)
{
    const unsigned char *end = s + n;
    const unsigned char *p = s;
    char *digs;
    size_t nd = 0, i = 0;
    char *r;
    int neg = 0;
    int has = 0, last_us = 1;
    *out = NULL;
    if (skip_ws(&p, end))
        return -1;
    if (p < end && (*p == '+' || *p == '-')) {
        neg = *p == '-';
        p++;
    }
    digs = malloc(n + 1);
    if (digs == NULL)
        return -2;
    while (p < end) {
        unsigned cp;
        size_t L = utf8_dec(p, end, &cp);
        int d;
        if (L == 0) {
            free(digs);
            return -1;
        }
        if (cp == (unsigned)'_') {
            if (last_us) {
                free(digs);
                return -1;
            }
            last_us = 1;
            p += L;
            continue;
        }
        d = py_digit(cp);
        if (d < 0)
            break;
        last_us = 0;
        has = 1;
        digs[nd++] = (char)('0' + d);
        p += L;
    }
    if (!has || last_us) {
        free(digs);
        return -1;
    }
    if (skip_ws(&p, end)) {
        free(digs);
        return -1;
    }
    if (p != end) {
        free(digs);
        return -1;
    }
    while (i < nd && digs[i] == '0')
        i++;
    if (i == nd) {
        /* zero (incl. "-0", like str(int())). */
        free(digs);
        r = malloc(2);
        if (r == NULL)
            return -2;
        r[0] = '0';
        r[1] = '\0';
        *out = r;
        return 0;
    }
    r = malloc((nd - i) + (size_t)(neg ? 1 : 0) + 1);
    if (r == NULL) {
        free(digs);
        return -2;
    }
    {
        size_t o = 0;
        if (neg)
            r[o++] = '-';
        memcpy(r + o, digs + i, nd - i);
        o += nd - i;
        r[o] = '\0';
    }
    free(digs);
    *out = r;
    return 0;
}


/* ASCII-only case-insensitive byte match (no locale fold). */
static int
ascii_ci(const unsigned char *p, const char *w, size_t k)
{
    for (size_t i = 0; i < k; i++) {
        unsigned c = p[i];
        if (c >= (unsigned)'A' && c <= (unsigned)'Z')
            c += (unsigned)('a' - 'A');
        if (c != (unsigned)w[i])
            return 0;
    }
    return 1;
}


static int
float_core(const unsigned char *s, size_t n, double *out)
{
    const unsigned char *end = s + n;
    const unsigned char *p = s;
    char *clean;
    size_t nc = 0;
    int intdig = 0, fracdig = 0, expdig = 0;
    int seen_dot = 0, seen_exp = 0, prev_is_dig = 0;
    int neg = 0;
    int is_inf = 0, is_nan = 0;
    char *tmp;
    size_t nt = 0;
    char *e2 = NULL;
    double v;
    if (skip_ws(&p, end))
        return -1;
    if (p < end && (*p == '+' || *p == '-')) {
        neg = *p == '-';
        p++;
    }
    /* inf/infinity/nan (ASCII, any case), then only trailing ws. */
    {
        size_t rem = (size_t)(end - p);
        if (rem >= 8 && ascii_ci(p, "infinity", 8)) {
            is_inf = 1;
            p += 8;
        } else if (rem >= 3 && ascii_ci(p, "inf", 3)) {
            is_inf = 1;
            p += 3;
        } else if (rem >= 3 && ascii_ci(p, "nan", 3)) {
            is_nan = 1;
            p += 3;
        }
    }
    if (is_inf || is_nan) {
        char word[8];
        if (skip_ws(&p, end))
            return -1;
        if (p != end)
            return -1;
        word[0] = neg ? '-' : '+';
        if (is_inf)
            memcpy(word + 1, "inf", 4);
        else
            memcpy(word + 1, "nan", 4);
        *out = strtod(word, NULL);
        return 0;
    }
    /*
     * Decimal: int [. frac] [exp], underscores strictly between
     * digits (peeked next side; prev side via flag). Every output
     * byte consumes >= 1 input byte, so n + 1 always fits.
     */
    clean = malloc(n + 1);
    if (clean == NULL)
        return -2;
    while (p < end) {
        unsigned cp, nx;
        size_t L = utf8_dec(p, end, &cp);
        int d;
        if (L == 0) {
            free(clean);
            return -1;
        }
        if (cp == (unsigned)'_') {
            size_t L2;
            if (!prev_is_dig) {
                free(clean);
                return -1;
            }
            if (p + L >= end) {
                free(clean);
                return -1;
            }
            L2 = utf8_dec(p + L, end, &nx);
            if (L2 == 0 || py_digit(nx) < 0) {
                free(clean);
                return -1;
            }
            p += L;
            continue;
        }
        d = py_digit(cp);
        if (d >= 0) {
            clean[nc++] = (char)('0' + d);
            if (seen_exp)
                expdig = 1;
            else if (seen_dot)
                fracdig = 1;
            else
                intdig = 1;
            prev_is_dig = 1;
            p += L;
            continue;
        }
        if (cp == (unsigned)'.' && !seen_dot && !seen_exp) {
            seen_dot = 1;
            clean[nc++] = '.';
            prev_is_dig = 0;
            p += L;
            continue;
        }
        if ((cp == (unsigned)'e' || cp == (unsigned)'E') && !seen_exp &&
            (intdig || fracdig)) {
            seen_exp = 1;
            clean[nc++] = 'e';
            p += L;
            if (p < end && (*p == '+' || *p == '-')) {
                clean[nc++] = (char)*p;
                p++;
            }
            prev_is_dig = 0;
            continue;
        }
        break;
    }
    if (!intdig && !fracdig) {
        free(clean);
        return -1;
    }
    if (seen_exp && !expdig) {
        free(clean);
        return -1;
    }
    if (skip_ws(&p, end)) {
        free(clean);
        return -1;
    }
    if (p != end) {
        free(clean);
        return -1;
    }
    tmp = malloc(nc + 2);
    if (tmp == NULL) {
        free(clean);
        return -2;
    }
    tmp[nt++] = neg ? '-' : '+';
    memcpy(tmp + nt, clean, nc);
    nt += nc;
    tmp[nt] = '\0';
    free(clean);
    errno = 0;
    v = strtod(tmp, &e2);
    if (e2 == NULL || *e2 != '\0') {
        free(tmp);
        return -1;
    }
    free(tmp);
    *out = v;
    return 0;
}


int
pycompat_int(const char *s, long long *out)
{
    if (s == NULL || out == NULL)
        return -1;
    return int_core((const unsigned char *)s, strlen(s), out, NULL);
}


int
pycompat_int_ex(const char *s, long long *out, int *overflow)
{
    if (s == NULL || out == NULL || overflow == NULL)
        return -1;
    return int_core((const unsigned char *)s, strlen(s), out,
        overflow);
}


int
pycompat_int_n(const char *s, size_t n, long long *out)
{
    if (s == NULL || out == NULL)
        return -1;
    return int_core((const unsigned char *)s, n, out, NULL);
}


int
pycompat_int_ex_n(const char *s, size_t n, long long *out,
    int *overflow)
{
    if (s == NULL || out == NULL || overflow == NULL)
        return -1;
    return int_core((const unsigned char *)s, n, out, overflow);
}


int
pycompat_int_big(const char *s, char **out)
{
    if (s == NULL || out == NULL)
        return -1;
    return big_core((const unsigned char *)s, strlen(s), out);
}


int
pycompat_int_big_n(const char *s, size_t n, char **out)
{
    if (s == NULL || out == NULL)
        return -1;
    return big_core((const unsigned char *)s, n, out);
}


int
pycompat_float(const char *s, double *out)
{
    if (s == NULL || out == NULL)
        return -1;
    return float_core((const unsigned char *)s, strlen(s), out);
}


int
pycompat_float_n(const char *s, size_t n, double *out)
{
    if (s == NULL || out == NULL)
        return -1;
    return float_core((const unsigned char *)s, n, out);
}
