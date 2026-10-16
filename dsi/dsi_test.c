/* dsi_test.c - byte-exact golden tests + unit negatives for ac4dsi.
 *
 * Vectors come from dsi_goldens.h (shared with the putbits suite so the
 * two suites can never drift apart on inputs).
 */
#include <stdio.h>
#include <string.h>

#include "ac4dsi.h"
#include "dsi_goldens.h"

static int fails = 0;

#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static void hexdump(const char *tag, const uint8_t *a, const uint8_t *b,
                    size_t n)
{
    size_t i;
    printf("%s diff (got vs want):\n  got: ", tag);
    for (i = 0; i < n; i++)
        printf("%02x%s", a[i], i + 1 < n ? " " : "\n  exp: ");
    for (i = 0; i < n; i++)
        printf("%02x%s", b[i], i + 1 < n ? " " : "\n");
}

static void check_golden(const char *name, const ac4dsi_stream_t *s,
                         const ac4dsi_pres_t *pres, size_t n_pres,
                         const uint8_t *want, size_t want_len)
{
    uint8_t out[128];
    size_t got_len = 0;
    int rc;
    memset(out, 0xAA, sizeof(out));
    rc = ac4dsi_serialize(s, pres, n_pres, out, sizeof(out), &got_len);
    CHECK(rc == AC4DSI_OK, "%s: rc=%d want 0", name, rc);
    CHECK(got_len == want_len, "%s: len=%lu want %lu", name,
          (unsigned long)got_len, (unsigned long)want_len);
    if (rc == AC4DSI_OK && got_len == want_len &&
        memcmp(out, want, want_len) != 0) {
        hexdump(name, out, want, want_len);
        printf("FAIL %s: bytes differ\n", name);
        fails++;
    } else if (rc == AC4DSI_OK && got_len == want_len) {
        printf("ok %s (%lu B byte-exact)\n", name, (unsigned long)got_len);
    }
}

static void test_goldens(void)
{
    ac4dsi_stream_t s;
    ac4dsi_pres_t p[2];
    size_t n_pres = 0;

    dsi_golden_dashif(&s, p, &n_pres);
    check_golden("dashif", &s, p, n_pres, DSI_G_DASHIF,
                 sizeof(DSI_G_DASHIF));

    dsi_golden_livesim2(&s, p, &n_pres);
    check_golden("livesim2", &s, p, n_pres, DSI_G_LIVESIM2,
                 sizeof(DSI_G_LIVESIM2));

    dsi_golden_believer(&s, p, &n_pres);
    check_golden("believer", &s, p, n_pres, DSI_G_BELIEVER,
                 sizeof(DSI_G_BELIEVER));

    dsi_golden_uuid(&s, p, &n_pres);
    check_golden("uuid", &s, p, n_pres, DSI_G_UUID, sizeof(DSI_G_UUID));
}

/* Max-length (63) language tag: boundary-legal, bytes verbatim. The tag
 * starts at bit 114 of the v1 block (byte 14 + 2 bits, non-aligned). */
static void test_tag63(void)
{
    ac4dsi_stream_t s;
    ac4dsi_pres_t p;
    uint8_t out[128];
    char tag[64];
    size_t len = 0, i;
    int rc;

    dsi_shape_tag63(&s, &p, tag);

    memset(out, 0xAA, sizeof(out));
    rc = ac4dsi_serialize(&s, &p, 1, out, sizeof(out), &len);
    /* 12 B header + pv/pb + 79 B v1 (114 fixed bits + 63 tag + tail). */
    CHECK(rc == AC4DSI_OK, "tag63 rc=%d", rc);
    CHECK(len == 93, "tag63 len=%lu", (unsigned long)len);
    CHECK(out[12] == 1 && out[13] == 79, "tag63 framing pv=%u pb=%u",
          out[12], out[13]);
    if (rc == AC4DSI_OK && len == 93) {
        /* Reconstruct each tag byte from its bit position. */
        for (i = 0; i < 63; i++) {
            size_t bit = 114 + i * 8; /* offset inside v1 block */
            size_t k = 14 + bit / 8;  /* v1 block starts at out[14] */
            unsigned shift = (unsigned)(bit % 8);
            uint8_t got;
            if (shift == 0) {
                got = out[k];
            } else {
                got = (uint8_t)((out[k] << shift) |
                                (out[k + 1] >> (8 - shift)));
            }
            if (got != (uint8_t)tag[i]) {
                printf("FAIL tag63 byte %lu: got %02x want %02x\n",
                       (unsigned long)i, got, (uint8_t)tag[i]);
                fails++;
                break;
            }
        }
        if (i == 63)
            printf("ok tag63 (63 tag bytes verbatim)\n");
    }
}

