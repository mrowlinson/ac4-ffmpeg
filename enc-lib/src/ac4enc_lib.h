/* ac4enc_lib.h: opaque-ctx incremental API over the AC-4 core.
 *
 * Design source: ENC-LIBDESIGN.md sec 1. r2 is the incremental
 * ctx the design calls for: feed 2048-sample frames, pull bare
 * raw AC-4 frames (TOC..pad, NO sync/size/trailer) with 1-frame
 * lookahead (2 for priming=0 joint mode), drain the tail at
 * flush. Memory is bounded (window + scratch, ~1MB worst case);
 * open-ended streams are NOT bounded by RAM. No whole-stream
 * run exists anywhere in the lib path.
 *
 * Packet contract (normative): each emitted packet is exactly
 * the bytes `assemble_raw` returns for that frame index,
 * byte-identical to the corresponding raw payload inside CLI
 * .ac4 output when opts+measure (+spec lines) match, in
 * measured mode. Muxer adds sync+size (+CRC).
 *
 * H1 (loudness two-pass) is REAL, not buffered away: the
 * stamp+bake path is driven by the CALLER's pass-1 measure
 * (opts.has_measure via ac4enc_measure2[_f]). Without a measure
 * the lib uses the NEUTRAL path (no stamp, no bake,
 * flat_file=0, default spec lines) and documents the
 * byte-divergence (README.md). No silent default pretends CLI
 * parity. H6 (spec lines) likewise needs pass 1:
 * ac4enc_spectral_lines over the full input; measured stereo
 * without caller lines uses the neutral default (documented).
 *
 * Thread-safety (H7): RESOLVED. The ctx owns all state and the
 * engine keeps zero shared mutable state, so concurrent encodes
 * on SEPARATE ctx objects are safe. One ctx is still
 * single-threaded (no internal locking).
 *
 * Error model (H8): 0 ok / 1 frame ready / -EINVAL bad arg or
 * config or short buffer / -ENOMEM / -EIO encode refusal or
 * internal failure. No other negatives are returned.
 * ac4enc_strerror names the code; ac4enc_err_sub/_stage/_frame/
 * _detail report the sticky last-error detail (subcode + stage
 * + frame + gate/values text for bake refusals). Short-buffer
 * -EINVAL sets the required size through
 * the out_len param (2-variants + flush2) and consumes no
 * input; the call is retryable with room. After -EIO,
 * ac4enc_last_error() carries the core diagnostic when the
 * refusal recorded one (S1 loud failure path).
 *
 * Rate contract (M3): 48 kHz ONLY. opts.sample_rate accepts 0
 * (legacy default = 48000) or 48000; anything else fails open
 * with -EINVAL. Feeding other rates is not quietly mistreated:
 * it is refused up front.
 *
 * Lifecycle: one stream per ctx. encode frames, optional
 * encode_tail for a final partial 1..2048 samples, then flush
 * to drain. After a tail call or the first flush, only flush
 * (then close) may follow; ac4enc_reset reuses the ctx (same
 * opts) for a new stream. After an error (-EIO/-ENOMEM) the
 * ctx is sticky-failed: calls keep returning the error until
 * reset or close.
 *
 * This header is self-contained and exposes NO core internals
 * (M4): it does not include enc_core.h. In-tree core consumers
 * (tests, equivalence probes) include enc_core.h directly.
 */
#ifndef AC4ENC_LIB_H
#define AC4ENC_LIB_H

#include <stddef.h>
#include <stdint.h>

/* M5 compile-time guard: these modes silently break bit-parity
 * (constant folding, contraction, isinf removal). Downstream
 * builds must use the flags contract (README.md) instead. */
#if defined(__FAST_MATH__) || \
    (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ + 0)
#error "ac4enc: -ffast-math/-ffinite-math-only break bit-parity"
#endif

