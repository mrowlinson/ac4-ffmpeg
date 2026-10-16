/* ac4dsi.h - standalone AC-4 DSI (dac4 payload) serializer, pure C.
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
 */
#ifndef AC4DSI_H
#define AC4DSI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes (negative returns from ac4dsi_serialize and helpers). */
#define AC4DSI_OK      0
#define AC4DSI_EINVAL (-22)  /* bad parameter / unsupported shape */
#define AC4DSI_ENOSPC (-28)  /* output buffer too small */

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
     * Default: mode 2 (our encoder wait_frames=6), rate 0 (CALLER MUST
     * set the target bitrate), precision 0xFFFFFFFF (unknown). */
    uint8_t  bit_rate_mode;      /* 2 bits. */
    uint32_t bit_rate;
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
    const char *language_tag;       /* Default "en". NULL = no tag. */
    int de_indicator;               /* Default 1 (all vectors). */
    int immersive_indicator;        /* Default 0 (believer v1 entry: 1). */
    /* --- opaque passthrough (pv >= 2 only) --- */
    const uint8_t *opaque;
    size_t         opaque_len; /* bytes; 0 with NULL is legal (empty entry) */
} ac4dsi_pres_t;

/* Fill *s with the documented stream defaults. rate/precision/mode left for
 * the caller only where noted above (rate MUST be set). */
void ac4dsi_stream_init(ac4dsi_stream_t *s);

/* Fill *p with the documented stereo v1 defaults (pv=1, md=0, pres_id=0,
 * ch_mode=1, mask 0x1, content on/"en", DE=1, imm=0). */
void ac4dsi_pres_init_v1(ac4dsi_pres_t *p);

/* Set the channel trio (md_compat, ch_mode, channel_groups) from a layout
 * name: "stereo" (0/1/0x000001) or "5.1" (1/4/0x000047). Returns AC4DSI_OK
 * or AC4DSI_EINVAL on unknown layout. */
int ac4dsi_pres_set_layout(ac4dsi_pres_t *p, const char *layout);

/* Table E.8 wait_frames -> bit_rate_mode rule: 0 -> 1 (CBR), 1..6 -> 2
 * (ABR), else 3 (VBR). */
int ac4dsi_mode_for_wait_frames(int wait_frames);

/* Serialize one ac4_dsi_v1 payload.
 *  s      stream header (must be non-NULL)
 *  pres   array of n_pres presentations (n_pres >= 1; pres non-NULL)
 *  out    output buffer (non-NULL), out_cap its size in bytes
 *  out_len receives the payload length in bytes (may be NULL)
 * Returns AC4DSI_OK, AC4DSI_EINVAL (bad/unsupported field), or
 * AC4DSI_ENOSPC (out_cap too small; *out_len still set to needed size). */
int ac4dsi_serialize(const ac4dsi_stream_t *s, const ac4dsi_pres_t *pres,
                     size_t n_pres, uint8_t *out, size_t out_cap,
                     size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* AC4DSI_H */
