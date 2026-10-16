/**
 * @file ac4dsi_putbits.c
 * @brief PutBitContext-based AC-4 DSI serializer implementation (DSI-11).
 *
 * Field order/widths mirror ac4dsi.c exactly; only the writer primitive
 * differs (put_bits/put_bits32 vs the hand-rolled loop). Validation is
 * shared (ac4dsi_priv.h), so the two emitters agree on legality by
 * construction; byte-identity (including short-buffer partials) is
 * pinned by dsi_putbits_test.c against the real lavc headers.
 *
 * Overflow accounting: lavc's put_bits() has no "count past the end"
 * mode (overflowing it hits an av_log + assert path), so this file
 * keeps its own honest room count (capbits - total, like the
 * hand-rolled nbits/cap pair) and never offers lavc more bits than
 * fit. A field that straddles the end emits its top bits that fit,
 * exactly like the bit-by-bit writer's prefix.
 */
#include "ac4dsi_putbits.h"
#include "ac4dsi_priv.h"

#include <limits.h>
#include <string.h>

#include "libavcodec/put_bits.h"

typedef struct {
    PutBitContext pb;
    size_t capbits; /* honest capacity: out_cap * 8, saturating */
    size_t total;   /* bits attempted (== hand-rolled nbits) */
    int over;       /* sticky overflow flag */
} pbw_t;

static void pbw_init(pbw_t *w, uint8_t *buf, size_t cap)
{
    int initsize;
    /* capbits is clamped to match lavc's int size (ac4dsi_priv.h), so the
     * honest room accounting below never offers lavc more bits than it
     * was sized for. need is always far below INT_MAX, so clamping a
     * gigantic cap is safe. */
    w->capbits = ac4dsi_capbits_for_cap(cap);
    initsize = cap > (size_t)INT_MAX ? INT_MAX : (int)cap;
    init_put_bits(&w->pb, buf, initsize);
    w->total = 0;
    w->over = 0;
}

/* Mirror of bw_put: big-endian, sticky overflow, keeps counting.
 * nbits is 1..32; 32-bit values go through put_bits32 (put_bits caps
 * at 31 bits per call). */
static void pbw_put(pbw_t *w, uint32_t val, int nbits)
{
    size_t room;
    if (nbits <= 0)
        return;
    if (w->over) {
        w->total += (size_t)nbits;
        return;
    }
    room = w->capbits > w->total ? w->capbits - w->total : 0u;
    w->total += (size_t)nbits;
    if ((size_t)nbits <= room) {
        if (nbits == 32)
            put_bits32(&w->pb, val);
        else
            put_bits(&w->pb, nbits, val);
        return;
    }
    /* Partial field: emit the top `room` bits that fit, then stick.
     * room < nbits <= 32, so room <= 31 and put_bits() is legal. */
    if (room > 0)
        put_bits(&w->pb, (int)room, val >> (nbits - (int)room));
    w->over = 1;
}

static void pbw_align(pbw_t *w)
{
    int pad = (int)((8u - (w->total & 7u)) & 7u);
    pbw_put(w, 0, pad);
}

static void pbw_bytes(pbw_t *w, const uint8_t *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        pbw_put(w, p[i], 8);
}

/* Write one synthesized v1 block (no pv/pb framing). Field-for-field
 * mirror of v1_write() in ac4dsi.c; keep the two in sync. */
