/* dsi_test.c - byte-exact golden tests + unit negatives for ac4dsi.
 *
 * Goldens: DSI-CONTRACT.md section 5 (own dac4 extraction from ff-corpus
 * MP4s, probe cross-checked 272/272, 272/272, 416/416 bits, 0 skip).
 */
#include <stdio.h>
#include <string.h>

#include "ac4dsi.h"

static int fails = 0;

#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

/* Goldens as raw bytes (contract hex, spaces stripped). */
static const uint8_t G_DASHIF[34] = {
    0x20, 0xa4, 0x01, 0x80, 0x96, 0x30, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff,
    0xff, 0xf0, 0x01, 0x12, 0xf8, 0xa8, 0x00, 0x00, 0x42, 0x00, 0x00, 0x02,
    0x50, 0x10, 0x00, 0x00, 0x03, 0x10, 0x99, 0x5b, 0x80, 0x80
};
static const uint8_t G_LIVESIM2[34] = {
    0x20, 0xa6, 0x01, 0x80, 0x96, 0x30, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff,
    0xff, 0xf0, 0x01, 0x12, 0xf9, 0xa8, 0x00, 0x00, 0x48, 0x00, 0x00, 0x8e,
    0x50, 0x10, 0x00, 0x00, 0x8f, 0x10, 0x99, 0x5b, 0x80, 0x80
};
static const uint8_t G_BELIEVER[52] = {
    0x20, 0xba, 0x02, 0x40, 0x00, 0x7d, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xe0,
    0x02, 0x12, 0xf8, 0x80, 0x00, 0x00, 0x42, 0x00, 0x00, 0x02, 0x50, 0x10,
    0x00, 0x00, 0x03, 0x10, 0x99, 0x5b, 0xa0, 0xc0, 0x01, 0x12, 0xf8, 0x80,
    0x00, 0x00, 0x42, 0x00, 0x00, 0x02, 0x50, 0x10, 0x00, 0x00, 0x03, 0x10,
    0x99, 0x5b, 0x80, 0xc0
};
/* Uuid golden: 5.1 defaults, 128k, program id 300 + 16x0xAB uuid.
 * Hand-verified field-by-field (short=300, mode 2, rate 0x1f400, uuid
 * bytes verbatim in bit order); independent probe re-parsed exact. */
static const uint8_t G_UUID[50] = {
    0x20, 0xba, 0x01, 0x80, 0x96, 0x6a, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea,
    0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xe0, 0x00, 0x1f,
    0x40, 0x0f, 0xff, 0xff, 0xff, 0xf0, 0x01, 0x12, 0xf9, 0x80, 0x00, 0x00,
    0x48, 0x00, 0x00, 0x8e, 0x50, 0x10, 0x00, 0x00, 0x8f, 0x10, 0x99, 0x5b,
    0x80, 0x80
};
/* Believer pres0: pv2, 18 opaque bytes (payload[14:32]), round-tripped. */
static const uint8_t BELIEVER_PV2[18] = {
    0xf8, 0x80, 0x00, 0x00, 0x42, 0x00, 0x00, 0x02, 0x50, 0x10, 0x00, 0x00,
    0x03, 0x10, 0x99, 0x5b, 0xa0, 0xc0
};

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

static void wild_stream(ac4dsi_stream_t *s, uint8_t fr_idx)
{
    ac4dsi_stream_init(s);
    s->frame_rate_index = fr_idx;
    s->b_program_id = 1;
    s->short_program_id = 300;
    s->bit_rate_mode = 3;
    s->bit_rate = 0;
    s->bit_rate_precision = 0xFFFFFFFFu;
}

static void test_goldens(void)
{
    ac4dsi_stream_t s;
    ac4dsi_pres_t p[2];

    /* dashif: stereo wild shape, fr_idx 2, pres_id 10. */
    wild_stream(&s, 2);
    ac4dsi_pres_init_v1(&p[0]);
    CHECK(ac4dsi_pres_set_layout(&p[0], "stereo") == AC4DSI_OK, "stereo");
    p[0].presentation_id = 10;
    check_golden("dashif", &s, p, 1, G_DASHIF, sizeof(G_DASHIF));

    /* livesim2: 5.1 wild shape, fr_idx 3, pres_id 10. */
    wild_stream(&s, 3);
    ac4dsi_pres_init_v1(&p[0]);
    CHECK(ac4dsi_pres_set_layout(&p[0], "5.1") == AC4DSI_OK, "5.1");
    p[0].presentation_id = 10;
    check_golden("livesim2", &s, p, 1, G_LIVESIM2, sizeof(G_LIVESIM2));

    /* believer: no program id, mode2/256000, pv2-opaque + v1(pres_id 0,
     * immersive 1), fr_idx 13. */
    ac4dsi_stream_init(&s);
    CHECK(ac4dsi_mode_for_wait_frames(6) == 2, "wait6->mode2");
    s.bit_rate_mode = ac4dsi_mode_for_wait_frames(6);
    s.bit_rate = 256000;
    memset(&p[0], 0, sizeof(p[0]));
    p[0].presentation_version = 2;
    p[0].opaque = BELIEVER_PV2;
    p[0].opaque_len = sizeof(BELIEVER_PV2);
    ac4dsi_pres_init_v1(&p[1]);
    p[1].immersive_indicator = 1;
    check_golden("believer", &s, p, 2, G_BELIEVER, sizeof(G_BELIEVER));

    /* uuid: 5.1, 128k, program id 300 + uuid 0xAB * 16. */
    ac4dsi_stream_init(&s);
    s.bit_rate = 128000;
    s.b_program_id = 1;
    s.short_program_id = 300;
    s.b_uuid = 1;
    memset(s.program_uuid, 0xAB, sizeof(s.program_uuid));
    ac4dsi_pres_init_v1(&p[0]);
    CHECK(ac4dsi_pres_set_layout(&p[0], "5.1") == AC4DSI_OK, "5.1 uuid");
    check_golden("uuid", &s, p, 1, G_UUID, sizeof(G_UUID));
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

    ac4dsi_stream_init(&s);
    s.bit_rate = 64000;
    ac4dsi_pres_init_v1(&p);
    for (i = 0; i < 63; i++)
        tag[i] = (char)(i + 1); /* 1..63, all nonzero, all distinct */
    tag[63] = '\0';
    p.language_tag = tag;

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
    size_t len = 0, i;
    int rc;

    ac4dsi_stream_init(&s);
    s.bit_rate = 64000;
    memset(&p, 0, sizeof(p));
    p.presentation_version = 2;
    for (i = 0; i < sizeof(op); i++)
        op[i] = (uint8_t)(i * 7 + 1);
    p.opaque = op;
    p.opaque_len = sizeof(op);

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
    test_negatives();
    if (fails == 0)
        printf("ALL PASS\n");
    else
        printf("%d FAILURES\n", fails);
    return fails != 0;
}
