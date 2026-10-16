/* enc_core.h: encoder core — frame assembly, rate loops, TOC.
 *
 * C port of encoder/enc.py + encoder/long51.py + encoder/fit51.py
 * (C-port phase, enc_core lane). enc.py maps to src/enc_core_enc.c,
 * long51.py to src/enc_core_long51.c, fit51.py to
 * src/enc_core_fit51.c; all share this header.
 *
 * Reuses the ported deps (bits/asf/mdct/loudness/enc_mc3/enc_jshort/
 * enc_tail59/enc_acpl/pycompat/sfb_offsets); nothing is re-ported.
 * Float discipline: builtin sum() of floats is pycompat_sum
 * (Neumaier), round() is pycompat_round_half_even, x**y is libm
 * pow() (kept a real call via -fno-builtin-pow: GCC would fold
 * pow(2.0,x)->exp2 etc., breaking bit-parity with CPython).
 * The _f entry points take float (post-fold) PCM doubles and are
 * bit-exact vs Python on fractional inputs (same op order incl
 * Neumaier nesting); the int-only entries are unchanged.
 *
 * Python dicts become fixed-size structs (coeffs sorted ascending;
 * refs_by_band carries -1 for absent keys). Python None returns on
 * the Optional-bytes entry points (encode, fit, assemble, fit_frame)
 * share the -1 channel with assert-class failures (never abort;
 * writer/buffers untouched on -1).
 */
#ifndef AC4_ENC_CORE_H
#define AC4_ENC_CORE_H

#include <stddef.h>
#include <stdint.h>

#include "bits.h"
#include "enc_mc3.h"

#define ENC_CORE_SYNC 0xAC41
#define ENC_CORE_FRAME_SAMPLES 2048
#define ENC_CORE_SAMPLE_RATE 48000
#define ENC_CORE_DIG_GAIN 3.0
#define ENC_CORE_X_EMIT_MAX 80000000.0
#define ENC_CORE_DEE_SUB0_P 6
#define ENC_CORE_DEE_SUB0_I 10
#define ENC_CORE_DEE_SUB2_I 4
#define ENC_CORE_MAX_SFB_FULL 63
#define ENC_CORE_MAX_SFB_LFE 7
#define ENC_CORE_TL_1024 3
#define ENC_CORE_TL_128 0
#define ENC_CORE_MSFB_MIX_1024 33
#define ENC_CORE_MSFB_MIX_128 9
#define ENC_CORE_MIX_GROUPING 0
#define ENC_CORE_DIALNORM_BITS 0
#define ENC_CORE_FLOOR_FRAC 1e-3
#define ENC_CORE_FLAT_F_THRESH 0.3
#define ENC_CORE_PURE_RATIO 1e-4
#define ENC_CORE_SHIFT_STEP 4
#define ENC_CORE_SHIFT_MAX 120
#define ENC_CORE_MIX_HF_LO 64
#define ENC_CORE_MIX_HF_RATIO 32.0
#define ENC_CORE_M_UNGATED_FLOOR -120.0
#define ENC_CORE_T_HIT_51_DB -17.2
#define ENC_CORE_LFE_HITS_BAKE_DB -8.8
#define ENC_CORE_TAIL16_DENSE 0xc400
#define ENC_CORE_TAIL16_GATED 0xc440
#define ENC_CORE_TAIL16_DIRAC 0xa140
#define ENC_CORE_TAIL16_IMPULSE 0xb040
#define ENC_CORE_N_MIX_SHORT 8
#define ENC_CORE_N_MIX_TILE128 16

/* Orient codes for the (3,0)/(0,3) mixed pair. */
#define ENC_CORE_ORIENT_LONG 0
#define ENC_CORE_ORIENT_1024_FIRST 1 /* (TL_1024, TL_128) */
#define ENC_CORE_ORIENT_128_FIRST 2 /* (TL_128, TL_1024) */

/* EMDF shapes (EMDF_PRESETS keys). */
#define ENC_CORE_EMDF_NONE 0
#define ENC_CORE_EMDF_DEE_I 1
#define ENC_CORE_EMDF_DEE_P 2
#define ENC_CORE_EMDF_I4 3
#define ENC_CORE_EMDF_P1 4

