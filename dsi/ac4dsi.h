/**
 * @file ac4dsi.h
 * @brief Standalone AC-4 DSI (dac4 payload) serializer, pure C.
 *
 * Emits one ac4_dsi_v1 structure (ETSI TS 103 190-2 Annex E) exactly as it
 * must appear in the MP4 dac4 box payload: no box header, runs to end of box.
 * No dependencies, no allocation; the caller supplies the output buffer.
 *
 * Field order/widths follow DSI-CONTRACT.md (ff-dsi-contract), itself derived
 * from the published Annex E tables and verified against 3 corpus MP4s.
 *
 * Supported shape (all wire-observed stereo/5.1 vectors):
 *  - header: dsi_version=1, program-id block, stream bitrate block
 *  - per presentation: pv==1 synthesized v1 block with presentation_config
 *    0x1F (exactly one channel-coded substream group); pv>=2 opaque
 *    passthrough (bytes copied verbatim, never parsed).
 * Not emitted (rejected with -EINVAL): pv==0 legacy v0 blocks, ch_mode 11-14
 * back/top modifiers (no wire vectors to verify against).
 *
 * NOTE: no license header yet. This tree carries no LICENSE file; the
 * SPDX/copyright line is added at lavc-submit time (DSI-15 remainder).
 */
