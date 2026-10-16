/* dsi_putbits_test.c - differential tests: PutBitContext emitter
 * (ac4dsi_serialize_putbits) vs the hand-rolled oracle
 * (ac4dsi_serialize), against the REAL lavc headers.
 *
 * This TU only builds against a configured ffmpeg tree (Makefile
 * LAVCBASE provides config.h + libavutil). The _Static_asserts below
 * fail the build if the include path ever resolves to anything but
 * the real libavutil/channel_layout.h, and test_lavc_smoke pins real
 * put_bits() behavior (not just declarations).
 *
 * Vectors come from dsi_goldens.h (shared with dsi_test so the two
 * suites can never drift apart on inputs).
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "ac4dsi.h"
#include "ac4dsi_priv.h"
#include "ac4dsi_putbits.h"
#include "dsi_goldens.h"

#include "libavcodec/put_bits.h"
#include "libavutil/channel_layout.h"

/* DSI-12 mask literals must equal the real lavc bits. */
_Static_assert(AV_CH_LAYOUT_STEREO == (uint64_t)AC4DSI_CHMASK_STEREO,
               "stereo mask drift vs libavutil");
_Static_assert(AV_CH_LAYOUT_5POINT1 == (uint64_t)AC4DSI_CHMASK_5POINT1,
               "5.1 mask drift vs libavutil");

static int fails = 0;
static unsigned long n_ok = 0, n_nospc = 0, n_inval = 0;

#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

/* Fuzz compare buffers: worst case is header + 4 max opaques. */
#define FZ_BUF 280000u
static uint8_t FZA[FZ_BUF];
static uint8_t FZB[FZ_BUF];
static char FZTAG[4][72];
static uint8_t FZOPQ[4][66000];

/* Run both emitters at cap, compare rc/len/bytes. EINVAL must leave
 * both buffers (0xAA-filled) and the len sentinel untouched. */
static void compare_both(const char *name, const ac4dsi_stream_t *s,
                         const ac4dsi_pres_t *pres, size_t n_pres,
                         size_t cap)
{
    size_t lenA = 0xDEADu, lenB = 0xDEADu;
    int rcA, rcB;
    if (cap > FZ_BUF)
        cap = FZ_BUF;
    memset(FZA, 0xAA, cap);
    memset(FZB, 0xAA, cap);
    rcA = ac4dsi_serialize(s, pres, n_pres, FZA, cap, &lenA);
    rcB = ac4dsi_serialize_putbits(s, pres, n_pres, FZB, cap, &lenB);
    if (rcA != rcB || lenA != lenB) {
        printf("FAIL %s cap=%lu: hand(rc=%d len=%lu) putbits(rc=%d len=%lu)\n",
               name, (unsigned long)cap, rcA, (unsigned long)lenA, rcB,
               (unsigned long)lenB);
        fails++;
        return;
    }
    if (rcA == AC4DSI_OK)
        n_ok++;
    else if (rcA == AC4DSI_ENOSPC)
        n_nospc++;
    else
        n_inval++;
    if (memcmp(FZA, FZB, cap) != 0) {
        size_t i;
        printf("FAIL %s cap=%lu rc=%d: bytes differ\n", name,
               (unsigned long)cap, rcA);
        for (i = 0; i < cap; i++) {
            if (FZA[i] != FZB[i]) {
                printf("  first diff at %lu: hand=%02x putbits=%02x\n",
                       (unsigned long)i, FZA[i], FZB[i]);
                break;
            }
        }
        fails++;
        return;
    }
    if (rcA == AC4DSI_EINVAL) {
        size_t i;
        /* Contract: EINVAL leaves out and *out_len untouched. */
        if (lenA != 0xDEADu) {
            printf("FAIL %s cap=%lu: EINVAL clobbered len (%lu)\n", name,
                   (unsigned long)cap, (unsigned long)lenA);
            fails++;
        }
        for (i = 0; i < cap; i++) {
            if (FZA[i] != 0xAA) {
                printf("FAIL %s cap=%lu: EINVAL clobbered out[%lu]\n",
                       name, (unsigned long)cap, (unsigned long)i);
                fails++;
                break;
            }
        }
    }
    /* out_len==NULL parity: same rc, same bytes. */
    memset(FZA, 0xAA, cap);
    memset(FZB, 0xAA, cap);
    rcA = ac4dsi_serialize(s, pres, n_pres, FZA, cap, NULL);
    rcB = ac4dsi_serialize_putbits(s, pres, n_pres, FZB, cap, NULL);
    if (rcA != rcB || memcmp(FZA, FZB, cap) != 0) {
        printf("FAIL %s cap=%lu NULL-len: rc %d vs %d or bytes differ\n",
               name, (unsigned long)cap, rcA, rcB);
        fails++;
    }
}