#ifdef __cplusplus
extern "C" {
#endif

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

/* Public DRC option mirror (layout-identical to the core
 * types; the lib converts at open, so the core header stays
 * private). Contents are validated at open (dry-run through
 * the real metadata writer): nmodes>=1 required when drc is
 * present, mid/kind/eac3/gain/curve ranges enforced. */
typedef struct {
    int nb_lo;
    int nb_hi;
    int maxboost;
    int lev_b;
    int nr_b;
    int g_b;
    int l_b;
    int maxcut;
    int lev_c;
    int nr_c;
    int g_c;
    int l_c;
    int tcdef;
    int tc[4];
    int has_thr;
    int thr0;
    int thr1;
} ac4enc_drc_curve_t;

#define AC4ENC_DRC_DEFAULT 0
#define AC4ENC_DRC_GAINS 1
#define AC4ENC_DRC_CURVE 2

typedef struct {
    int mid;
    int kind;
    int gain;
    ac4enc_drc_curve_t curve;
} ac4enc_drc_mode_t;

typedef struct {
    int eac3;
    const ac4enc_drc_mode_t *modes;
    int nmodes;
} ac4enc_drc_t;

/* All whole-stream options. Strings/arrays are copied at open. */
typedef struct {
    const char *layout; /* "stereo" | "5.1" (only) */
    int rate_kbps; /* DATA_RATES steps, layout floor */
    const char *interval; /* iframe rule: NULL/"1sec"/decimal>=2 */
    const int *forced;
    int nforced; /* forced iframe indices (CORE indices: the
                  * CLI's csvfile user index shifted +1) */
    const char *preset; /* NULL=manual stamp path */
    const double *window; /* NULL=KBD4096 default, else 4096 */
    int ns2_tone; /* 0/1 */
    int shorts; /* 0/1 */
    int joint_shorts; /* 0/1; stereo only */
    int priming; /* stereo: 1 priming (coff 0) | 0 joint (coff 2).
                  * 5.1: must be 1 (joint is stereo-only). */
    int iframe_base; /* joint mode global base (any int) */
    int iframe_total; /* joint mode: >=0 explicit total cap,
                       * <0 open-ended (exact for base==0) */
    double target_db; /* 0.0 or -31..-10 */
    int has_bake; /* 0/1; requires has_measure */
    double bake_db; /* explicit gain dB (finite, not NaN) */
    const ac4enc_drc_t *drc; /* NULL or validated modes */
    const int *dmx; /* NULL or 5 ints (range-free: bit-packed) */
    int sample_rate; /* 0 (=48000) or 48000; else -EINVAL */
    /* Pass-1 measurement (H1). All-or-nothing: */
    int has_measure;
    double m_db; /* not NaN */
    double f_flat; /* not NaN */
    double tf_tone; /* not NaN */
    int in_peak; /* >= 0 */
    double d_peak; /* >= 0, not NaN when has_d_peak */
    int has_d_peak;
    /* Pass-1 spec lines (H6, stereo only; mc ignores). Without
     * these, measured stereo uses the neutral default
     * (nlines=0: fullband-only LONG path), which is
     * byte-divergent from the CLI on tonal content. */
    int has_spec_lines; /* 0/1 */
    int n_spec_lines; /* 0..7 */
    int spec_lines[7]; /* each 0..2047 */
} ac4enc_opts_t;

/* Sticky last-error detail (post-open calls only; open reports
 * the rc alone). */
typedef enum {
    AC4ENC_EOK = 0,
    AC4ENC_EBADCALL, /* protocol misuse (domain mix, feed after
                      * tail/flush, emit without input...) */
    AC4ENC_EBADOPT, /* rejected option value (open-time) */
    AC4ENC_ENOMEM, /* allocation failure */
    AC4ENC_EBAKE_OVERFLOW, /* bake_round overflow (refusal) */
    AC4ENC_EBAKE_PEAK, /* baked peak >= 32767 at end (refusal) */
    AC4ENC_EENCODE, /* per-frame encode refusal / internal */
    AC4ENC_ESHORTBUF, /* outcap < frame (out_len = needed) */
    AC4ENC_EFPENV /* unsafe FP environment (open-time) */
} ac4enc_subcode_t;

/* 0 ok / -EINVAL bad opts / -ENOMEM / -EIO fpenv or core
 * refusal (e.g. uncomposable stamp: fail-fast at open).
 * Copies opts content; validates everything (interval rule,
 * DRC dry-run, measure/line ranges, rate). */
int ac4enc_open(ac4enc_ctx_t **ctx, const ac4enc_opts_t *opts);

/* Feed exactly 2048 samples per ch (ch-major, same scale as
 * enc_core_encode_pcm). Returns 1 frame ready (bare frame in
 * out) / 0 need-more-input / <0 error. If the call makes a
 * frame ready, out must be non-NULL with outcap >= the packet
 * size (see ac4enc_max_packet); otherwise -EINVAL WITHOUT
 * consuming input (retry with room). Non-emitting calls ignore
 * out/outcap. At most one frame per call; undrained ready
 * frames never accumulate past one. */
int ac4enc_encode_frame(ac4enc_ctx_t *c,
                        const int *const *pcm /* nch x 2048 */,
                        uint8_t *out, size_t outcap,
                        int *is_iframe /* NULL ok */);

/* Float twin: double rows, same contract as _f entries. */
int ac4enc_encode_frame_f(ac4enc_ctx_t *c,
                          const double *const *pcm,
                          uint8_t *out, size_t outcap,
                          int *is_iframe);

/* Length-reporting twins (preferred): *out_len always set on
 * 1, and to the needed size on short-buffer -EINVAL. */
int ac4enc_encode_frame2(ac4enc_ctx_t *c,
                         const int *const *pcm,
                         uint8_t *out, size_t outcap,
                         size_t *out_len /* required */,
                         int *is_iframe);
int ac4enc_encode_frame_f2(ac4enc_ctx_t *c,
                           const double *const *pcm,
                           uint8_t *out, size_t outcap,
                           size_t *out_len /* required */,
                           int *is_iframe);

/* Tail call: feed the final 0 < n_valid <= 2048 samples per
 * ch. Ends input (runs the baked-peak check); after a tail
 * call only flush may follow. Same 1/0/<0 contract. */
int ac4enc_encode_tail(ac4enc_ctx_t *c,
                       const int *const *pcm /* nch x n_valid */,
                       int n_valid, uint8_t *out,
                       size_t outcap, int *is_iframe);
int ac4enc_encode_tail_f(ac4enc_ctx_t *c,
                         const double *const *pcm,
                         int n_valid, uint8_t *out,
                         size_t outcap, int *is_iframe);
int ac4enc_encode_tail2(ac4enc_ctx_t *c,
                        const int *const *pcm,
                        int n_valid, uint8_t *out,
                        size_t outcap,
                        size_t *out_len /* required */,
                        int *is_iframe);
int ac4enc_encode_tail_f2(ac4enc_ctx_t *c,
                          const double *const *pcm,
                          int n_valid, uint8_t *out,
                          size_t outcap,
                          size_t *out_len /* required */,
                          int *is_iframe);

/* Drain encoded frames. 1 = bare frame in out (with
 * *out_len set), 0 = done, <0 error. The first call ends
 * input (no-feed streams encode the priming plan: 2 frames
 * at fed 0, m9) and runs the baked-peak check. Short-buffer
 * -EINVAL sets *out_len to the needed size. */
int ac4enc_flush2(ac4enc_ctx_t *c, uint8_t *out,
                  size_t outcap, size_t *out_len /* required */,
                  int *is_iframe);
void ac4enc_close(ac4enc_ctx_t *c); /* NULL-safe */

/* Reuse the ctx (same opts) for a new stream: clears fed
 * input, emitted frames, end/error state. 0 ok / -EINVAL NULL
 * / -ENOMEM reopen failure (ctx stays closed-able). */
int ac4enc_reset(ac4enc_ctx_t *c);

/* Name an rc (0/1/-EINVAL/-ENOMEM/-EIO, else "unknown"). */
const char *ac4enc_strerror(int rc);
/* Name a subcode (else "unknown"). */
const char *ac4enc_subcode_str(ac4enc_subcode_t sub);
/* Sticky last-error detail. Stage is one of "-"/"feed"/
 * "tail"/"flush"/"reset". Frame is the emit index or -1. */
ac4enc_subcode_t ac4enc_err_sub(const ac4enc_ctx_t *c);
const char *ac4enc_err_stage(const ac4enc_ctx_t *c);
int ac4enc_err_frame(const ac4enc_ctx_t *c);
/* R3 loud refusal: gate + offending values for the bake
 * subcodes ("baked peak gate: ...", "bake_round gate: ..."),
 * else "". Never NULL (NULL ctx -> ""). Follows err_sub. */
const char *ac4enc_err_detail(const ac4enc_ctx_t *c);

/* 64-bit stream totals (m2): samples fed per ch (all feeds)
 * and bare frames emitted. int64 never wraps in practice
 * (the old int total overflowed past ~12h at 48 kHz).
 * -1 on NULL. Reset clears both. */
int64_t ac4enc_fed_samples(const ac4enc_ctx_t *c);
int64_t ac4enc_emitted_frames(const ac4enc_ctx_t *c);

/* Max bare-frame bytes for a rate (exact CBR period max), or
 * -EINVAL on bad rate. Buffers of this size never go short. */
int ac4enc_max_packet(int rate_kbps);

/* 1 iff the FP environment is contraction-free (M5 runtime
 * self-check). ac4enc_open refuses (-EIO) when this is 0. */
int ac4enc_fp_ok(void);

/* Pass-1 helpers for H1: whole-input measure, feeds opts.
 * nch 2 = stereo ops, 6 = 5.1 ops (integrated_mc + mono6
 * flatness + 6ch peak scan, same op order as the core).
 * Inputs are 48 kHz by contract (M3). */
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

/* H6 pass-1: spec-lines estimate over the L row (baked when
 * target_db != 0, mirroring the core prologue). Stereo only
 * (mc ignores lines). Int callers convert L exactly to
 * double. Feeds opts.has_spec_lines/n_spec_lines/spec_lines.
 * -EIO if the bake itself overflows (the encode would refuse
 * too: consistent). */
int ac4enc_spectral_lines(const double *L, int nL,
                          const ac4enc_measure_t *m,
                          double target_db, int has_bake,
                          double bake_db, int *nlines,
                          int lines[7]);

/* Loud core diagnostic (S1): non-empty iff the most recent
 * core encode call refused with a recorded message. Read
 * immediately after -EIO. Never NULL. */
const char *ac4enc_last_error(void);

#ifdef __cplusplus
}
#endif

#endif