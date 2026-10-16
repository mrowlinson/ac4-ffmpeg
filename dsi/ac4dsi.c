/**
 * @file ac4dsi.c
 * @brief Standalone AC-4 DSI serializer implementation.
 *
 * Bit layout: DSI-CONTRACT.md section 2 (header + pv framing + v1 block),
 * cross-checked field-by-field against tools/dac4_probe.py parse order.
 * All multi-bit fields are big-endian bit order (uimsbf); byte_align pads
 * with zero bits.
 */
#include "ac4dsi.h"
#include "ac4dsi_priv.h"

#include <string.h>

typedef struct {
    uint8_t *buf;
    size_t   cap;   /* bytes */
    size_t   nbits; /* bits written */
    int      over;  /* sticky overflow flag */
} bitwriter_t;

static void bw_put(bitwriter_t *w, uint32_t val, int nbits)
{
    int i;
    if (nbits <= 0)
        return;
    for (i = nbits - 1; i >= 0; i--) {
        size_t byte = w->nbits >> 3;
        int bit = 7 - (int)(w->nbits & 7u);
        if (byte >= w->cap) {
            /* Keep counting so ENOSPC can report the needed size. */
            w->over = 1;
            w->nbits++;
            continue;
        }
        if ((val >> (unsigned)i) & 1u)
            w->buf[byte] |= (uint8_t)(1u << (unsigned)bit);
        w->nbits++;
    }
}

static void bw_align(bitwriter_t *w)
{
    while (w->nbits & 7u)
        bw_put(w, 0, 1);
}

static void bw_bytes(bitwriter_t *w, const uint8_t *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        bw_put(w, p[i], 8);
}

void ac4dsi_stream_init(ac4dsi_stream_t *s)
{
    memset(s, 0, sizeof(*s));
    s->bitstream_version = 2;
    s->fs_index = 1;
    s->frame_rate_index = 13;
    s->b_program_id = 0;
    s->bit_rate_mode = 2;
    s->bit_rate = 0; /* NOT emittable in mode 2: serialize rejects it
                      * (DSI-7), so the caller must set a target rate */
    s->bit_rate_precision = 0xFFFFFFFFu;
}

void ac4dsi_pres_init_v1(ac4dsi_pres_t *p)
{
    memset(p, 0, sizeof(*p));
    p->presentation_version = 1;
    p->md_compat = 0;
    p->b_presentation_id = 1;
    p->presentation_id = 0;
    p->ch_mode = 1;
    p->channel_groups = 0x000001u;
    p->b_content_type = 1;
    p->content_classifier = 0;
    p->language_tag = "en";
    p->de_indicator = 1;
    p->immersive_indicator = 0;
}

/* lavc channel_layout mask -> DSI channel trio (DSI-12). Row order is
 * the stereo/5.1 pair the lavc encoder wrapper accepts; keep in sync
 * with the ac4dsi_chmask_t enum. */
const ac4dsi_layout_map_t ac4dsi_layout_map[AC4DSI_N_LAYOUTS] = {
    { AC4DSI_CHMASK_STEREO, 0, 1, 0x000001u },
    { AC4DSI_CHMASK_5POINT1, 1, 4, 0x000047u },
};

int ac4dsi_pres_set_layout_mask(ac4dsi_pres_t *p, uint64_t lavc_mask)
{
    size_t i;
    if (!p)
        return AC4DSI_EINVAL;
    for (i = 0; i < AC4DSI_N_LAYOUTS; i++) {
        if (ac4dsi_layout_map[i].lavc_mask == lavc_mask) {
            p->md_compat = ac4dsi_layout_map[i].md_compat;
            p->ch_mode = ac4dsi_layout_map[i].ch_mode;
            p->channel_groups = ac4dsi_layout_map[i].channel_groups;
            return AC4DSI_OK;
        }
    }
    return AC4DSI_EINVAL;
}

int ac4dsi_pres_set_layout(ac4dsi_pres_t *p, const char *layout)
{
    if (!p || !layout)
        return AC4DSI_EINVAL;
    if (strcmp(layout, "stereo") == 0)
        return ac4dsi_pres_set_layout_mask(p, AC4DSI_CHMASK_STEREO);
    if (strcmp(layout, "5.1") == 0)
        return ac4dsi_pres_set_layout_mask(p, AC4DSI_CHMASK_5POINT1);
    return AC4DSI_EINVAL;
}

const char *ac4dsi_strerror(int rc)
{
    switch (rc) {
    case AC4DSI_OK:
        return "ok";
    case AC4DSI_EINVAL:
        return "invalid argument / unsupported shape";
    case AC4DSI_ENOSPC:
        return "output buffer too small";
    default:
        return "unknown error";
    }
}

int ac4dsi_mode_for_wait_frames(int wait_frames)
{
    if (wait_frames == 0)
        return 1;
    if (wait_frames >= 1 && wait_frames <= 6)
        return 2;
    return 3;
}