/* Behavioral pin that this TU exercises the REAL put_bits(). */
static void test_lavc_smoke(void)
{
    uint8_t buf[16];
    PutBitContext pb;
    memset(buf, 0, sizeof(buf));
    init_put_bits(&pb, buf, sizeof(buf));
    put_bits(&pb, 3, 1);
    put_bits(&pb, 7, 2);
    align_put_bits(&pb);
    flush_put_bits(&pb);
    CHECK(put_bits_count(&pb) == 16, "smoke count=%d",
          put_bits_count(&pb));
    CHECK(buf[0] == 0x20 && buf[1] == 0x80, "smoke bytes %02x %02x",
          buf[0], buf[1]);
    /* 32-bit path (bit_rate fields go through put_bits32). */
    memset(buf, 0, sizeof(buf));
    init_put_bits(&pb, buf, sizeof(buf));
    put_bits32(&pb, 0xDEADBEEFu);
    flush_put_bits(&pb);
    CHECK(buf[0] == 0xDE && buf[1] == 0xAD && buf[2] == 0xBE &&
          buf[3] == 0xEF, "smoke32 %02x%02x%02x%02x", buf[0], buf[1],
          buf[2], buf[3]);
    printf("ok lavc-smoke (real PutBitContext)\n");
}

/* capbits clamp pin (MINOR-1): cap*8 below the ceiling, clamped to
 * INT_MAX*8 at/above it. Unreachable end-to-end (need << INT_MAX), but
 * the helper is unit-pinned so the honesty bound cannot drift. */
static void test_capbits_clamp(void)
{
    size_t maxbits = (size_t)INT_MAX * 8u;
    CHECK(ac4dsi_capbits_for_cap(0) == 0, "capbits 0");
    CHECK(ac4dsi_capbits_for_cap(64) == 512, "capbits 64");
    CHECK(ac4dsi_capbits_for_cap((size_t)INT_MAX) == maxbits,
          "capbits INT_MAX");
    CHECK(ac4dsi_capbits_for_cap((size_t)INT_MAX + 1) == maxbits,
          "capbits INT_MAX+1");
    CHECK(ac4dsi_capbits_for_cap(SIZE_MAX) == maxbits, "capbits SIZE_MAX");
    printf("ok capbits-clamp\n");
}