/* Opaque pres_bytes escape: 300 B forces the 0xFF + u16 path. */
static void test_opaque_escape(void)
{
    ac4dsi_stream_t s;
    ac4dsi_pres_t p;
    static uint8_t op[300];
    static uint8_t out[512];
    size_t len = 0;
    int rc;

    dsi_shape_escape(&s, &p, op);

    rc = ac4dsi_serialize(&s, &p, 1, out, sizeof(out), &len);
    /* 12 B header + pv + 0xFF + u16(45) + 300 = 316. */
    CHECK(rc == AC4DSI_OK, "escape rc=%d", rc);
    CHECK(len == 316, "escape len=%lu", (unsigned long)len);
    CHECK(out[12] == 2 && out[13] == 255 && out[14] == 0 && out[15] == 45,
          "escape framing %u %u %u %u", out[12], out[13], out[14], out[15]);
    if (rc == AC4DSI_OK && len == 316 &&
        memcmp(out + 16, op, sizeof(op)) != 0) {
        printf("FAIL escape: opaque bytes differ\n");
        fails++;
    } else if (rc == AC4DSI_OK && len == 316) {
        printf("ok opaque-escape (300 B round-trip)\n");
    }
}

/* Mask-enum layout API (DSI-12): table pins + exotic rejects. Literals
 * below are the lavc AV_CH_LAYOUT_* bits (verified against the real
 * libavutil headers by _Static_assert in dsi_putbits_test.c). */
