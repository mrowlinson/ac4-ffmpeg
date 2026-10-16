/* ac4d.h — streaming AC-4 decoder library API (opaque context).
 *
 * Refactored from the c-ac4dec batch decoder (single TU, all-static) into
 * a heap-instance streaming API for ffmpeg integration. Design:
 * DEC-LIBDESIGN.md (ff-dec-libdesign). Hazard H1 (streaming-first):
 * bounded per-frame step, no whole-stream malloc, no rewind; sticky
 * latches from the first fed frame; gap frames emit silence in place
 * (AC4D_GAP) instead of the CLI's rewind-and-stub fallback.
 *
 * Threading: one ac4d_ctx per stream; calls on the same ctx must be
 * serialized (the decode core keeps no thread-local state). Distinct
 * ctx instances are fully independent (all decode state lives in the
 * heap ctx, ~16 MB).
 */
#ifndef AC4D_H
#define AC4D_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes. >= 0: ok-class. < 0: hard errors (sticky: the ctx keeps
 * reporting the first hard error; the caller must close + reopen). */
#define AC4D_OK 0
#define AC4D_NEED_MORE 1  /* get_frame/flush: no frame ready now */
#define AC4D_GAP 2        /* silence emitted for this frame; note set */
#define AC4D_ERR (-1)     /* generic decode failure (see gap note) */
#define AC4D_NO_MEM (-2)
#define AC4D_BAD_PACKET (-3)      /* sync/CRC/length/shape reject */
#define AC4D_CONFIG_CHANGE (-4)   /* bed_cm/nch/spf shift after latch */
#define AC4D_BITSTREAM (-5)       /* strict-path bit error (stereo etc.) */
#define AC4D_UNSUPPORTED (-6)     /* DRC-on / DE-on (whole-stream legs) */

/* Open-time options (CLI flag mirrors). Zero-init; pointer may be NULL
 * for all defaults. Preselect matches --sel/--pres-index (-1 = default
 * presentation 0). sr_override/ch_override match --sr/--ch (0 = none);
 * sr_override relabels sample_rate only -- output counts stay
 * stream-determined. bits = 16/24/32f output width (as 16/24/32; 32
 * means float32). log wires the decode-core diagnostics (stderr in
 * the CLI); NULL = quiet (default). */
typedef struct {
    int pres_sel;
    int drc_on;
    int drc_out_ref;
    int drc_ref_given;
    int drc_boost_pct, drc_cut_pct;
    int de_given;
    double de_level;
    int sr_override;
    int ch_override;
    int bits;
    int ajoc_core;
    void (*log)(int lvl, const char *msg, void *u);
    void *log_u;
} ac4d_opts;

/* Stream info, valid after the first successful feed (latch).
 * sample_rate = OUTPUT (post-s5.11-converter) rate = base_fs x
 * sf_mult on every fri row (44100/48000 x 1/2/4), or the sr_override
 * when set. spf = per-frame OUTPUT samples/channel: get_info reports
 * the latched max (ceil over fractional rows); each get_frame/flush
 * fi reports that frame's actual count (Table-70 pattern on
 * fractional rows, constant elsewhere). bed_cm = TOC ch_mode.
 * ch_layout_mask uses ffmpeg AV_CH_* bit values (mono 0x4, stereo
 * 0x3, 5.1 0x3F, 7.1 0x63F). real = 1 when the latched class decodes
 * real PCM. */
typedef struct {
    int sample_rate, channels, bits, spf;
    int bed_cm;
    uint64_t ch_layout_mask;
    int real;
} ac4d_info;

typedef struct ac4d_ctx ac4d_ctx;

int ac4d_open(ac4d_ctx **cp, const ac4d_opts *o);
int ac4d_feed(ac4d_ctx *c, const uint8_t *pkt, size_t len);
/* ONE raw AC-4 sync frame (AC40/AC41) per feed. Container ingest stays
 * out (caller demuxes); AC41 CRC is checked inside. */
int ac4d_get_frame(ac4d_ctx *c, void *pcm, size_t cap, ac4d_info *fi);
/* Next ready frame: interleaved s16 (bits=16), 3-byte LE s24 (bits=24),
 * or float32 (bits=32) per the open bits option; channels*fi->spf
 * samples (fi->spf = this frame's actual output count, which varies
 * on fractional rows -- always read it back, never assume the latched
 * max). Returns AC4D_OK / AC4D_NEED_MORE / AC4D_GAP (silence written,
 * note set) / <0 hard error. pcm may be NULL with cap 0 to poll. fi
 * may be NULL. Required cap = channels*fi->spf*(bits/8) (24 -> x3);
 * size caller buffers with channels*get_info-spf (the max).
 * Drain contract (H1): call get_frame until AC4D_NEED_MORE after each
 * feed; the instance holds only a few frames, so unbounded undrained
 * feeds fail cleanly with AC4D_ERR (ring overrun), never grow. */
int ac4d_flush(ac4d_ctx *c, void *pcm, size_t cap, ac4d_info *fi);
/* End of stream: drain the pipeline (skeleton tail, post-pass tails,
 * compand tail, OLA ring). Call repeatedly until AC4D_NEED_MORE; each
 * OK/GAP return emits one trailing frame (same layout as get_frame;
 * fi carries that frame's actual count, NULL-ok). */
void ac4d_close(ac4d_ctx *c);
const char *ac4d_strerror(int rc);
const char *ac4d_gap_note(const ac4d_ctx *c); /* never NULL after GAP */
int ac4d_get_info(const ac4d_ctx *c, ac4d_info *i);
/* (design §1 names this ac4d_info; renamed: C forbids sharing the
 * name with the ac4d_info struct type) */

#ifdef __cplusplus
}
#endif

#endif