static void test_null_parity(void)
{
    ac4dsi_stream_t s;
    ac4dsi_pres_t p;
    uint8_t out[64];
    size_t l1, l2;
    int r1, r2;
    ac4dsi_stream_init(&s);
    s.bit_rate = 64000;
    ac4dsi_pres_init_v1(&p);
    r1 = ac4dsi_serialize(NULL, &p, 1, out, sizeof(out), &l1);
    r2 = ac4dsi_serialize_putbits(NULL, &p, 1, out, sizeof(out), &l2);
    CHECK(r1 == r2 && r1 == AC4DSI_EINVAL, "null stream %d/%d", r1, r2);
    r1 = ac4dsi_serialize(&s, NULL, 1, out, sizeof(out), &l1);
    r2 = ac4dsi_serialize_putbits(&s, NULL, 1, out, sizeof(out), &l2);
    CHECK(r1 == r2 && r1 == AC4DSI_EINVAL, "null pres %d/%d", r1, r2);
    r1 = ac4dsi_serialize(&s, &p, 1, NULL, sizeof(out), &l1);
    r2 = ac4dsi_serialize_putbits(&s, &p, 1, NULL, sizeof(out), &l2);
    CHECK(r1 == r2 && r1 == AC4DSI_EINVAL, "null out %d/%d", r1, r2);
    r1 = ac4dsi_serialize(&s, &p, 0, out, sizeof(out), &l1);
    r2 = ac4dsi_serialize_putbits(&s, &p, 0, out, sizeof(out), &l2);
    CHECK(r1 == r2 && r1 == AC4DSI_EINVAL, "n_pres 0 %d/%d", r1, r2);
    printf("ok null-parity\n");
}

/* All goldens x every cap 0..need+1, plus putbits-vs-golden direct. */
static void test_golden_sweep(void)
{
    static const struct {
        const char *name;
        void (*build)(ac4dsi_stream_t *, ac4dsi_pres_t *, size_t *);
        const uint8_t *golden;
        size_t golden_len;
    } cases[] = {
        { "dashif", dsi_golden_dashif, DSI_G_DASHIF,
          sizeof(DSI_G_DASHIF) },
        { "livesim2", dsi_golden_livesim2, DSI_G_LIVESIM2,
          sizeof(DSI_G_LIVESIM2) },
        { "believer", dsi_golden_believer, DSI_G_BELIEVER,
          sizeof(DSI_G_BELIEVER) },
        { "uuid", dsi_golden_uuid, DSI_G_UUID, sizeof(DSI_G_UUID) },
    };
    size_t c, cap;
    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        ac4dsi_stream_t s;
        ac4dsi_pres_t p[2];
        size_t n_pres = 0, len = 0;
        int rc;
        cases[c].build(&s, p, &n_pres);
        /* Direct wire-truth pin: putbits output == golden bytes. */
        memset(FZB, 0xAA, 128);
        rc = ac4dsi_serialize_putbits(&s, p, n_pres, FZB, 128, &len);
        CHECK(rc == AC4DSI_OK && len == cases[c].golden_len,
              "%s putbits rc=%d len=%lu", cases[c].name, rc,
              (unsigned long)len);
        if (rc == AC4DSI_OK && len == cases[c].golden_len &&
            memcmp(FZB, cases[c].golden, len) != 0) {
            printf("FAIL %s: putbits output != golden bytes\n",
                   cases[c].name);
            fails++;
        }
        for (cap = 0; cap <= cases[c].golden_len + 1; cap++)
            compare_both(cases[c].name, &s, p, n_pres, cap);
        printf("ok sweep-%s (0..%lu)\n", cases[c].name,
               (unsigned long)cases[c].golden_len + 1);
    }
}

/* tag63 + opaque-escape shapes at boundary caps. */
static void test_shape_sweep(void)
{
    static const size_t tag_caps[] = { 0, 1, 10, 92, 93, 94, 128 };
    static const size_t esc_caps[] = { 0, 1, 10, 315, 316, 317, 512 };
    ac4dsi_stream_t s;
    ac4dsi_pres_t p;
    char tag[64];
    static uint8_t op[300];
    size_t i;
    dsi_shape_tag63(&s, &p, tag);
    for (i = 0; i < sizeof(tag_caps) / sizeof(tag_caps[0]); i++)
        compare_both("tag63", &s, &p, 1, tag_caps[i]);
    printf("ok sweep-tag63\n");
    dsi_shape_escape(&s, &p, op);
    for (i = 0; i < sizeof(esc_caps) / sizeof(esc_caps[0]); i++)
        compare_both("escape", &s, &p, 1, esc_caps[i]);
    printf("ok sweep-escape\n");
}