/* Stamped-substream template bytes (enc.py P_TAIL / I12 / SUB2 / I15). */
extern const uint8_t ENC_CORE_P_TAIL[5];
extern const uint8_t ENC_CORE_I12_MID[2];
extern const uint8_t ENC_CORE_I12_MID_TAIL[2];
extern const uint8_t ENC_CORE_I12_TAIL[6];
extern const uint8_t ENC_CORE_I11_TAIL[5];
extern const uint8_t ENC_CORE_SUB2_DEE[4];
extern const uint8_t ENC_CORE_I15_TAIL51[3];
extern const double ENC_CORE_LEG_COMP_51_DB[6];

/* One quantized LONG leg: sorted lines + per-band refs (-1 = no
 * key). msfb is informational (packers fill it; writers ignore). */
typedef struct {
    int lines[2048];
    int qs[2048];
    int n;
    int refs[63];
    int ref;
    int msfb;
} enc_core_leg_t;

/* One mixed-short group leg (1024 rows use lines[:1024]). */
typedef struct {
    int lines[1024];
    int qs[1024];
    int n;
    int refs[63];
} enc_core_mgroup_t;

typedef struct {
    enc_core_mgroup_t g[9];
} enc_core_mixed_leg_t;

/* DRC duration/curve payloads (write_drc_curve c dict). */
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
} enc_core_drc_curve_t;

#define ENC_CORE_DRC_DEFAULT 0
#define ENC_CORE_DRC_GAINS 1
#define ENC_CORE_DRC_CURVE 2

typedef struct {
    int mid;
    int kind;
    int gain;
    enc_core_drc_curve_t curve;
} enc_core_drc_mode_t;

typedef struct {
    int eac3;
    const enc_core_drc_mode_t *modes;
    int nmodes;
} enc_core_drc_t;

/* Stamped sub0/sub2 bytes. Content-frame assembly reads
 * (p6, i12, s4); _assemble_51 reads (p6, i15, s4); prim0 reads
 * (fr0, s4). NULL stamp = legacy zero sub0/sub2. */
typedef struct {
    const uint8_t *p6;
    int n_p6;
    const uint8_t *i12;
    int n_i12;
    const uint8_t *s4;
    int n_s4;
    const uint8_t *fr0;
    int n_fr0;
} enc_core_stamp_t;

/* ---- loudness stamping ---- */
int enc_core_content16(double m, int peak, double f, double target_db);
/* Float-peak twin (post-fold PCM; identical to content16 on
 * integer-valued peaks). */
int enc_core_content16_f(double m, double peak, double f,
                         double target_db);
int enc_core_tail16(int sparse, double tf);
/* mid23 NULL = default shape (I12_MID / tail callers pass their
 * own). Returns bytes (<= 12) or -1. */
int enc_core_compose_sub0(int iframe, int sparse, int stamp,
                          int content16, const uint8_t *mid23, int pid,
                          uint8_t *out);
int enc_core_compose_sub0_51(int stamp, int content16,
                             const uint8_t *mid23 /*NULL=I12_MID*/,
                             int pid, const int *dmx /*NULL or 5 ints*/,
                             uint8_t *out /*>= 15*/);
int enc_core_dmx_block17(int loro_c, int loro_s, int ltrt_c,
                         int ltrt_s, int preferred);
/* PRESET_ID lookup; -1 on unknown name. */
int enc_core_preset_id(const char *name);

/* ---- channel folds (out rows must hold the returned n; -1 NULL) ---- */
int enc_core_fold_8ch_to_51(const double *const *chs, const int *lens,
                            double **out);
int enc_core_fold_8ch_to_51_meter(const double *const *chs,
                                  const int *lens, double **out);
int enc_core_mix51_to_stereo(const double *const *chs, const int *lens,
                             double **out);
int enc_core_fold_8ch_to_stereo(const double *const *chs,
                                const int *lens, double **out);

/* ---- TOC / framing ---- */
int enc_core_nframes_for_samples(int nsamples);
int enc_core_seq_counter(int i);
int enc_core_b6hi_phase(int i);
/* interval NULL/"1sec" or decimal step (assert >= 2). out must
 * hold nframes; returns the ascending I-frame count or -1. */