static void test_layout_masks(void)
{
    /* Exotics: near-misses and real lavc layouts we must NOT map.
     * 0x3f is the trap: AV_CH_LAYOUT_5POINT1_BACK (the wrapper takes
     * the 0x60f SIDE variant, so 0x3f must stay EINVAL here too). */
    static const uint64_t exotics[] = {
        0x0ULL, 0x1ULL, 0x2ULL, 0x4ULL /* mono */, 0x7ULL,
        0x33ULL /* quad */, 0x3fULL /* 5.1-back */,
        0x63fULL /* 7.1 */, 0x607ULL /* 5.1 minus surrounds */,
        0x60eULL /* 5.1 minus FL */, 0xe0fULL /* 5.1 + top-center */,
        0x60fULL | (1ULL << 32) /* 5.1 + stray high bit */,
        0x8000000000000003ULL /* stereo + stray high bit */,
        0xFFFFFFFFFFFFFFFFULL
    };
    ac4dsi_pres_t p;
    size_t i;
    unsigned b;
    int rc;

    /* Literals pinned (lavc cross-check is in the putbits suite). */
    CHECK(AC4DSI_CHMASK_STEREO == 0x3, "stereo mask literal");
    CHECK(AC4DSI_CHMASK_5POINT1 == 0x60f, "5.1 mask literal");

    /* Table count cannot drift from the table itself. */
    CHECK(AC4DSI_N_LAYOUTS == 2, "n_layouts==2");
    CHECK(sizeof(ac4dsi_layout_map) / sizeof(ac4dsi_layout_map[0]) ==
          AC4DSI_N_LAYOUTS, "table size matches count");

    /* Every table row round-trips through the setter. */
    for (i = 0; i < AC4DSI_N_LAYOUTS; i++) {
        ac4dsi_pres_init_v1(&p);
        p.md_compat = 7;
        p.ch_mode = 31;
        p.channel_groups = 0x3FFFFu;
        rc = ac4dsi_pres_set_layout_mask(&p,
                                         ac4dsi_layout_map[i].lavc_mask);
        CHECK(rc == AC4DSI_OK, "row %lu rc=%d", (unsigned long)i, rc);
        CHECK(p.md_compat == ac4dsi_layout_map[i].md_compat &&
              p.ch_mode == ac4dsi_layout_map[i].ch_mode &&
              p.channel_groups == ac4dsi_layout_map[i].channel_groups,
              "row %lu trio", (unsigned long)i);
    }

    /* Exact trios pinned (regression anchor, not just self-consistency). */
    ac4dsi_pres_init_v1(&p);
    CHECK(ac4dsi_pres_set_layout_mask(&p, AC4DSI_CHMASK_STEREO) ==
          AC4DSI_OK, "stereo rc");
    CHECK(p.md_compat == 0 && p.ch_mode == 1 && p.channel_groups == 1u,
          "stereo trio %u/%u/0x%x", p.md_compat, p.ch_mode,
          p.channel_groups);
    CHECK(ac4dsi_pres_set_layout_mask(&p, AC4DSI_CHMASK_5POINT1) ==
          AC4DSI_OK, "5.1 rc");
    CHECK(p.md_compat == 1 && p.ch_mode == 4 && p.channel_groups == 0x47u,
          "5.1 trio %u/%u/0x%x", p.md_compat, p.ch_mode, p.channel_groups);

    /* NULL pres rejects. */
    CHECK(ac4dsi_pres_set_layout_mask(NULL, AC4DSI_CHMASK_STEREO) ==
          AC4DSI_EINVAL, "mask null pres");

    /* Exotics reject and leave the trio untouched. */
    for (i = 0; i < sizeof(exotics) / sizeof(exotics[0]); i++) {
        ac4dsi_pres_init_v1(&p);
        p.md_compat = 7;
        p.ch_mode = 31;
        p.channel_groups = 0x3FFFFu;
        rc = ac4dsi_pres_set_layout_mask(&p, exotics[i]);
        CHECK(rc == AC4DSI_EINVAL, "exotic 0x%llx rc=%d",
              (unsigned long long)exotics[i], rc);
        CHECK(p.md_compat == 7 && p.ch_mode == 31 &&
              p.channel_groups == 0x3FFFFu, "exotic 0x%llx untouched",
              (unsigned long long)exotics[i]);
    }
    /* Every single-bit mask rejects (no 1-channel layout maps). */
    for (b = 0; b < 64; b++) {
        ac4dsi_pres_init_v1(&p);
        rc = ac4dsi_pres_set_layout_mask(&p, 1ULL << b);
        CHECK(rc == AC4DSI_EINVAL, "single bit %u rc=%d", b, rc);
    }

    /* Legacy string shim (DSI-12): same trios as the mask form. */
    ac4dsi_pres_init_v1(&p);
    CHECK(ac4dsi_pres_set_layout(&p, "stereo") == AC4DSI_OK, "shim stereo");
    CHECK(p.md_compat == 0 && p.ch_mode == 1 && p.channel_groups == 1u,
          "shim stereo trio");
    CHECK(ac4dsi_pres_set_layout(&p, "5.1") == AC4DSI_OK, "shim 5.1");
    CHECK(p.md_compat == 1 && p.ch_mode == 4 && p.channel_groups == 0x47u,
          "shim 5.1 trio");

    printf("layout-masks done\n");
}