/* Explicit wire boundaries: max opaque, just-over opaque, maxed v1. */
static void test_boundaries(void)
{
    ac4dsi_stream_t s;
    ac4dsi_pres_t p[4];
    char maxtag[64];
    size_t i, need = 0;
    int rc;

    ac4dsi_stream_init(&s);
    s.bit_rate = 64000;

    /* Max-legal opaque (65790 = 255 + 65535 escape ceiling). */
    for (i = 0; i < 65790; i++)
        FZOPQ[0][i] = (uint8_t)(i * 13 + 5);
    memset(&p[0], 0, sizeof(p[0]));
    p[0].presentation_version = 2;
    p[0].opaque = FZOPQ[0];
    p[0].opaque_len = 65790;
    rc = ac4dsi_serialize(&s, &p[0], 1, FZA, FZ_BUF, &need);
    CHECK(rc == AC4DSI_OK, "max-opaque setup rc=%d", rc);
    if (rc == AC4DSI_OK) {
        compare_both("max-opaque", &s, &p[0], 1, 100);
        if (need >= 1)
            compare_both("max-opaque", &s, &p[0], 1, need - 1);
        compare_both("max-opaque", &s, &p[0], 1, need);
    }

    /* Just over the ceiling: EINVAL parity. */
    p[0].opaque_len = 65791;
    compare_both("over-opaque", &s, &p[0], 1, 1024);

    /* Maxed v1: every field at its legal ceiling (tag 63, pres_id 31,
     * mask 18 bits, md 7, ch 31, classifier 7). */
    for (i = 0; i < 63; i++)
        maxtag[i] = (char)(0x7F - i);
    maxtag[63] = '\0';
    ac4dsi_pres_init_v1(&p[0]);
    p[0].md_compat = 7;
    p[0].presentation_id = 31;
    p[0].ch_mode = 31;
    p[0].channel_groups = 0x3FFFFu;
    p[0].content_classifier = 7;
    p[0].language_tag = maxtag;
    rc = ac4dsi_serialize(&s, &p[0], 1, FZA, FZ_BUF, &need);
    CHECK(rc == AC4DSI_OK, "max-v1 setup rc=%d", rc);
    if (rc == AC4DSI_OK) {
        if (need >= 1)
            compare_both("max-v1", &s, &p[0], 1, need - 1);
        compare_both("max-v1", &s, &p[0], 1, need);
    }

    /* 4-pres mixed stack: v1 stereo + pv2 + pv3 + pv255-empty. */
    ac4dsi_pres_init_v1(&p[0]);
    ac4dsi_pres_set_layout_mask(&p[0], AC4DSI_CHMASK_STEREO);
    memset(&p[1], 0, sizeof(p[1]));
    p[1].presentation_version = 2;
    p[1].opaque = FZOPQ[1];
    p[1].opaque_len = 1000;
    for (i = 0; i < 1000; i++)
        FZOPQ[1][i] = (uint8_t)(i + 1);
    memset(&p[2], 0, sizeof(p[2]));
    p[2].presentation_version = 3;
    p[2].opaque = FZOPQ[2];
    p[2].opaque_len = 255; /* exactly at the escape threshold */
    for (i = 0; i < 255; i++)
        FZOPQ[2][i] = (uint8_t)(255 - i);
    memset(&p[3], 0, sizeof(p[3]));
    p[3].presentation_version = 255;
    p[3].opaque = NULL;
    p[3].opaque_len = 0; /* empty entry is legal */
    rc = ac4dsi_serialize(&s, p, 4, FZA, FZ_BUF, &need);
    CHECK(rc == AC4DSI_OK, "mix4 setup rc=%d", rc);
    if (rc == AC4DSI_OK) {
        compare_both("mix4", &s, p, 4, 64);
        if (need >= 1)
            compare_both("mix4", &s, p, 4, need - 1);
        compare_both("mix4", &s, p, 4, need);
    }
    printf("ok boundaries\n");
}