#ifndef AC4DSI_H
#define AC4DSI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes (negative returns from ac4dsi_serialize and helpers).
 * Numeric values match errno EINVAL/ENOSPC so strerror(-rc) also works,
 * but prefer ac4dsi_strerror() (stable across the sibling libs' mapping).
 * NOTE vs sibling enc-lib (ac4enc_lib.h H8: only -EINVAL/-ENOMEM/-EIO):
 * this module keeps a distinct -ENOSPC for short buffers so callers can
 * tell "retry with the reported size" apart from "bad parameters".
 * Mapping for shared plumbing: ENOSPC ~ EINVAL-with-size (like enc-lib's
 * short-buffer -EINVAL + required size); never -ENOMEM/-EIO here. */
#define AC4DSI_OK      0
#define AC4DSI_EINVAL (-22)  /* bad parameter / unsupported shape */
#define AC4DSI_ENOSPC (-28)  /* output buffer too small */

/**
 * Human-readable name for an AC4DSI_* return code ("ok",
 * "invalid argument / unsupported shape", "output buffer too small",
 * "unknown error"). Never returns NULL.
 */
const char *ac4dsi_strerror(int rc);

#define AC4DSI_DSI_VERSION 1
#define AC4DSI_MAX_OPAQUE  (255u + 65535u) /* pres_bytes 0xFF escape limit */
#define AC4DSI_MAX_LANG    63u              /* 6-bit language-tag length */
#define AC4DSI_MAX_PRES    511u             /* 9-bit n_presentations */

/* Stream-level DSI header fields. Obtain defaults from ac4dsi_stream_init(),
 * then override. Documented defaults below. */
typedef struct {
    uint8_t bitstream_version;   /* 7 bits. Default 2 (current encoder). */
    uint8_t fs_index;            /* 1 bit. Default 1 (48 kHz base). */
    uint8_t frame_rate_index;    /* 4 bits. Default 13 (encoder rate). */
    /* OPEN (a): program id. Default off (b_program_id=0), matching our
     * encoder TOC today. Wild vectors use on + short_program_id=300. */
    int      b_program_id;
    uint16_t short_program_id;   /* 16 bits, used iff b_program_id. */
    int      b_uuid;             /* used iff b_program_id. Default 0. */
    uint8_t  program_uuid[16];   /* used iff b_uuid. */
    /* Stream bitrate block (Table E.8 rule: wait_frames==0 -> mode 1,
     * 1..6 -> mode 2, else mode 3; see ac4dsi_mode_for_wait_frames).
     * Default: mode 2 (our encoder wait_frames=6), rate 0, precision
     * 0xFFFFFFFF (unknown). The rate 0 default is NOT emittable in mode 2:
     * serialize rejects mode==2 && rate==0 with -EINVAL, so a caller that
     * forgets to set the target bitrate fails loudly instead of emitting
     * a dubious mode-2/rate-0 header (DSI-7). Mode 3 (VBR) with rate 0
     * stays legal (wild vectors carry unknown rate). */
    uint8_t  bit_rate_mode;      /* 2 bits, 1..3 (Table E.8; 0 rejected). */
    uint32_t bit_rate;            /* Target bitrate; MUST be set for mode 2. */
    uint32_t bit_rate_precision;
} ac4dsi_stream_t;

/* One DSI presentation entry.
 *  - presentation_version == 1: v1_* fields are synthesized (cfg 0x1F,
 *    single channel-coded group). Obtain defaults from ac4dsi_pres_init_v1().
 *  - presentation_version >= 2: opaque bytes are copied verbatim under the
 *    pv/pres_bytes framing (opaque round-trip, never parsed).
 *  - presentation_version == 0: rejected (legacy v0, do not emit). */
typedef struct {
    uint8_t presentation_version;
    /* --- v1 synthesis fields (pv == 1 only) --- */
    uint8_t  md_compat;          /* 3 bits. Default 0 (stereo). */
    /* OPEN (b): presentation id. Default present, = 0 (our encoder TOC).
     * Wild vectors use 10. */
    int      b_presentation_id;  /* Default 1. */
    uint8_t  presentation_id;    /* 5 bits. Default 0. */
    uint8_t  ch_mode;            /* 5 bits. Default 1 (stereo); 4 = 5.1. */
    uint32_t channel_groups;     /* 18 bits. Default 0x000001 (L R). */
    /* OPEN (c): content tag. Default on, classifier 0, tag "en", matching
     * all wild vectors. Set b_content_type=0 for no content block, or
     * language_tag=NULL for classifier-only. */
    int         b_content_type;     /* Default 1. */
    uint8_t     content_classifier; /* 3 bits. Default 0. */
    /* Wire format is ISO 639-1 (2 letters, e.g. "en"), up to 63 bytes.
     * Default "en". NULL = classifier-only, no tag. lavc-side warning:
     * container/codec metadata usually carries ISO 639-2 ("eng") — the
     * caller MUST map 639-2 -> 639-1 before wiring this field; writing
     * "eng" raw emits a corrupt 3-letter tag, and truncating to "en"
     * without a table is wrong for codes whose 639-1 differs. */
    const char *language_tag;
    int de_indicator;               /* Default 1 (all vectors). */
    int immersive_indicator;        /* Default 0 (believer v1 entry: 1). */
    /* --- opaque passthrough (pv >= 2 only) --- */
    const uint8_t *opaque;
    size_t         opaque_len; /* bytes; 0 with NULL is legal (empty entry) */
} ac4dsi_pres_t;

/**
 * Fill *s with the documented stream defaults. NOTE: the defaults are not
 * all emittable — bit_rate stays 0 and serialize rejects mode==2 with a
 * zero rate (DSI-7), so set s.bit_rate before serializing.
 */
void ac4dsi_stream_init(ac4dsi_stream_t *s);

/**
 * Fill *p with the documented stereo v1 defaults (pv=1, md=0, pres_id=0,
 * ch_mode=1, mask 0x1, content on/"en", DE=1, imm=0).
 */
void ac4dsi_pres_init_v1(ac4dsi_pres_t *p);

/* DSI channel-layout ids as lavc channel_layout bitmasks
 * (libavutil/channel_layout.h). Values are pinned to the lavc
 * AV_CH_LAYOUT_* bits and cross-checked against the real headers by the
 * putbits suite (_Static_assert in dsi_putbits_test.c):
 *  - STEREO  = FL|FR (0x3)
 *  - 5POINT1 = FL|FR|FC|LFE|SL|SR (0x60f: the SIDE variant the lavc
 *    encoder wrapper accepts as AV_CHANNEL_LAYOUT_5POINT1).
 * The BACK variant (0x3f) is NOT accepted: the wrapper rejects it too,
 * so mapping it here would emit DSI the encoder can never produce. */
typedef enum ac4dsi_chmask {
    AC4DSI_CHMASK_STEREO  = 0x3,
    AC4DSI_CHMASK_5POINT1 = 0x60f
} ac4dsi_chmask_t;

/* One row of the lavc-mask -> DSI channel-trio mapping table. */
typedef struct {
    uint64_t lavc_mask;     /* AV_CH_LAYOUT_* bits (ac4dsi_chmask_t) */
    uint8_t  md_compat;
    uint8_t  ch_mode;
    uint32_t channel_groups;
} ac4dsi_layout_map_t;

#define AC4DSI_N_LAYOUTS 2
extern const ac4dsi_layout_map_t ac4dsi_layout_map[AC4DSI_N_LAYOUTS];

/**
 * Set the channel trio (md_compat, ch_mode, channel_groups) from a lavc
 * channel_layout bitmask (ac4dsi_chmask_t). This is the encoder-wiring
 * form (DSI-12): pass avctx->ch_layout.u.mask straight in.
 *
 * On success the trio is written; on failure *p is untouched.
 *
 * @return AC4DSI_OK, or AC4DSI_EINVAL on NULL pres / unknown mask.
 */
int ac4dsi_pres_set_layout_mask(ac4dsi_pres_t *p, uint64_t lavc_mask);

#if defined(__GNUC__) || defined(__clang__)
#define AC4DSI_DEPRECATED \
    __attribute__((deprecated("use ac4dsi_pres_set_layout_mask instead")))
#else
/* No deprecation spelling on this compiler; still legacy, see below. */
#define AC4DSI_DEPRECATED
#endif

/**
 * Set the channel trio (md_compat, ch_mode, channel_groups) from a layout
 * name: "stereo" (0/1/0x000001) or "5.1" (1/4/0x000047).
 *
 * LEGACY (DSI-12): kept for CLI/test convenience; new code (and the lavc
 * encoder wiring) uses ac4dsi_pres_set_layout_mask(). Implemented as a
 * thin shim over the mask table, so both spellings stay consistent.
 *
 * @return AC4DSI_OK, or AC4DSI_EINVAL on unknown layout.
 */
int ac4dsi_pres_set_layout(ac4dsi_pres_t *p, const char *layout)
    AC4DSI_DEPRECATED;

/**
 * Table E.8 wait_frames -> bit_rate_mode rule: 0 -> 1 (CBR), 1..6 -> 2
 * (ABR), else 3 (VBR). Negative inputs are nonsense but conform to the
 * "else" row, so they also yield 3 (documented, not clamped); callers
 * should not pass them (DSI-10).
 */
int ac4dsi_mode_for_wait_frames(int wait_frames);

/**
 * Serialize one ac4_dsi_v1 payload.
 *
 * @param s       stream header (must be non-NULL)
 * @param pres    array of n_pres presentations (n_pres >= 1; non-NULL)
 * @param n_pres  presentation count, 1..511
 * @param out     output buffer (non-NULL), out_cap its size in bytes
 * @param out_cap output buffer size in bytes
 * @param out_len receives the payload length in bytes (may be NULL)
 * @return AC4DSI_OK, AC4DSI_EINVAL (bad/unsupported field — including
 *         bit_rate_mode==0 and mode==2 with bit_rate==0), or AC4DSI_ENOSPC
 *         (out_cap too small; *out_len still set to needed size).
 *
 * Two-call sizing pattern: pass a small/empty buffer, then realloc to
 * *out_len and retry (validation precedes all writes, so ENOSPC is
 * purely about sizing — a retry with the reported size cannot fail).
 * Failure semantics: on EINVAL, out is untouched and *out_len (if any)
 * is left unchanged; on ENOSPC, out holds the zero-padded partial
 * payload (out_cap bytes, truncated) and *out_len is the full need.
 */
int ac4dsi_serialize(const ac4dsi_stream_t *s, const ac4dsi_pres_t *pres,
                     size_t n_pres, uint8_t *out, size_t out_cap,
                     size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* AC4DSI_H */