int enc_core_iframe_set(int nframes, const char *interval,
                        const int *forced, int nforced, int *out);
int enc_core_joint_iframes(int nframes, int base, int total,
                           const char *interval, const int *forced,
                           int nforced, int *out);
int enc_core_cbr_raw_sizes(int nframes, int rate_kbps, int *out);
/* preset >= 0 selects EMDF_PRESETS; < 0 uses the explicit tuple.
 * 0 ok, -1 on bad preset or resv-length assert. */
int enc_core_emdf_info(bitwriter_t *w, int preset, int b_si, int si,
                       int len_pri, int len_sec, const uint8_t *resv,
                       int nresv);
int enc_core_write_toc(bitwriter_t *w, int i, int iframe,
                       const char *layout, int audio_si,
                       int content_type, int emdf,
                       const uint8_t *resv, int nresv);
int enc_core_finish_toc(bitwriter_t *w, const int *sizes, int nsizes);

/* ---- SIMPLE audio_data writers (bytes or -1) ---- */
int enc_core_write_audio_51(const enc_core_leg_t *lfe,
                            const enc_core_leg_t *l,
                            const enc_core_leg_t *r,
                            const enc_core_leg_t *ls,
                            const enc_core_leg_t *rs,
                            const enc_core_leg_t *c, uint8_t *out,
                            size_t outcap);
int enc_core_write_audio_51_mixed(const enc_core_leg_t *lfe,
                                  const enc_core_mixed_leg_t *l,
                                  int refL,
                                  const enc_core_mixed_leg_t *r,
                                  int refR,
                                  const enc_core_mixed_leg_t *ls,
                                  int refLs,
                                  const enc_core_mixed_leg_t *rs,
                                  int refRs,
                                  const enc_core_mixed_leg_t *c,
                                  int refC, int orient, uint8_t *out,
                                  size_t outcap);
int enc_core_write_audio_data(const char *layout, int iframe,
                              uint8_t *out, size_t outcap);
int enc_core_write_audio_spectral(const int *linesL, const int *qsL,
                                  int nL, int refL, const int *linesR,
                                  const int *qsR, int nR, int refR,
                                  uint8_t *out, size_t outcap);
int enc_core_write_audio_fullband(const enc_core_leg_t *l,
                                  const enc_core_leg_t *r, uint8_t *out,
                                  size_t outcap);
int enc_core_write_audio_joint_mixed(const enc_core_mixed_leg_t *m,
                                     int refM,
                                     const enc_core_mixed_leg_t *s,
                                     int refS, int orient, uint8_t *out,
                                     size_t outcap);
int enc_core_write_audio_mixed(const enc_core_mixed_leg_t *l, int refL,
                               const enc_core_mixed_leg_t *r, int refR,
                               int orient, uint8_t *out, size_t outcap);

/* ---- DRC / metadata / substreams (bytes or -1) ---- */
int enc_core_write_drc_curve(bitwriter_t *w,
                             const enc_core_drc_curve_t *c);
int enc_core_drc_curve_bits(const enc_core_drc_curve_t *c);
int enc_core_write_drc_frame(bitwriter_t *w, int eac3,
                             const enc_core_drc_mode_t *modes,
                             int nmodes, int iframe);
int enc_core_drc_frame_bits(const enc_core_drc_mode_t *modes,
                            int nmodes, int iframe);
int enc_core_write_metadata(const char *layout, int iframe,
                            const enc_core_drc_t *drc, uint8_t *out,
                            size_t outcap);
int enc_core_write_sub_audio(const uint8_t *audio, int n_audio,
                             const char *layout, int iframe,
                             const enc_core_drc_t *drc, const int *dmx,
                             uint8_t *out, size_t outcap);
int enc_core_write_substream(const char *layout, int iframe,
                             const enc_core_drc_t *drc, const int *dmx,
                             uint8_t *out, size_t outcap);
int enc_core_write_raw_frame(int i, int raw_size, const char *layout,
                             int iframe, const enc_core_drc_t *drc,
                             const int *dmx, uint8_t *out,
                             size_t outcap);
