/* enc_step.h: private incremental-encode engine (r2).
 *
 * The engine is the single frame-stepping implementation behind
 * both the whole-stream core drivers (which feed all input, then
 * drain: thin wrappers proving zero-divergence) and the
 * incremental ac4enc_lib ctx (feed 2048-sample frames, emit bare
 * frames with 1-frame lookahead, drain at end).
 *
 * Private to the enc-lib build: NOT installed, NOT part of the
 * core's public API (enc_core.h is untouched by r2). All mutable
 * scratch is heap-owned by the ctx (B1: no function-static
 * mutable state anywhere in the tree); separate ctx objects
 * encode concurrently without sharing.
 *
 * Return codes mirror the lib: 0 ok / 1 frame ready (ready/emit
 * only) / -EINVAL config or protocol error / -ENOMEM /
 * -EIO encode refusal or internal failure. enc_step_detail
 * reports the sticky subcode (ESHORT is NOT sticky: the frame
 * stays pending and the call is retryable with room).
 */
#ifndef AC4ENC_STEP_H
#define AC4ENC_STEP_H

#include <stddef.h>
#include <stdint.h>

#include "enc_core.h"

typedef struct enc_step_ctx enc_step_ctx_t;

/* Static run config. Borrowed pointers must outlive the ctx
 * (the lib holds owned copies; the core wrappers pass prologue
 * locals that outlive the call). */
typedef struct {
    int nch; /* 2 stereo | 6 5.1 */
    int rate_kbps;
    const char *layout; /* nch=2: "stereo" (checked); else ignored */
    const char *interval; /* iframe rule: NULL/"1sec"/decimal (checked) */
    const double *window; /* borrowed 4096 taps or NULL=KBD4096 */
    int ns2_tone;
    int shorts;
    int joint_shorts; /* stereo only */
    const int *forced;
    int nforced;
    const enc_core_drc_t *drc; /* dry-run validated at open */
    const int *dmx; /* NULL or 5 ints; range-free by construction */
    int priming; /* stereo: 1 priming (coff 0) | 0 joint (coff 2) */
    int iframe_base; /* stereo joint only */
    int iframe_total; /* stereo joint: >=0 explicit cap, <0 open-ended.
                       * Whole-stream wrappers resolve <0 to the true
                       * nframes before open (exact). Open-ended is
                       * exact for iframe_base==0 (the cap never
                       * binds); base>0 + open-ended documents the
                       * edge (late-cadence marks past the unknown
                       * total are kept). */
    /* Prologue products (caller-computed; the engine applies them
     * in exact core op order). */
    double bake0[6]; /* per-ch factor; stereo: full bake. mc: bc. */
    double bake1[6]; /* mc leg-comp lin[c]; stereo ignored. */
    int bake_two; /* 0: v=x*bake0[c]. 1: v=(x*bake0[c])*bake1[c]. */
    int do_bake; /* apply bake_round on ingest */
    int do_peak; /* track baked peak; end() refuses >= 32767 */
    const enc_core_stamp_t *stamp; /* borrowed or NULL (neutral) */
    int flat_file;
    /* Stereo spectral path (H6): caller-measured lines. mc ignores. */
    int nlines; /* 0..7 */
    int lines[7]; /* each 0..2047 (checked) */
} enc_step_cfg_t;

/* Sticky failure detail (enc_step_detail). */
enum {
    ENC_STEP_OK = 0,
    ENC_STEP_EBCFG, /* bad static config at open */
    ENC_STEP_ENOMEM,
    ENC_STEP_EBAKE, /* bake_round overflow on ingest (refusal) */
    ENC_STEP_EPEAK, /* baked peak >= 32767 at end (refusal) */
    ENC_STEP_EEMIT, /* per-frame encode refusal / internal failure */
    ENC_STEP_EPROTO, /* feed after end, emit when not ready, ... */
    ENC_STEP_ESHORT /* outcap < frame bytes (*out_len = needed) */
};

int enc_step_open(enc_step_ctx_t **cp, const enc_step_cfg_t *cfg);
/* Feed n samples per ch (ns[c] each; rows defined as
 * (double)x for feed_int). ns!=NULL. */
int enc_step_feed(enc_step_ctx_t *c, const double *const *rows,
                  const int *ns);
int enc_step_feed_int(enc_step_ctx_t *c, const int *const *rows,
                      const int *ns);
/* Signal end of input: freezes the frame plan (nframes from fed
 * samples) and runs the baked-peak check. Strictly once. */
int enc_step_end(enc_step_ctx_t *c);
/* 1 = next frame ready (call emit), 0 = need more input (or
 * drained: end() + emitted == plan), <0 sticky failure. */
int enc_step_ready(enc_step_ctx_t *c);
/* Pure peek: 1 if a frame would be ready with n more fed
 * samples (lets callers size/check buffers before feeding).
 * 0 otherwise. Never fails (NULL -> 0). */
int enc_step_ready_after(const enc_step_ctx_t *c, int n);
/* Set the stamp before the first emit (the lib composes it
 * per-domain at first feed). Allowed iff emitted==0; EPROTO
 * after. Borrowed like cfg.stamp. */
int enc_step_set_stamp(enc_step_ctx_t *c,
                       const enc_core_stamp_t *stamp);
/* CBR byte size of the next emittable frame (pure peek; no
 * advance), or -1. Sized buffers never go short. */
int enc_step_next_size(enc_step_ctx_t *c);
/* Emit one bare frame (TOC..pad, no sync/size/trailer). 1 with
 * *out_len/is_iframe set, or <0 (ESHORT sets *out_len). */
int enc_step_emit(enc_step_ctx_t *c, uint8_t *out, size_t outcap,
                  size_t *out_len, int *is_iframe);
int enc_step_emitted(const enc_step_ctx_t *c);
int enc_step_detail(const enc_step_ctx_t *c);
/* R3 loud refusal: baked peak tracked on ingest (the EPEAK
 * offending value), or -1 on NULL. */
int enc_step_err_peak(const enc_step_ctx_t *c);
/* R3 loud refusal: EBAKE failing-sample locus (ch, per-ch fed
 * index, pre-bake value). 0 ok / -1 none (NULLs, or no EBAKE
 * recorded). */
int enc_step_err_sample(const enc_step_ctx_t *c, int *ch,
                        int *idx, double *val);
/* Closed-form max CBR raw size for a rate (period max), or -1
 * on bad rate. Backs ac4enc_max_packet. */
int enc_step_max_cbr(int rate_kbps);
void enc_step_close(enc_step_ctx_t *c);

#endif