static void test_negatives(void)
{
    ac4dsi_stream_t s;
    ac4dsi_pres_t p;
    uint8_t out[128];
    size_t len = 0;
    char longtag[65];
    int rc;

    ac4dsi_stream_init(&s);
    s.bit_rate = 64000;
    ac4dsi_pres_init_v1(&p);

    /* NULL handling. */
    CHECK(ac4dsi_serialize(NULL, &p, 1, out, sizeof(out), &len) ==
          AC4DSI_EINVAL, "null stream");
    CHECK(ac4dsi_serialize(&s, NULL, 1, out, sizeof(out), &len) ==
          AC4DSI_EINVAL, "null pres");
    CHECK(ac4dsi_serialize(&s, &p, 1, NULL, sizeof(out), &len) ==
          AC4DSI_EINVAL, "null out");
    CHECK(ac4dsi_serialize(&s, &p, 0, out, sizeof(out), &len) ==
          AC4DSI_EINVAL, "n_pres 0");
    CHECK(ac4dsi_pres_set_layout(NULL, "stereo") == AC4DSI_EINVAL,
          "layout null pres");
    CHECK(ac4dsi_pres_set_layout(&p, NULL) == AC4DSI_EINVAL,
          "layout null name");
    CHECK(ac4dsi_pres_set_layout(&p, "7.1.4") ==
          AC4DSI_EINVAL, "layout unknown");

    /* Header range checks. */
    {
        ac4dsi_stream_t b = s;
        b.frame_rate_index = 16;
        CHECK(ac4dsi_serialize(&b, &p, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "fr_idx 16");
    }
    {
        ac4dsi_stream_t b = s;
        b.fs_index = 2;
        CHECK(ac4dsi_serialize(&b, &p, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "fs 2");
    }
    {
        ac4dsi_stream_t b = s;
        b.bit_rate_mode = 4;
        CHECK(ac4dsi_serialize(&b, &p, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "mode 4");
    }
    {
        /* Table E.8 defines modes 1-3 only (DSI-15). */
        ac4dsi_stream_t b = s;
        b.bit_rate_mode = 0;
        CHECK(ac4dsi_serialize(&b, &p, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "mode 0");
    }
    {
        /* DSI-7: fresh init defaults (mode 2, rate unset) must fail
         * loudly, not emit mode-2/rate-0. */
        ac4dsi_stream_t b;
        ac4dsi_stream_init(&b);
        CHECK(ac4dsi_serialize(&b, &p, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "mode2 rate0 (unset default)");
    }
    {
        /* DSI-7: mode 2 with an explicit rate still emits. */
        ac4dsi_stream_t b;
        ac4dsi_stream_init(&b);
        b.bit_rate = 64000;
        len = 0;
        CHECK(ac4dsi_serialize(&b, &p, 1, out, sizeof(out), &len) ==
              AC4DSI_OK && len == 32, "mode2 rate64k emits");
    }
    {
        /* DSI-7: mode 3 (VBR) with rate 0 stays legal (wild vectors).
         * dashif/livesim2 goldens pin this too; assert the rc here. */
        ac4dsi_stream_t b;
        ac4dsi_stream_init(&b);
        b.bit_rate_mode = 3;
        CHECK(ac4dsi_serialize(&b, &p, 1, out, sizeof(out), &len) ==
              AC4DSI_OK, "mode3 rate0 ok");
    }
    {
        /* bsv<=1 carries no program-id block; asking for one is EINVAL. */
        ac4dsi_stream_t b = s;
        b.bitstream_version = 1;
        b.b_program_id = 1;
        CHECK(ac4dsi_serialize(&b, &p, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "bsv1+prog");
    }
    {
        ac4dsi_stream_t b = s;
        b.b_uuid = 1; /* uuid without program id */
        CHECK(ac4dsi_serialize(&b, &p, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "uuid w/o prog");
    }

    /* Presentation checks. */
    {
        ac4dsi_pres_t q = p;
        q.presentation_version = 0;
        CHECK(ac4dsi_serialize(&s, &q, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "pv0");
    }
    {
        ac4dsi_pres_t q = p;
        q.presentation_id = 32;
        CHECK(ac4dsi_serialize(&s, &q, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "pres_id 32");
    }
    {
        /* 11-14 need back/top modifier bits (no wire vectors); 15 emits. */
        unsigned m;
        for (m = 11; m <= 14; m++) {
            ac4dsi_pres_t q = p;
            q.ch_mode = (uint8_t)m;
            CHECK(ac4dsi_serialize(&s, &q, 1, out, sizeof(out), &len) ==
                  AC4DSI_EINVAL, "ch_mode %u reject", m);
        }
    }
    {
        ac4dsi_pres_t q = p;
        q.channel_groups = 0x40000u;
        CHECK(ac4dsi_serialize(&s, &q, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "mask 19 bits");
    }
    {
        ac4dsi_pres_t q = p;
        q.content_classifier = 8;
        CHECK(ac4dsi_serialize(&s, &q, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "classifier 8");
    }
    memset(longtag, 'x', sizeof(longtag) - 1);
    longtag[sizeof(longtag) - 1] = '\0';
    {
        ac4dsi_pres_t q = p;
        q.language_tag = longtag; /* 64 > 63 */
        CHECK(ac4dsi_serialize(&s, &q, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "lang 64");
    }
    {
        ac4dsi_pres_t q;
        memset(&q, 0, sizeof(q));
        q.presentation_version = 2;
        q.opaque_len = 4; /* opaque NULL with len>0 */
        CHECK(ac4dsi_serialize(&s, &q, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "opaque null");
    }

    /* Truncation: small buffer -> ENOSPC and needed size reported. */
    len = 0;
    rc = ac4dsi_serialize(&s, &p, 1, out, 10, &len);
    CHECK(rc == AC4DSI_ENOSPC, "small buf rc=%d", rc);
    CHECK(len == 32, "small buf need=%lu", (unsigned long)len);

    /* Exact fit succeeds; one byte short fails with the same need. */
    len = 0;
    rc = ac4dsi_serialize(&s, &p, 1, out, 32, &len);
    CHECK(rc == AC4DSI_OK && len == 32, "exact fit rc=%d len=%lu", rc,
          (unsigned long)len);
    len = 0;
    rc = ac4dsi_serialize(&s, &p, 1, out, 31, &len);
    CHECK(rc == AC4DSI_ENOSPC && len == 32, "off-by-one rc=%d len=%lu",
          rc, (unsigned long)len);

    /* out_len may be NULL (length simply not reported). */
    rc = ac4dsi_serialize(&s, &p, 1, out, sizeof(out), NULL);
    CHECK(rc == AC4DSI_OK, "out_len NULL rc=%d", rc);

    /* EINVAL leaves *out_len untouched (sentinel survives). */
    {
        ac4dsi_pres_t q = p;
        q.ch_mode = 11;
        len = 0xDEADu;
        rc = ac4dsi_serialize(&s, &q, 1, out, sizeof(out), &len);
        CHECK(rc == AC4DSI_EINVAL && len == 0xDEADu,
              "einval keeps len rc=%d len=%lu", rc, (unsigned long)len);
    }

    /* strerror equivalent for all codes + unknown. */
    CHECK(strcmp(ac4dsi_strerror(AC4DSI_OK), "ok") == 0, "str ok");
    CHECK(ac4dsi_strerror(AC4DSI_EINVAL) != NULL, "str einval");
    CHECK(ac4dsi_strerror(AC4DSI_ENOSPC) != NULL, "str enospc");
    CHECK(strcmp(ac4dsi_strerror(-999), "unknown error") == 0,
          "str unknown");

    /* Mode rule helper (Table E.8). Negatives are nonsense but documented
     * to follow the "else" row (DSI-10). */
    CHECK(ac4dsi_mode_for_wait_frames(0) == 1, "wait0->1");
    CHECK(ac4dsi_mode_for_wait_frames(1) == 2, "wait1->2");
    CHECK(ac4dsi_mode_for_wait_frames(6) == 2, "wait6->2");
    CHECK(ac4dsi_mode_for_wait_frames(7) == 3, "wait7->3");
    CHECK(ac4dsi_mode_for_wait_frames(100) == 3, "wait100->3");
    CHECK(ac4dsi_mode_for_wait_frames(-1) == 3, "wait-1->3");
    CHECK(ac4dsi_mode_for_wait_frames(-100) == 3, "wait-100->3");

    /* Sanity: default stereo synth is 32 B (12 B header without program
     * id + pv/pb + 18 B v1) with framing intact. */
    len = 0;
    rc = ac4dsi_serialize(&s, &p, 1, out, sizeof(out), &len);
    CHECK(rc == AC4DSI_OK && len == 32, "default stereo rc=%d len=%lu", rc,
          (unsigned long)len);
    /* Header is 12 B without program id -> pv at [12], pb at [13]. */
    CHECK(out[12] == 1 && out[13] == 18, "framing pv=%u pb=%u", out[12],
          out[13]);

    printf("negatives done\n");
}

int main(void)
{
    test_goldens();
    test_tag63();
    test_opaque_escape();
    test_layout_masks();
    test_negatives();
    if (fails == 0)
        printf("ALL PASS\n");
    else
        printf("%d FAILURES\n", fails);
    return fails != 0;
}
