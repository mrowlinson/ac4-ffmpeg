/* dsi_goldens.h - shared golden vectors + shape builders for the dsi suites.
 *
 * Both dsi_test (hand-rolled oracle) and dsi_putbits_test (PutBitContext
 * emitter) build their vectors from these builders, so the two suites
 * can never drift apart on inputs. Builders use the mask layout API
 * (DSI-12); the legacy string shim is covered separately in dsi_test.
 *
 * Goldens: DSI-CONTRACT.md section 5 (own dac4 extraction from ff-corpus
 * MP4s, probe cross-checked 272/272, 272/272, 416/416 bits, 0 skip).
 */
#ifndef DSI_GOLDENS_H
#define DSI_GOLDENS_H

#include <string.h>

#include "ac4dsi.h"

/* Goldens as raw bytes (contract hex, spaces stripped). */
static const uint8_t DSI_G_DASHIF[34] = {
    0x20, 0xa4, 0x01, 0x80, 0x96, 0x30, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff,
    0xff, 0xf0, 0x01, 0x12, 0xf8, 0xa8, 0x00, 0x00, 0x42, 0x00, 0x00, 0x02,
    0x50, 0x10, 0x00, 0x00, 0x03, 0x10, 0x99, 0x5b, 0x80, 0x80
};
static const uint8_t DSI_G_LIVESIM2[34] = {
    0x20, 0xa6, 0x01, 0x80, 0x96, 0x30, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff,
    0xff, 0xf0, 0x01, 0x12, 0xf9, 0xa8, 0x00, 0x00, 0x48, 0x00, 0x00, 0x8e,
    0x50, 0x10, 0x00, 0x00, 0x8f, 0x10, 0x99, 0x5b, 0x80, 0x80
};
static const uint8_t DSI_G_BELIEVER[52] = {
    0x20, 0xba, 0x02, 0x40, 0x00, 0x7d, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xe0,
    0x02, 0x12, 0xf8, 0x80, 0x00, 0x00, 0x42, 0x00, 0x00, 0x02, 0x50, 0x10,
    0x00, 0x00, 0x03, 0x10, 0x99, 0x5b, 0xa0, 0xc0, 0x01, 0x12, 0xf8, 0x80,
    0x00, 0x00, 0x42, 0x00, 0x00, 0x02, 0x50, 0x10, 0x00, 0x00, 0x03, 0x10,
    0x99, 0x5b, 0x80, 0xc0
};
/* Uuid golden: 5.1 defaults, 128k, program id 300 + 16x0xAB uuid.
 * Hand-verified field-by-field (short=300, mode 2, rate 0x1f400, uuid
 * bytes verbatim in bit order); independent probe re-parsed exact. */
static const uint8_t DSI_G_UUID[50] = {
    0x20, 0xba, 0x01, 0x80, 0x96, 0x6a, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea,
    0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xe0, 0x00, 0x1f,
    0x40, 0x0f, 0xff, 0xff, 0xff, 0xf0, 0x01, 0x12, 0xf9, 0x80, 0x00, 0x00,
    0x48, 0x00, 0x00, 0x8e, 0x50, 0x10, 0x00, 0x00, 0x8f, 0x10, 0x99, 0x5b,
    0x80, 0x80
};
/* Believer pres0: pv2, 18 opaque bytes (payload[14:32]), round-tripped. */
static const uint8_t DSI_BELIEVER_PV2[18] = {
    0xf8, 0x80, 0x00, 0x00, 0x42, 0x00, 0x00, 0x02, 0x50, 0x10, 0x00, 0x00,
    0x03, 0x10, 0x99, 0x5b, 0xa0, 0xc0
};

/* Wild-type stream: program id 300, VBR/unknown rate, given fr_idx. */
static inline void dsi_wild_stream(ac4dsi_stream_t *s, uint8_t fr_idx)
{
    ac4dsi_stream_init(s);
    s->frame_rate_index = fr_idx;
    s->b_program_id = 1;
    s->short_program_id = 300;
    s->bit_rate_mode = 3;
    s->bit_rate = 0;
    s->bit_rate_precision = 0xFFFFFFFFu;
}