int enc_core_write_sync_frame(const uint8_t *raw, int n_raw,
                              const char *trailer, uint8_t *out,
                              size_t outcap);
int enc_core_encode_stream(int nsamples, int rate_kbps,
                           const char *layout, const char *interval,
                           const char *trailer, const int *forced,
                           int nforced, const enc_core_drc_t *drc,
                           const int *dmx, uint8_t *out, size_t outcap);

/* ---- quantization ---- */
double enc_core_gain_for_ref(int ref);
int enc_core_ref_for_peak(double peak, int qmax);
int enc_core_quantize_frame(const double *pcm, int n_pcm, int start,
                            const int *lines, int nlines,
                            const double *window /*NULL=default*/,
                            double gain, enc_core_leg_t *out);
int enc_core_quantize_full(const double *X, int n, double gain,
                           int ref_shift, int max_bands,
                           int floor_adaptive, enc_core_leg_t *out);
/* offs NULL = 2048 ladder; cap < 0 = None. */
int enc_core_quantize_bands(const double *X, int n, double gain,
                            int ref_shift, int max_bands,
                            const int *offs, int nsfb, int cap,
                            int floor_adaptive, enc_core_leg_t *out);
/* specs9: 9 row pointers (1024/128 lengths by orient slot);
 * gains NULL or 9 doubles out. */
int enc_core_quantize_mixed(const double *const *specs9, int orient,
                            double gain, int ref_shift, int max_bands,
                            enc_core_mixed_leg_t *out, double *gains);
int enc_core_tile_pure(const double *XL, const double *XR, int n,
                       const int *lines, int nlines);

/* ---- stereo rate loops (bytes, -1 None/error) ---- */
int enc_core_encode_fullband_frame(int i, const double *XL,
                                   const double *XR, int raw_size,
                                   const char *layout, int iframe,
                                   int ns2_content,
                                   const enc_core_stamp_t *stamp,
                                   const enc_core_drc_t *drc,
                                   const int *dmx, int floor_adaptive,
                                   uint8_t *out, size_t outcap);
int enc_core_assemble_content_frame(int i, const uint8_t *audio,
                                    int n_audio, int raw_size,
                                    const char *layout, int iframe,
                                    int ns2_content,
                                    const enc_core_stamp_t *stamp,
                                    const enc_core_drc_t *drc,
                                    const int *dmx, uint8_t *out,
                                    size_t outcap);
int enc_core_encode_mixed_frame(int i, const double *const *specsL9,
                                const double *const *specsR9, int orient,
                                int raw_size, const char *layout,
                                int iframe,
                                const enc_core_stamp_t *stamp,
                                const enc_core_drc_t *drc, const int *dmx,
                                uint8_t *out, size_t outcap);
int enc_core_encode_joint_mixed_frame(int i, const double *const *specsM9,
                                      const double *const *specsS9,
                                      int orient, int raw_size,
                                      const char *layout, int iframe,
                                      const enc_core_stamp_t *stamp,
                                      const enc_core_drc_t *drc,
                                      const int *dmx, uint8_t *out,
                                      size_t outcap);

/* ---- transient detector (1 mixed + o0/o1 set, 0 long) ---- */
int enc_core_tile_transient_orient(const double *specL16,
                                   const double *specR16, int start,
                                   int total /*<0 None*/, int *o0,
                                   int *o1);
int enc_core_tile_transient_orient_mc(const double *const *specs,
                                      int nch, int start,
                                      int total /*<0 None*/, int *o0,
                                      int *o1);
/* cache16: 16x128 flat; out rows hold 1024 (128-rows use [:128]).
 * 0 ok, -1 on bad orient. */
int enc_core_mixed_specs_for_frame(const double *pcm, int n_pcm,
                                   int start, int orient,
                                   const double *cache16, int edge_prev,
                                   int edge_next, double out[9][1024]);

/* ---- 5.1 assembly (bytes, -1 None/error) ---- */
int enc_core_assemble_51(int i, const uint8_t *audio, int n_audio,
                         int raw_size, int iframe,
                         const enc_core_stamp_t *stamp51,
                         int content_type, const enc_core_drc_t *drc,
                         const int *dmx, uint8_t *out, size_t outcap);