/* Full ch_mode sweep (0..31 incl. 11-14 rejects) x md x content shape. */
static void test_chmode_sweep(void)
{
    static const uint8_t mds[] = { 0, 1, 7 };
    ac4dsi_stream_t s;
    ac4dsi_pres_t p;
    unsigned m, mi, ci;
    ac4dsi_stream_init(&s);
    s.bit_rate = 64000;
    for (m = 0; m < 32; m++) {
        for (mi = 0; mi < sizeof(mds) / sizeof(mds[0]); mi++) {
            for (ci = 0; ci < 3; ci++) {
                size_t need = 0;
                int rc;
                ac4dsi_pres_init_v1(&p);
                p.md_compat = mds[mi];
                p.ch_mode = (uint8_t)m;
                if (ci == 1)
                    p.language_tag = NULL; /* classifier-only */
                else if (ci == 2)
                    p.b_content_type = 0; /* no content block */
                rc = ac4dsi_serialize(&s, &p, 1, FZA, FZ_BUF, &need);
                if (rc == AC4DSI_OK && need >= 1)
                    compare_both("chmode", &s, &p, 1, need - 1);
                compare_both("chmode", &s, &p, 1,
                             rc == AC4DSI_OK ? need : 64);
            }
        }
    }
    printf("ok sweep-chmode (0..31 x md x content)\n");
}

/* Mask-API layouts x pres shapes at exact/off-by-one caps. */
static void test_layout_sweep(void)
{
    static const uint64_t masks[] = {
        AC4DSI_CHMASK_STEREO, AC4DSI_CHMASK_5POINT1
    };
    static const uint8_t ids[] = { 0, 10, 31 };
    ac4dsi_stream_t s;
    ac4dsi_pres_t p;
    size_t mi, ii;
    unsigned tail;
    ac4dsi_stream_init(&s);
    s.bit_rate = 128000;
    for (mi = 0; mi < 2; mi++) {
        for (ii = 0; ii < 3; ii++) {
            for (tail = 0; tail < 4; tail++) {
                size_t need = 0;
                int rc;
                ac4dsi_pres_init_v1(&p);
                ac4dsi_pres_set_layout_mask(&p, masks[mi]);
                p.presentation_id = ids[ii];
                p.de_indicator = (int)(tail & 1u);
                p.immersive_indicator = (int)((tail >> 1) & 1u);
                rc = ac4dsi_serialize(&s, &p, 1, FZA, FZ_BUF, &need);
                CHECK(rc == AC4DSI_OK, "layout sweep setup rc=%d", rc);
                if (rc == AC4DSI_OK && need >= 1)
                    compare_both("layout", &s, &p, 1, need - 1);
                compare_both("layout", &s, &p, 1, need);
            }
        }
    }
    printf("ok sweep-layout (masks x ids x DE/imm)\n");
}

/* Deterministic fuzz: LCG, fixed seed/count. */
static uint64_t fz_state = 0x12345678abcdefULL;

static uint64_t fz_next(void)
{
    fz_state = fz_state * 6364136223846793005ULL +
               1442695040888963407ULL;
    return fz_state >> 11;
}

static uint64_t fz_lt(uint64_t n)
{
    return n ? fz_next() % n : 0;
}

/* Mostly-valid draw (MINOR-4): uniform in [0,valid_n), except a 1-in-rare
 * draw goes wide ([0,wide_n)) so EINVAL parity keeps its coverage while
 * OK diversity rides fuzz too. */
static uint64_t fz_biased(uint64_t valid_n, uint64_t wide_n, uint64_t rare)
{
    if (fz_lt(rare) == 0)
        return fz_lt(wide_n);
    return fz_lt(valid_n);
}