size_t ac4dsi_v1_taglen(const char *s)
{
    size_t n = 0;
    if (!s)
        return 0;
    while (s[n] != '\0')
        n++;
    return n;
}

/* Validate one v1 presentation's parameters. Shared with the PutBitContext
 * emitter (ac4dsi_priv.h) so both agree on legality. */
int ac4dsi_v1_check(const ac4dsi_pres_t *p)
{
    size_t taglen;
    if (p->md_compat > 7u)
        return AC4DSI_EINVAL;
    if (p->b_presentation_id && p->presentation_id > 31u)
        return AC4DSI_EINVAL;
    if (p->ch_mode > 31u)
        return AC4DSI_EINVAL;
    /* 11-14 need back/top modifier bits; no wire vectors pin them. */
    if (p->ch_mode >= 11u && p->ch_mode <= 14u)
        return AC4DSI_EINVAL;
    if (p->channel_groups > 0x3FFFFu)
        return AC4DSI_EINVAL;
    if (p->content_classifier > 7u)
        return AC4DSI_EINVAL;
    taglen = ac4dsi_v1_taglen(p->language_tag);
    if (taglen > AC4DSI_MAX_LANG)
        return AC4DSI_EINVAL;
    if ((p->de_indicator & ~1) != 0 || (p->immersive_indicator & ~1) != 0)
        return AC4DSI_EINVAL;
    if ((p->b_presentation_id & ~1) != 0 || (p->b_content_type & ~1) != 0)
        return AC4DSI_EINVAL;
    return AC4DSI_OK;
}

/* Write one synthesized v1 block (no pv/pb framing). w must have room. */
static void v1_write(bitwriter_t *w, const ac4dsi_pres_t *p)
{
    size_t taglen = ac4dsi_v1_taglen(p->language_tag);
    size_t i;
    bw_put(w, AC4DSI_V1_CFG_SINGLE_GROUP, 5);
    bw_put(w, p->md_compat, 3);
    bw_put(w, (uint32_t)(p->b_presentation_id != 0), 1);
    if (p->b_presentation_id)
        bw_put(w, p->presentation_id, 5);
    bw_put(w, 0, 2); /* dsi_frame_rate_multiply_info */
    bw_put(w, 0, 2); /* dsi_frame_rate_fraction_info */
    bw_put(w, 0, 5); /* presentation_emdf_version */
    bw_put(w, 0, 10); /* presentation_key_id */
    bw_put(w, 1, 1); /* b_presentation_channel_coded */
    bw_put(w, p->ch_mode, 5);
    bw_put(w, 0, 6); /* reserved */
    bw_put(w, p->channel_groups, 18);
    bw_put(w, 0, 1); /* b_presentation_core_differs */
    bw_put(w, 0, 1); /* b_presentation_filter */
    /* single substream group (cfg 0x1F): */
    bw_put(w, 1, 1); /* b_substreams_present */
    bw_put(w, 0, 1); /* b_hsf_ext */
    bw_put(w, 1, 1); /* b_channel_coded */
    bw_put(w, 1, 8); /* n_substreams */
    bw_put(w, 0, 2); /* dsi_sf_multiplier */
    bw_put(w, 0, 1); /* no substream_bitrate_indicator */
    bw_put(w, 0, 6); /* reserved */
    bw_put(w, p->channel_groups, 18); /* mask mirrors presentation mask */
    bw_put(w, (uint32_t)(p->b_content_type != 0), 1);
    if (p->b_content_type) {
        bw_put(w, p->content_classifier, 3);
        bw_put(w, taglen != 0, 1);
        if (taglen != 0) {
            bw_put(w, (uint32_t)taglen, 6);
            for (i = 0; i < taglen; i++)
                bw_put(w, (uint8_t)p->language_tag[i], 8);
        }
    }
    bw_put(w, 0, 1); /* b_pre_virtualized */
    bw_put(w, 0, 1); /* b_add_emdf_substreams */
    bw_put(w, 0, 1); /* b_presentation_bitrate_info */
    bw_put(w, 0, 1); /* b_alternative */
    bw_align(w);
    /* DE/immersive tail (space always remains: block is byte-exact). */
    bw_put(w, (uint32_t)(p->de_indicator != 0), 1);
    bw_put(w, (uint32_t)(p->immersive_indicator != 0), 1);
    bw_put(w, 0, 4); /* reserved */
    bw_put(w, 0, 1); /* no extended presentation id */
    bw_put(w, 0, 1); /* pad */
}