/* ---- full-stream drivers (bytes or -1) ---- */
int enc_core_encode_pcm(const int *pcmL, int nL, const int *pcmR,
                        int nR, int rate_kbps, const char *layout,
                        const char *interval, const char *trailer,
                        const double *window /*NULL or 4096*/,
                        int ns2_tone, int shorts, double target_db,
                        int has_bake, double bake_db, int joint_shorts,
                        const int *forced, int nforced,
                        const char *preset /*NULL=manual*/,
                        const enc_core_drc_t *drc, const int *dmx,
                        int priming, int iframe_base,
                        int iframe_total /*<0 None*/, uint8_t *out,
                        size_t outcap);
int enc_core_encode_pcm_mc(const int *const *chs, const int *lens,
                           int rate_kbps, const char *interval,
                           const char *trailer,
                           const double *window /*NULL or 4096*/,
                           double target_db, int has_bake,
                           double bake_db, int shorts,
                           const int *const *meter_chs /*NULL or 6*/,
                           const int *meter_lens, const int *forced,
                           int nforced, const char *preset /*NULL*/,
                           const enc_core_drc_t *drc, const int *dmx,
                           uint8_t *out, size_t outcap);
/* Float-PCM twins: same contract, double inputs (post-fold rows;
 * finite only — bake_round fails non-finite like int(round())).
 * Bit-exact vs enc.encode_pcm(_mc) on fractional inputs. */
int enc_core_encode_pcm_f(const double *pcmL, int nL,
                          const double *pcmR, int nR, int rate_kbps,
                          const char *layout, const char *interval,
                          const char *trailer,
                          const double *window /*NULL or 4096*/,
                          int ns2_tone, int shorts, double target_db,
                          int has_bake, double bake_db,
                          int joint_shorts, const int *forced,
                          int nforced, const char *preset /*NULL*/,
                          const enc_core_drc_t *drc, const int *dmx,
                          int priming, int iframe_base,
                          int iframe_total /*<0 None*/, uint8_t *out,
                          size_t outcap);
int enc_core_encode_pcm_mc_f(const double *const *chs,
                             const int *lens, int rate_kbps,
                             const char *interval, const char *trailer,
                             const double *window /*NULL or 4096*/,
                             double target_db, int has_bake,
                             double bake_db, int shorts,
                             const double *const *meter_chs /*NULL/6*/,
                             const int *meter_lens, const int *forced,
                             int nforced, const char *preset /*NULL*/,
                             const enc_core_drc_t *drc, const int *dmx,
                             uint8_t *out, size_t outcap);

/* ---- ac4enc-lib H1: caller-measured entries ----
 * Same contracts as the four drivers above, except the pass-1
 * loudness measure comes from the caller (ac4enc_measure[_f]) or
 * is absent (neutral). ms==NULL selects the NEUTRAL path: no
 * bake, no stamp, flat_file=0 — byte-divergent from the CLI by
 * design (see enc-lib/README.md). ms!=NULL with target_db==0.0
 * sets flat_file from f_flat (CLI target-0 parity). d_peak is
 * the float-path peak (content16_f); int paths use in_peak.
 * The _mc_m twins take no meter input: meter content is folded
 * into the caller measure (v1: lib passes audio rows only). */
typedef struct {
    double m_db;
    double f_flat;
    double tf_tone;
    int in_peak;
    double d_peak;
    int has_d_peak;
} enc_core_measure_t;
int enc_core_encode_pcm_m(const int *pcmL, int nL, const int *pcmR,
                          int nR, int rate_kbps, const char *layout,
                          const char *interval, const char *trailer,
                          const double *window, int ns2_tone,
                          int shorts, double target_db, int has_bake,
                          double bake_db, int joint_shorts,
                          const int *forced, int nforced,
                          const char *preset,
                          const enc_core_drc_t *drc, const int *dmx,
                          int priming, int iframe_base,
                          int iframe_total,
                          const enc_core_measure_t *ms, uint8_t *out,
                          size_t outcap);