static void fuzz_case(unsigned long caseno)
{
    static const uint64_t fz_masks[] = {
        0x3ULL, 0x60fULL, 0x0ULL, 0x4ULL, 0x33ULL, 0x3fULL, 0x63fULL,
        0x607ULL, 0xFFFFFFFFFFFFFFFFULL
    };
    static const size_t fz_oplens[] = {
        0, 1, 2, 254, 255, 256, 300, 1000, 65790, 65791
    };
    ac4dsi_stream_t s;
    ac4dsi_pres_t p[4];
    size_t n_pres, i, need = 0, cap;
    int rc;

    ac4dsi_stream_init(&s);
    /* Mostly bsv 2..3; sometimes 0..1 (then prog flags stay 0 so the
     * version itself gets emitted); rarely >127 (EINVAL). */
    if (fz_lt(40) == 0)
        s.bitstream_version = (uint8_t)(128 + fz_lt(128));
    else if (fz_lt(12) == 0)
        s.bitstream_version = (uint8_t)fz_lt(2);
    else
        s.bitstream_version = (uint8_t)(2 + fz_lt(2));
    s.fs_index = (uint8_t)fz_biased(2, 3, 12);
    s.frame_rate_index = (uint8_t)fz_biased(16, 18, 12);
    if (s.bitstream_version <= 1u && fz_lt(8) != 0) {
        s.b_program_id = 0;
        s.b_uuid = 0;
    } else {
        s.b_program_id = (int)fz_biased(2, 3, 12);
        if (s.b_program_id == 1)
            s.b_uuid = (int)fz_biased(2, 3, 12);
        else if (fz_lt(12) == 0)
            s.b_uuid = (int)(1 + fz_lt(2)); /* uuid w/o prog / flag 2 */
        else
            s.b_uuid = 0;
    }
    s.short_program_id = fz_lt(4) == 0 ? 300 : (uint16_t)fz_lt(65536);
    for (i = 0; i < sizeof(s.program_uuid); i++)
        s.program_uuid[i] = (uint8_t)fz_next();
    s.bit_rate_mode = fz_lt(10) == 0 ? (uint8_t)fz_lt(5)
                                     : (uint8_t)(1 + fz_lt(3));
    s.bit_rate = (uint32_t[]){ 0, 64000, 128000, 256000,
                               (uint32_t)fz_next() }[fz_lt(5)];
    s.bit_rate_precision = (uint32_t[]){ 0, 0xFFFFFFFFu,
                                         (uint32_t)fz_next() }[fz_lt(3)];

    n_pres = 1 + fz_lt(4);
    for (i = 0; i < n_pres; i++) {
        static const uint8_t pvs[] = { 1, 1, 1, 1, 2, 2, 3, 5, 255, 2 };
        /* pv0 (legacy reject) is rare but still covered. */
        uint8_t pv = fz_lt(12) == 0 ? 0
                                    : pvs[fz_lt(sizeof(pvs) /
                                                sizeof(pvs[0]))];
        memset(&p[i], 0, sizeof(p[i]));
        p[i].presentation_version = pv;
        if (pv == 1) {
            size_t tlen, k;
            unsigned m;
            ac4dsi_pres_init_v1(&p[i]);
            p[i].md_compat = (uint8_t)fz_biased(8, 10, 12);
            p[i].b_presentation_id = (int)fz_biased(2, 3, 12);
            p[i].presentation_id = (uint8_t)fz_biased(32, 40, 12);
            if (fz_lt(8) == 0) {
                p[i].ch_mode = (uint8_t)fz_lt(32); /* incl 11-14 */
            } else {
                m = (unsigned)fz_lt(28);
                p[i].ch_mode = (uint8_t)(m + (m >= 11 ? 4 : 0));
            }
            p[i].channel_groups = (uint32_t)fz_biased(1u << 18, 1u << 20,
                                                      12);
            /* Sometimes set the trio through the mask API (exotics
             * leave the random trio in place: setter fails clean). */
            if (fz_lt(3) == 0) {
                ac4dsi_pres_set_layout_mask(
                    &p[i], fz_masks[fz_lt(sizeof(fz_masks) /
                                          sizeof(fz_masks[0]))]);
            }
            p[i].b_content_type = (int)fz_biased(2, 3, 12);
            p[i].content_classifier = (uint8_t)fz_biased(8, 10, 12);
            switch (fz_lt(5)) {
            case 0:
                p[i].language_tag = NULL;
                break;
            case 1:
                p[i].language_tag = "";
                break;
            case 2:
                p[i].language_tag = "en";
                break;
            default:
                /* Mostly <= 63 (legal); sometimes 64..70 (EINVAL). */
                tlen = fz_lt(12) == 0 ? 64 + fz_lt(7) : fz_lt(64);
                for (k = 0; k < tlen; k++)
                    FZTAG[i][k] = (char)(1 + fz_lt(90));
                FZTAG[i][tlen] = '\0';
                p[i].language_tag = FZTAG[i];
                break;
            }
            p[i].de_indicator = (int)fz_biased(2, 3, 12);
            p[i].immersive_indicator = (int)fz_biased(2, 3, 12);
        } else if (pv != 0) {
            size_t olen, k;
            if (fz_lt(4) == 0)
                olen = fz_lt(66000);
            else
                olen = fz_oplens[fz_lt(sizeof(fz_oplens) /
                                       sizeof(fz_oplens[0]))];
            if (olen > sizeof(FZOPQ[i]))
                olen = sizeof(FZOPQ[i]);
            for (k = 0; k < olen; k++)
                FZOPQ[i][k] = (uint8_t)(caseno + k * 7 + 1);
            p[i].opaque_len = olen;
            /* Rarely NULL bytes with len>0 (legal iff len==0). */
            p[i].opaque = (olen == 0 || fz_lt(12) != 0) ? FZOPQ[i] : NULL;
        }
    }

    /* Learn need with a full-size oracle run, then compare at a
     * need-relative cap (mostly exact/off-by-one: partial parity). */
    rc = ac4dsi_serialize(&s, p, n_pres, FZA, FZ_BUF, &need);
    if (rc == AC4DSI_OK) {
        switch (fz_lt(8)) {
        case 0:
            cap = 0;
            break;
        case 1:
            cap = fz_lt(33);
            break;
        case 2:
            cap = need / 2;
            break;
        case 3:
            cap = need >= 1 ? need - 1 : 0;
            break;
        case 4:
            cap = need;
            break;
        case 5:
            cap = need + 1;
            break;
        case 6:
            cap = need + 7;
            break;
        default:
            cap = FZ_BUF;
            break;
        }
        if (cap > FZ_BUF)
            cap = FZ_BUF;
    } else {
        static const size_t abs_caps[] = { 0, 1, 31, 32, 64, 1024 };
        cap = abs_caps[fz_lt(sizeof(abs_caps) / sizeof(abs_caps[0]))];
    }
    compare_both("fuzz", &s, p, n_pres, cap);
}

static void test_fuzz(void)
{
    unsigned long i;
    const unsigned long N = 3000;
    for (i = 0; i < N; i++) {
        fuzz_case(i);
        if (fails > 20) {
            printf("fuzz: too many failures, stopping early\n");
            break;
        }
    }
    printf("ok fuzz (%lu cases)\n", (unsigned long)(i));
}

int main(void)
{
    test_lavc_smoke();
    test_capbits_clamp();
    test_null_parity();
    test_golden_sweep();
    test_shape_sweep();
    test_boundaries();
    test_chmode_sweep();
    test_layout_sweep();
    test_fuzz();
    printf("tally: ok=%lu enospc=%lu einval=%lu\n", n_ok, n_nospc, n_inval);
    if (fails == 0)
        printf("ALL PASS\n");
    else
        printf("%d FAILURES\n", fails);
    return fails != 0;
}
