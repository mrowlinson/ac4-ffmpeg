/* pycompat.h: CPython scalar semantics for the C port.
 *
 * Exact twins of builtin sum()/round()/int()/float() (CPython 3.14):
 * Neumaier summation, ties-to-even rounding, and the
 * underscore/whitespace grammars including full Unicode whitespace
 * and decimal digits (Unicode 16.0 tables; fuzz gate:
 * tests/test_pycompat.py). C11, zero deps.
 *
 * Unicode coverage (verified first-hand against CPython 3.14):
 * - leading/trailing whitespace: exactly the 25 codepoints int()/
 *   float() strip (0009-000D, 0020, 0085, 00A0, 1680, 2000-200A,
 *   2028, 2029, 202F, 205F, 3000). U+001C-001F are NOT stripped
 *   (str.isspace() says True but int()/float() reject them), and
 *   neither are U+200B/U+FEFF.
 * - digits: all 76 Unicode decimal blocks (the unicodedata.decimal
 *   set); scripts may mix; underscores strictly between digits.
 * - the rest of the grammar is ASCII-only: sign, '.', 'e', 'E',
 *   'inf'/'infinity'/'nan', '_' have no Unicode equivalents.
 *
 * Residual limits (precise):
 * - int64 range: pycompat_int saturates (LLONG_MAX/MIN, rc 0);
 *   pycompat_int_ex reports overflow; pycompat_int_big is exact.
 * - CPython's default 4300-digit security cap (ValueError beyond)
 *   is interpreter policy, not grammar: pycompat parses per
 *   grammar at any length (saturating/exact per API above).
 * - float() needs C numeric locale (strtod decimal point).
 * - NUL-terminated forms stop at the first NUL; use the _n forms
 *   for embedded-NUL inputs (NUL -> ValueError, like CPython).
 * - invalid UTF-8 -> ValueError (-1).
 */
#ifndef AC4_PYCOMPAT_H
#define AC4_PYCOMPAT_H

#include <stddef.h>

/* builtin sum() of floats (Neumaier; fuzz-verified). */
double pycompat_sum(const double *v, size_t n);

/* builtin round() of one float, ties to even (fuzz-verified). */
int pycompat_round_half_even(double x);

/*
 * Same, long-long range. NaN/Inf/out-of-int64-range return -1
 * (callers map to Python's ValueError/OverflowError classes).
 */
int pycompat_round_half_even_ll(double x, long long *out);

/*
 * builtin int(s, 10): 0 ok (*out set, saturated at int64),
 * -1 ValueError. Full Unicode whitespace/digits; underscores
 * strictly between digits; any length (no 4300-digit cap).
 */
int pycompat_int(const char *s, long long *out);

/*
 * Same, plus overflow report: 0 ok (*out set, *overflow 1 iff the
 * mathematical value exceeds int64), -1 ValueError.
 */
int pycompat_int_ex(const char *s, long long *out, int *overflow);

/* Explicit-length forms (embedded NUL -> -1). */
int pycompat_int_n(const char *s, size_t n, long long *out);
int pycompat_int_ex_n(const char *s, size_t n, long long *out,
    int *overflow);

/*
 * Exact bignum parse: 0 ok (*out malloc'd canonical decimal:
 * optional '-', no leading zeros, "0" for zero; caller frees),
 * -1 ValueError, -2 allocation failure. Any length.
 */
int pycompat_int_big(const char *s, char **out);
int pycompat_int_big_n(const char *s, size_t n, char **out);

/*
 * builtin float(s): 0 ok (*out set), -1 ValueError, -2 allocation
 * failure. Decimal, inf/infinity/nan (ASCII, any case),
 * underscores strictly between digits; hex rejected like CPython.
 * Full Unicode whitespace/digits; any length (C numeric locale).
 */
int pycompat_float(const char *s, double *out);

/* Explicit-length form (embedded NUL -> -1). */
int pycompat_float_n(const char *s, size_t n, double *out);

#endif
