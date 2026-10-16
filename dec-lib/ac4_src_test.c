/* ac4_src_test.c - s5.11 output-count unit tests (N5 fix).
 *
 * Pins the Table 70 patterns exactly (29.97/59.94/119.88 rows), the
 * integer-row constants, sf scaling, the one-period sum rule, and the
 * sequence_counter phase rule (mid-stream seq 0 advances, never
 * resets). No fixtures; pure header logic.
 */
#include <stdio.h>
#include <string.h>

#include "src/ac4_src.h"

static int fails = 0;

#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static void check_row(int fri, int fs, int sf, const long *want, int nw) {
    long num, den, per;
    long sum = 0;
    int i;
    ac4_src_params(fri, fs, sf, &num, &den, &per);
    CHECK(per == nw, "fri=%d fs=%d sf=%d: per=%ld want %d", fri, fs, sf,
          per, nw);
    for (i = 0; i < nw; i++) {
        long got = ac4_src_nout(num, den, i);
        sum += got;
        CHECK(got == want[i], "fri=%d fs=%d sf=%d eff=%d: got %ld want %ld",
              fri, fs, sf, i, got, want[i]);
    }
    /* One full period sums to exactly L*P (no lost/found samples). */
    CHECK(sum * den == num * per, "fri=%d fs=%d sf=%d: sum=%ld != L*P",
          fri, fs, sf, sum);
}

static void check_const(int fri, int fs, int sf, long want) {
    long num, den, per;
    ac4_src_params(fri, fs, sf, &num, &den, &per);
    CHECK(per == 1, "fri=%d fs=%d sf=%d: per=%ld want 1", fri, fs, sf,
          per);
    CHECK(ac4_src_nout(num, den, 0) == want,
          "fri=%d fs=%d sf=%d: got %ld want %ld", fri, fs, sf,
          ac4_src_nout(num, den, 0), want);
    CHECK(ac4_src_ceil(num, den) == want, "fri=%d fs=%d sf=%d ceil", fri,
          fs, sf);
}

int main(void) {
    /* Table 70 rows (spec text, comma decimals -> integers). */
    static const long r2997[] = {1601, 1602, 1601, 1602, 1602};
    static const long r5994[] = {800, 801, 801, 801, 801};
    static const long r11988[] = {400, 400, 401, 400, 401};
    long num, den, per;

    check_row(3, 1, 1, r2997, 5);
    check_row(8, 1, 1, r5994, 5);
    check_row(11, 1, 1, r11988, 5);

    /* Integer rows: constant counts (Table E.1 sample_delta). */
    check_const(0, 1, 1, 2002);
    check_const(1, 1, 1, 2000);
    check_const(2, 1, 1, 1920);
    check_const(4, 1, 1, 1600);
    check_const(5, 1, 1, 1001);
    check_const(6, 1, 1, 1000);
    check_const(7, 1, 1, 960);
    check_const(9, 1, 1, 800);
    check_const(10, 1, 1, 480);
    check_const(12, 1, 1, 400);
    check_const(13, 1, 1, 2048);
    check_const(13, 0, 1, 2048);

    /* sf scaling (A0 r2 rule, N5-blessed). */
    check_const(2, 1, 4, 7680);
    check_const(13, 1, 4, 8192);
    check_const(4, 1, 2, 3200);

    /* Fractional-row ceiling = max per-frame count (buffer sizing). */
    ac4_src_params(3, 1, 1, &num, &den, &per);
    CHECK(ac4_src_ceil(num, den) == 1602, "fri3 ceil=%ld want 1602",
          ac4_src_ceil(num, den));

    /* Phase rule: first-frame 0 -> 0; mid-stream 0 advances. */
    CHECK(ac4_src_eff(5, 0, 0, 1) == 0, "first-frame seq0");
    CHECK(ac4_src_eff(5, 4, 0, 0) == 0, "seq0 after eff4 wraps to 0");
    CHECK(ac4_src_eff(5, 2, 0, 0) == 3, "seq0 after eff2 -> 3");
    CHECK(ac4_src_eff(5, 0, 7, 0) == 2, "tune-in seq7 -> 2");
    CHECK(ac4_src_eff(5, 0, 7, 1) == 2, "first-frame seq7 -> 2");
    /* Counter wrap 1020->1 is phase-continuous mod 5. */
    CHECK(ac4_src_eff(5, 0, 1020, 0) == 0, "seq1020 -> 0");
    CHECK(ac4_src_eff(5, 0, 1, 0) == 1, "seq1 -> 1");

    /* OMAX covers every row (fri13/sf4 = 8192 is the max). */
    {
        int fri, fs, sf;
        for (fri = 0; fri <= 13; fri++)
            for (fs = 0; fs <= 1; fs++)
                for (sf = 1; sf <= 4; sf *= 2) {
                    ac4_src_params(fri, fs, sf, &num, &den, &per);
                    CHECK(ac4_src_ceil(num, den) <= AC4_SRC_OMAX,
                          "fri=%d fs=%d sf=%d ceil %ld > OMAX", fri, fs,
                          sf, ac4_src_ceil(num, den));
                }
    }

    /* VER fps preserved to display precision on every row. */
    {
        static const char *want[14] = {
            "23.98", "24.00", "25.00", "29.97", "30.00", "47.95",
            "48.00", "50.00", "59.94", "60.00", "100.00", "119.88",
            "120.00", "23.44",
        };
        int fri;
        for (fri = 0; fri <= 13; fri++) {
            char got[16];
            snprintf(got, sizeof got, "%.2f", ac4_src_fps(fri, 1));
            CHECK(strcmp(got, want[fri]) == 0, "fri=%d fps=%s want %s",
                  fri, got, want[fri]);
        }
    }

    if (fails == 0)
        printf("ac4_src_test: ALL PASS\n");
    return fails != 0;
}
