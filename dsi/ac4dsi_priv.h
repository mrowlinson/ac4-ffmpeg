/*
 * AC-4 DSI shared emitter internals
 * Copyright (c) 2026 Michael Rowlinson
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file ac4dsi_priv.h
 * @brief Shared internals for the two DSI emitters (DSI-11).
 *
 * The hand-rolled emitter (ac4dsi.c) and the PutBitContext emitter
 * (ac4dsi_putbits.c) share validation and wire constants through this
 * header so the two can never disagree on what is legal or on buffer
 * bounds. Only the bit Emission differs, and the putbits suite pins
 * the two byte-identical.
 *
 * Private: not installed, not part of the public API. The helpers below
 * are header-static (internal linkage per TU), so no ac4dsi_ globals leak
 * into the future lavc link (MINOR-2).
 */
#ifndef AC4DSI_PRIV_H
#define AC4DSI_PRIV_H

#include <limits.h>

#include "ac4dsi.h"

/* Fixed v1 synthesis constants for the observed stereo/5.1 shape. */
#define AC4DSI_V1_CFG_SINGLE_GROUP 0x1Fu
/* Worst case: 114 fixed bits + 63 tag bytes + align + 8 tail = 632 bits
 * = 79 B, so 80 leaves 1 B margin. (Each tag byte adds a full byte.) */
#define AC4DSI_V1_TMP_SIZE 80u

/**
 * strlen that tolerates NULL (NULL -> 0). Never reads past NUL.
 */
static inline size_t ac4dsi_v1_taglen(const char *s)
{
    size_t n = 0;
    if (!s)
        return 0;
    while (s[n] != '\0')
        n++;
    return n;
}

/**
 * Validate the stream header + presentation count. AC4DSI_OK or EINVAL.
 */
static inline int ac4dsi_stream_check(const ac4dsi_stream_t *s, size_t n_pres)
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

/**
 * Validate one v1 presentation's synthesis parameters. AC4DSI_OK/EINVAL.
 */
static inline int ac4dsi_v1_check(const ac4dsi_pres_t *p)
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

/**
 * Honest bit capacity for a byte cap (putbits room accounting).
 *
 * lavc's init_put_bits takes an int size, so a cap above INT_MAX is
 * clamped for lavc; capbits is clamped to match so the accounting never
 * offers lavc more bits than it was sized for (MINOR-1). Unreachable in
 * practice (need is always far below INT_MAX), pinned by unit test anyway.
 */
static inline size_t ac4dsi_capbits_for_cap(size_t cap)
{
    size_t capbits = cap > SIZE_MAX / 8u ? SIZE_MAX : cap * 8u;
    size_t maxbits = (size_t)INT_MAX * 8u;
    return capbits > maxbits ? maxbits : capbits;
}

#endif /* AC4DSI_PRIV_H */
