/* ac4enc_lib.h: opaque-ctx streaming API over the AC-4 encoder core.
 *
 * Design source: ENC-LIBDESIGN.md sec 1. v1 adaptation: the ctx
 * BUFFERS input frames and runs the whole-stream core once at
 * flush (see "v1 buffering" below), so encode_frame always
 * returns 0 until flush. The packet contract is exact: each
 * emitted packet is the bare raw AC-4 frame (TOC..pad, NO
 * sync/size/trailer) for that frame index, byte-identical to
 * the corresponding raw payload inside CLI .ac4 output when
 * opts+measure match.
 *
 * v1 buffering (documented divergence from the design's
 * incremental vision): H2 (nframes planning), H3 (MDCT rings),
 * H4 (orient lookahead), H6 (freq estimate) are all satisfied
 * by holding the full stream: nframes is known at flush, the
 * core sees the exact whole input it was written for. Cost:
 * whole-stream latency and memory, and open-ended streams are
 * bounded by RAM. A future incremental ctx (rings + 1-frame
 * lookahead per the design) keeps this header unchanged.
 *
 * H1 (loudness two-pass) is REAL in v1, not buffered away: the
 * core never sees the input before flush, so the stamp+bake
 * path is driven by the CALLER's pass-1 measure (opts.has_measure
 * via ac4enc_measure2[_f]), through the _m core entries. Without
 * a measure the lib uses the NEUTRAL path (no stamp, no bake,
 * flat_file=0) and documents the byte-divergence (README.md).
 * There is no silent default that pretends CLI parity.
 *
 * Thread-safety (H7): v1 is NOT thread-safe. The ctx owns all
 * lib state, but the vendored core still keeps function-static
 * scratch (sL/sR tiles etc.), so concurrent encodes — even on
 * separate ctx — race. Single-threaded use matches the CLI
 * exactly. Killing the core statics is future work.
 *
 * Error model (H8): 0 ok / 1 frame ready / -EINVAL bad arg /
 * -ENOMEM allocation failure / -EIO core encode failure.
 * No other negatives are returned.
 */
#ifndef AC4ENC_LIB_H
#define AC4ENC_LIB_H

#include <stddef.h>
#include <stdint.h>

#include "enc_core.h"

#define AC4ENC_FRAME_SAMPLES 2048
#define AC4ENC_OUTCAP_SAFE 65536

typedef struct ac4enc_ctx ac4enc_ctx_t;

/* Full pass-1 measure. m_db/f_flat/tf_tone feed the bake gain
 * and stamp; in_peak feeds content16 (int path); d_peak feeds
 * content16_f (float path) when has_d_peak is set, else the
 * float path falls back to (double)in_peak. */
typedef struct {
    double m_db;
    double f_flat;
    double tf_tone;
    int in_peak;
    double d_peak;
    int has_d_peak;
} ac4enc_measure_t;

/* All whole-stream options. Strings/arrays are copied at open. */
typedef struct {
    const char *layout;     /* "stereo" | "5.1" (only) */
    int rate_kbps;          /* DATA_RATES steps, layout floor */
    const char *interval;   /* iframe rule, e.g. "1sec"; NULL ok */
    const int *forced;
    int nforced;            /* forced iframe indices */
    const char *preset;     /* NULL=manual stamp path */
    const double *window;   /* NULL=KBD4096 default, else 4096 */
    int ns2_tone;
    int shorts;
    int joint_shorts;
    int priming;
    int iframe_base;
    int iframe_total;       /* <0=None */
    double target_db;
    int has_bake;
    double bake_db;
    const enc_core_drc_t *drc;
    const int *dmx;         /* NULL or 5 ints */
    /* Pass-1 measurement (H1). All-or-nothing: */
    int has_measure;
    double m_db;
    double f_flat;
    double tf_tone;
    int in_peak;
    /* v1 float-path extension: float callers should set these
     * (ac4enc_measure2_f fills the full struct instead). */
    double d_peak;
    int has_d_peak;
} ac4enc_opts_t;

/* 1 frame ready / 0 need-more-input (v1: always 0 until flush) /
 * <0 error. See header notes. */
int ac4enc_open(ac4enc_ctx_t **ctx, const ac4enc_opts_t *opts);

/* Feed exactly 2048 int samples per ch per call (ch-major, same
 * scale as enc_core_encode_pcm). v1: buffers, returns 0. */
int ac4enc_encode_frame(ac4enc_ctx_t *c,
                        const int *const *pcm /* nch x 2048 */,
                        uint8_t *out, size_t outcap,
                        int *is_iframe /* NULL ok */);

/* Float twin: double rows, same contract as _f entries. */
int ac4enc_encode_frame_f(ac4enc_ctx_t *c,
                          const double *const *pcm,
                          uint8_t *out, size_t outcap,
                          int *is_iframe);

/* v1 tail API (design gap: streams are rarely a multiple of
 * 2048): feed the final 0 < n_valid <= 2048 samples per ch.
 * After a tail call only flush may follow. */
int ac4enc_encode_tail(ac4enc_ctx_t *c,
                       const int *const *pcm /* nch x n_valid */,
                       int n_valid, uint8_t *out, size_t outcap,
                       int *is_iframe);
int ac4enc_encode_tail_f(ac4enc_ctx_t *c,
                         const double *const *pcm,
                         int n_valid, uint8_t *out, size_t outcap,
                         int *is_iframe);

/* Drain encoded frames. 1 = bare frame in out, 0 = done,
 * <0 error. First call runs the core encode. */
int ac4enc_flush(ac4enc_ctx_t *c, uint8_t *out, size_t outcap,
                 int *is_iframe);
/* v1 extension (design gap: flush never reports the frame
 * length, so callers cannot size packets). Same as flush but
 * stores the bare-frame byte count in *out_len (required).
 * Prefer this over ac4enc_flush. */
int ac4enc_flush2(ac4enc_ctx_t *c, uint8_t *out, size_t outcap,
                  size_t *out_len, int *is_iframe);
void ac4enc_close(ac4enc_ctx_t *c); /* NULL-safe */

/* Pass-1 helpers for H1: whole-input measure, feeds opts.
 * nch 2 = stereo ops, 6 = 5.1 ops (integrated_mc + mono6
 * flatness + 6ch peak scan, same op order as the core). */
int ac4enc_measure(const int *const *chs, const int *lens, int nch,
                   double *m_db, double *f_flat, double *tf_tone,
                   int *in_peak);
int ac4enc_measure_f(const double *const *chs, const int *lens,
                     int nch, double *m_db, double *f_flat,
                     double *tf_tone, int *in_peak);
/* Full-struct twins (preferred; carry d_peak for the float
 * path without loss). */
int ac4enc_measure2(const int *const *chs, const int *lens,
                    int nch, ac4enc_measure_t *out);
int ac4enc_measure2_f(const double *const *chs, const int *lens,
                      int nch, ac4enc_measure_t *out);

#endif