/* dashif: stereo wild shape, fr_idx 2, pres_id 10. Needs p[1]. */
static inline void dsi_golden_dashif(ac4dsi_stream_t *s, ac4dsi_pres_t *p,
                                     size_t *n_pres)
{
    dsi_wild_stream(s, 2);
    ac4dsi_pres_init_v1(p);
    ac4dsi_pres_set_layout_mask(p, AC4DSI_CHMASK_STEREO);
    p->presentation_id = 10;
    *n_pres = 1;
}

/* livesim2: 5.1 wild shape, fr_idx 3, pres_id 10. Needs p[1]. */
static inline void dsi_golden_livesim2(ac4dsi_stream_t *s, ac4dsi_pres_t *p,
                                       size_t *n_pres)
{
    dsi_wild_stream(s, 3);
    ac4dsi_pres_init_v1(p);
    ac4dsi_pres_set_layout_mask(p, AC4DSI_CHMASK_5POINT1);
    p->presentation_id = 10;
    *n_pres = 1;
}

/* believer: no program id, mode2/256000, pv2-opaque + v1(pres_id 0,
 * immersive 1), fr_idx 13. Needs p[2]. */
static inline void dsi_golden_believer(ac4dsi_stream_t *s, ac4dsi_pres_t *p,
                                       size_t *n_pres)
{
    ac4dsi_stream_init(s);
    s->bit_rate_mode = ac4dsi_mode_for_wait_frames(6);
    s->bit_rate = 256000;
    memset(&p[0], 0, sizeof(p[0]));
    p[0].presentation_version = 2;
    p[0].opaque = DSI_BELIEVER_PV2;
    p[0].opaque_len = sizeof(DSI_BELIEVER_PV2);
    ac4dsi_pres_init_v1(&p[1]);
    p[1].immersive_indicator = 1;
    *n_pres = 2;
}

/* uuid: 5.1, 128k, program id 300 + uuid 0xAB * 16. Needs p[1]. */
static inline void dsi_golden_uuid(ac4dsi_stream_t *s, ac4dsi_pres_t *p,
                                   size_t *n_pres)
{
    ac4dsi_stream_init(s);
    s->bit_rate = 128000;
    s->b_program_id = 1;
    s->short_program_id = 300;
    s->b_uuid = 1;
    memset(s->program_uuid, 0xAB, sizeof(s->program_uuid));
    ac4dsi_pres_init_v1(p);
    ac4dsi_pres_set_layout_mask(p, AC4DSI_CHMASK_5POINT1);
    *n_pres = 1;
}

/* Max-length (63) language tag shape: boundary-legal, tag bytes verbatim.
 * tag must hold 64 bytes; the builder fills 1..63 + NUL. */
static inline void dsi_shape_tag63(ac4dsi_stream_t *s, ac4dsi_pres_t *p,
                                   char tag[64])
{
    size_t i;
    ac4dsi_stream_init(s);
    s->bit_rate = 64000;
    ac4dsi_pres_init_v1(p);
    for (i = 0; i < 63; i++)
        tag[i] = (char)(i + 1); /* 1..63, all nonzero, all distinct */
    tag[63] = '\0';
    p->language_tag = tag;
}

/* Opaque pres_bytes escape shape: 300 B forces the 0xFF + u16 path.
 * op must hold 300 bytes; the builder fills a nonzero pattern. */
static inline void dsi_shape_escape(ac4dsi_stream_t *s, ac4dsi_pres_t *p,
                                    uint8_t op[300])
{
    size_t i;
    ac4dsi_stream_init(s);
    s->bit_rate = 64000;
    memset(p, 0, sizeof(*p));
    p->presentation_version = 2;
    for (i = 0; i < 300; i++)
        op[i] = (uint8_t)(i * 7 + 1);
    p->opaque = op;
    p->opaque_len = 300;
}

#endif /* DSI_GOLDENS_H */
