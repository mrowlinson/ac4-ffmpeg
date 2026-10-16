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
        ac4dsi_pres_t q = p;
        q.ch_mode = 11;
        CHECK(ac4dsi_serialize(&s, &q, 1, out, sizeof(out), &len) ==
              AC4DSI_EINVAL, "ch_mode 11");
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

    /* Mode rule helper (Table E.8). */
    CHECK(ac4dsi_mode_for_wait_frames(0) == 1, "wait0->1");
    CHECK(ac4dsi_mode_for_wait_frames(1) == 2, "wait1->2");
    CHECK(ac4dsi_mode_for_wait_frames(6) == 2, "wait6->2");
    CHECK(ac4dsi_mode_for_wait_frames(7) == 3, "wait7->3");
    CHECK(ac4dsi_mode_for_wait_frames(100) == 3, "wait100->3");

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
    test_negatives();
    if (fails == 0)
        printf("ALL PASS\n");
    else
        printf("%d FAILURES\n", fails);
    return fails != 0;
}