static void pbw_v1_write(pbw_t *w, const ac4dsi_pres_t *p)
{
    size_t taglen = ac4dsi_v1_taglen(p->language_tag);
    size_t i;
    pbw_put(w, AC4DSI_V1_CFG_SINGLE_GROUP, 5);
    pbw_put(w, p->md_compat, 3);
    pbw_put(w, (uint32_t)(p->b_presentation_id != 0), 1);
    if (p->b_presentation_id)
        pbw_put(w, p->presentation_id, 5);
    pbw_put(w, 0, 2); /* dsi_frame_rate_multiply_info */
    pbw_put(w, 0, 2); /* dsi_frame_rate_fraction_info */
    pbw_put(w, 0, 5); /* presentation_emdf_version */
    pbw_put(w, 0, 10); /* presentation_key_id */
    pbw_put(w, 1, 1); /* b_presentation_channel_coded */
    pbw_put(w, p->ch_mode, 5);
    pbw_put(w, 0, 6); /* reserved */
    pbw_put(w, p->channel_groups, 18);
    pbw_put(w, 0, 1); /* b_presentation_core_differs */
    pbw_put(w, 0, 1); /* b_presentation_filter */
    /* single substream group (cfg 0x1F): */
    pbw_put(w, 1, 1); /* b_substreams_present */
    pbw_put(w, 0, 1); /* b_hsf_ext */
    pbw_put(w, 1, 1); /* b_channel_coded */
    pbw_put(w, 1, 8); /* n_substreams */
    pbw_put(w, 0, 2); /* dsi_sf_multiplier */
    pbw_put(w, 0, 1); /* no substream_bitrate_indicator */
    pbw_put(w, 0, 6); /* reserved */
    pbw_put(w, p->channel_groups, 18); /* mask mirrors presentation mask */
    pbw_put(w, (uint32_t)(p->b_content_type != 0), 1);
    if (p->b_content_type) {
        pbw_put(w, p->content_classifier, 3);
        pbw_put(w, taglen != 0, 1);
        if (taglen != 0) {
            pbw_put(w, (uint32_t)taglen, 6);
            for (i = 0; i < taglen; i++)
                pbw_put(w, (uint8_t)p->language_tag[i], 8);
        }
    }
    pbw_put(w, 0, 1); /* b_pre_virtualized */
    pbw_put(w, 0, 1); /* b_add_emdf_substreams */
    pbw_put(w, 0, 1); /* b_presentation_bitrate_info */
    pbw_put(w, 0, 1); /* b_alternative */
    pbw_align(w);
    /* DE/immersive tail (space always remains: block is byte-exact). */
    pbw_put(w, (uint32_t)(p->de_indicator != 0), 1);
    pbw_put(w, (uint32_t)(p->immersive_indicator != 0), 1);
    pbw_put(w, 0, 4); /* reserved */
    pbw_put(w, 0, 1); /* no extended presentation id */
    pbw_put(w, 0, 1); /* pad */
}

int ac4dsi_serialize_putbits(const ac4dsi_stream_t *s,
                             const ac4dsi_pres_t *pres, size_t n_pres,
                             uint8_t *out, size_t out_cap, size_t *out_len)
{
    pbw_t w;
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
    pbw_init(&w, out, out_cap);

    /* Header. */
    pbw_put(&w, AC4DSI_DSI_VERSION, 3);
    pbw_put(&w, s->bitstream_version, 7);
    pbw_put(&w, s->fs_index, 1);
    pbw_put(&w, s->frame_rate_index, 4);
    pbw_put(&w, (uint32_t)n_pres, 9);
    if (s->bitstream_version > 1u) {
        pbw_put(&w, (uint32_t)(s->b_program_id != 0), 1);
        if (s->b_program_id) {
            pbw_put(&w, s->short_program_id, 16);
            pbw_put(&w, (uint32_t)(s->b_uuid != 0), 1);
            if (s->b_uuid)
                pbw_bytes(&w, s->program_uuid, 16);
        }
    }
    pbw_put(&w, s->bit_rate_mode, 2);
    pbw_put(&w, s->bit_rate, 32);
    pbw_put(&w, s->bit_rate_precision, 32);
    pbw_align(&w);

    /* Presentations. */
    for (i = 0; i < n_pres; i++) {
        const ac4dsi_pres_t *p = &pres[i];
        if (p->presentation_version == 1u) {
            pbw_t v;
            size_t pb;
            memset(v1tmp, 0, sizeof(v1tmp));
            pbw_init(&v, v1tmp, sizeof(v1tmp));
            pbw_v1_write(&v, p);
            flush_put_bits(&v.pb);
            if (v.over || (v.total & 7u) != 0u)
                return AC4DSI_EINVAL; /* defensive: ac4dsi_v1_check bounds
                                       * the shape to <= 79 B, so v1tmp
                                       * (80 B) cannot overflow; kept in
                                       * case the synthesis shape grows */
            pb = (size_t)put_bits_count(&v.pb) >> 3;
            pbw_put(&w, 1, 8);
            if (pb < 255u) {
                pbw_put(&w, (uint32_t)pb, 8);
            } else {
                /* Dead today: pb <= 79 < 255 by the bound above. Kept
                 * symmetric with the opaque path (where the escape is
                 * live) so a future larger v1 shape stays correct. */
                pbw_put(&w, 255, 8);
                pbw_put(&w, (uint32_t)(pb - 255u), 16);
            }
            pbw_bytes(&w, v1tmp, pb);
        } else {
            pbw_put(&w, p->presentation_version, 8);
            if (p->opaque_len < 255u) {
                pbw_put(&w, (uint32_t)p->opaque_len, 8);
            } else {
                pbw_put(&w, 255, 8);
                pbw_put(&w, (uint32_t)(p->opaque_len - 255u), 16);
            }
            if (p->opaque_len != 0u)
                pbw_bytes(&w, p->opaque, p->opaque_len);
        }
    }

    need = (w.total + 7u) >> 3;
    if (out_len)
        *out_len = need;
    /* Push lavc's cached bits into out. Safe: the honest accounting
     * above never offered lavc more bits than fit. */
    flush_put_bits(&w.pb);
    if (w.over)
        return AC4DSI_ENOSPC;
    return AC4DSI_OK;
}