int enc_core_encode_pcm_mc_m(const int *const *chs, const int *lens,
                             int rate_kbps, const char *interval,
                             const char *trailer,
                             const double *window, double target_db,
                             int has_bake, double bake_db, int shorts,
                             const int *forced, int nforced,
                             const char *preset,
                             const enc_core_drc_t *drc, const int *dmx,
                             const enc_core_measure_t *ms, uint8_t *out,
                             size_t outcap);
int enc_core_encode_pcm_f_m(const double *pcmL, int nL,
                            const double *pcmR, int nR, int rate_kbps,
                            const char *layout, const char *interval,
                            const char *trailer,
                            const double *window, int ns2_tone,
                            int shorts, double target_db, int has_bake,
                            double bake_db, int joint_shorts,
                            const int *forced, int nforced,
                            const char *preset,
                            const enc_core_drc_t *drc, const int *dmx,
                            int priming, int iframe_base,
                            int iframe_total,
                            const enc_core_measure_t *ms, uint8_t *out,
                            size_t outcap);
int enc_core_encode_pcm_mc_f_m(const double *const *chs,
                               const int *lens, int rate_kbps,
                               const char *interval, const char *trailer,
                               const double *window, double target_db,
                               int has_bake, double bake_db, int shorts,
                               const int *forced, int nforced,
                               const char *preset,
                               const enc_core_drc_t *drc, const int *dmx,
                               const enc_core_measure_t *ms, uint8_t *out,
                               size_t outcap);

/* ---- long51: steady-LONG 5.1 content packing ---- */
/* mc3-ready joint pack. joint.u.lon pointers alias the owned
 * arrays (do not copy this struct by value; valid while out
 * lives, like enc_mc3_quiet_t). */
typedef struct {
    enc_mc3_joint_t joint;
    int side[63];
    int nside;
    int msfb;
    int used[32];
    int npairs;
    int alphas[32];
    int nalphas;
    int deltas[32];
    int ndeltas;
    int bets[32];
    int nbets;
    int lines0[2048];
    int qs0[2048];
    int n0;
    int refs0[63];
    int lines1[2048];
    int qs1[2048];
    int n1;
    int refs1[63];
    int gmax[1];
    int used_arr[32];
    int deltas_arr[32];
    int leg_refs[2];
    enc_jshort_chparam_t cp;
    enc_jshort_long_leg_t lm;
    enc_jshort_long_leg_t ls;
} enc_core_joint_pack_t;

int enc_core_band_peaks(const double *X, int n, const int *offs,
                        int msfb, double *peaks);
int enc_core_pack_joint_long(const double *XA, int nA,
                             const double *XB, int nB, int has_gain,
                             double gain, int msfb_cap, double floor_,
                             int ref_shift, int kcap,
                             enc_core_joint_pack_t *out);
int enc_core_pack_mono(const double *XC, int nC, int has_gain,
                       double gain, int msfb_cap, int ref_shift,
                       int kcap, enc_core_leg_t *out);

/* Content-adaptive ACPL pair. a0/a1 wire pointers alias the
 * owned dfs arrays. */
typedef struct {
    enc_tail_acpl_t a0;
    enc_tail_acpl_t a1;
    int a0_adfs[14];
    int a0_bdfs[14];
    int a1_adfs[10];
    int a1_bdfs[10];
} enc_core_acpl_dual_t;

int enc_core_pack_acpl_dual(const double *pL, const double *pLs,
                            const double *pR, const double *pRs,
                            int npeaks, double floor_e, int interp,
                            enc_core_acpl_dual_t *out);

/* ---- fit51: mode-3 5.1 CBR rate loop ---- */
typedef struct {
    int kcap;
    int shift;
    int msfb[3];
    int bets[32];
    int nbets;
    int audio_len;
} enc_core_fit_info_t;

/* (body bytes, info) or -1 (Python None or assert-class). */
int enc_core_fit_frame_51(int i, const double *const *Xs,
                          const int *lens, int raw_size, int iframe,
                          const enc_acpl_books_t *abooks,
                          const enc_core_stamp_t *stamp51,
                          enc_core_fit_info_t *info, uint8_t *out,
                          size_t outcap);

#endif