int ac4dsi_stream_check(const ac4dsi_stream_t *s, size_t n_pres)
{
    if (n_pres < 1u || n_pres > AC4DSI_MAX_PRES)
        return AC4DSI_EINVAL;
    if (s->bitstream_version > 127u)
        return AC4DSI_EINVAL;
    if (s->fs_index > 1u || s->frame_rate_index > 15u)
        return AC4DSI_EINVAL;
    /* Table E.8 defines modes 1-3 only (DSI-15). */
    if (s->bit_rate_mode == 0u || s->bit_rate_mode > 3u)
        return AC4DSI_EINVAL;
    /* Mode 2 (ABR) without a target rate is the init-default footgun:
     * reject so a caller that forgot s.bit_rate fails loudly instead of
     * emitting mode-2/rate-0 (DSI-7). Mode 3 (VBR) with rate 0 stays
     * legal: wild vectors carry unknown rate. */
    if (s->bit_rate_mode == 2u && s->bit_rate == 0u)
        return AC4DSI_EINVAL;
    if ((s->b_program_id & ~1) != 0 || (s->b_uuid & ~1) != 0)
        return AC4DSI_EINVAL;
    /* Program-id block exists on the wire only when bsv > 1. */
    if (s->bitstream_version <= 1u && (s->b_program_id || s->b_uuid))
        return AC4DSI_EINVAL;
    if (!s->b_program_id && s->b_uuid)
        return AC4DSI_EINVAL;
    return AC4DSI_OK;
}

int ac4dsi_serialize(const ac4dsi_stream_t *s, const ac4dsi_pres_t *pres,
                     size_t n_pres, uint8_t *out, size_t out_cap,
                     size_t *out_len)
{
    bitwriter_t w;
    uint8_t v1tmp[AC4DSI_V1_TMP_SIZE];
    size_t i, need = 0;
    int rc;

    if (!s || !pres || !out)
        return AC4DSI_EINVAL;
    rc = ac4dsi_stream_check(s, n_pres);
    if (rc != AC4DSI_OK)
        return rc;
    for (i = 0; i < n_pres; i++) {
        const ac4dsi_pres_t *p = &pres[i];
        if (p->presentation_version == 0u)
            return AC4DSI_EINVAL; /* v0 legacy: do not emit */
        if (p->presentation_version == 1u) {
            rc = ac4dsi_v1_check(p);
            if (rc != AC4DSI_OK)
                return rc;
        } else {
            if (p->opaque_len > AC4DSI_MAX_OPAQUE)
                return AC4DSI_EINVAL;
            if (p->opaque_len != 0u && p->opaque == NULL)
                return AC4DSI_EINVAL;
        }
    }

    memset(out, 0, out_cap);
    w.buf = out;
    w.cap = out_cap;
    w.nbits = 0;
    w.over = 0;

    /* Header. */
    bw_put(&w, AC4DSI_DSI_VERSION, 3);
    bw_put(&w, s->bitstream_version, 7);
    bw_put(&w, s->fs_index, 1);
    bw_put(&w, s->frame_rate_index, 4);
    bw_put(&w, (uint32_t)n_pres, 9);
    if (s->bitstream_version > 1u) {
        bw_put(&w, (uint32_t)(s->b_program_id != 0), 1);
        if (s->b_program_id) {
            bw_put(&w, s->short_program_id, 16);
            bw_put(&w, (uint32_t)(s->b_uuid != 0), 1);
            if (s->b_uuid)
                bw_bytes(&w, s->program_uuid, 16);
        }
    }
    bw_put(&w, s->bit_rate_mode, 2);
    bw_put(&w, s->bit_rate, 32);
    bw_put(&w, s->bit_rate_precision, 32);
    bw_align(&w);

    /* Presentations. */
    for (i = 0; i < n_pres; i++) {
        const ac4dsi_pres_t *p = &pres[i];
        if (p->presentation_version == 1u) {
            bitwriter_t v;
            size_t pb;
            memset(v1tmp, 0, sizeof(v1tmp));
            v.buf = v1tmp;
            v.cap = sizeof(v1tmp);
            v.nbits = 0;
            v.over = 0;
            v1_write(&v, p);
            if (v.over || (v.nbits & 7u) != 0u)
                return AC4DSI_EINVAL; /* defensive: ac4dsi_v1_check bounds the
                                       * shape to <= 79 B, so v1tmp (80 B)
                                       * cannot overflow; kept in case the
                                       * synthesis shape ever grows */
            pb = v.nbits >> 3;
            bw_put(&w, 1, 8);
            if (pb < 255u) {
                bw_put(&w, (uint32_t)pb, 8);
            } else {
                /* Dead today: pb <= 79 < 255 by the bound above. Kept
                 * symmetric with the opaque path (where the escape is
                 * live) so a future larger v1 shape stays correct. */
                bw_put(&w, 255, 8);
                bw_put(&w, (uint32_t)(pb - 255u), 16);
            }
            bw_bytes(&w, v1tmp, pb);
        } else {
            bw_put(&w, p->presentation_version, 8);
            if (p->opaque_len < 255u) {
                bw_put(&w, (uint32_t)p->opaque_len, 8);
            } else {
                bw_put(&w, 255, 8);
                bw_put(&w, (uint32_t)(p->opaque_len - 255u), 16);
            }
            if (p->opaque_len != 0u)
                bw_bytes(&w, p->opaque, p->opaque_len);
        }
    }

    need = (w.nbits + 7u) >> 3;
    if (out_len)
        *out_len = need;
    if (w.over)
        return AC4DSI_ENOSPC;
    return AC4DSI_OK;
}
