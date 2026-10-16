/* ac4_stages.h — decode-stage interfaces (e3 integrator owned).
 *
 * Wave05 stage-1 split: e0 = Huffman tables, e1 = entropy/section
 * decode, e2 = IMDCT+OLA. Each stage lands its own .c + header on its
 * lane; this file is the INTEGRATION contract they plug into. Until a
 * stage lands, the AC4_HAVE_* macros below stay 0 and the static
 * stubs keep the decode loop compiling; every real frame then maps
 * to an honest CLI exit 3 (battery SKIP, never FAIL).
 *
 * Wave05-end state (verified first-hand in sibling worktrees):
 * - e0: src/huffman_tables.c 995 lines + .h, selftest ALL PASS
 *   (49 books, Kraft exact, C==PY, 2980 entries). Merge blocked on
 *   provenance ruling (QUESTION-e0-1, parent).
 * - e1: DONE Python (decoder/asf_section.py 380 lines, 10/10 tests:
 *   strict section headers, unmix 11 books, escape, gain, zero-runs
 *   exact, HuffmanReader protocol). C port BLOCKED on Q-e1-1
 *   (sf_info wire widths, with m5).
 * - e2: src/imdct.c 199 lines double-precision core, uniform
 *   roundtrips 4/4 PASS err~1e-13, switch seqs FAIL (open
 *   QUESTION-e2-1 bridge rule). e3 dispatches long-only.
 *
 * Contracts:
 * - DRC default OFF on the bit-compare path (spec/drc-default-off).
 * - e2 owns numerics: double core, s16 = lround(v*32767)+clip.
 * - Overlap state zero-init (e2 assumption, QUESTION-e2-3).
 * - Long-block only until block-switch bits settle.
 */
#ifndef AC4_STAGES_H
#define AC4_STAGES_H

#include <stddef.h>
#include <stdint.h>

/* ---- feature gates: stage lanes flip these to 1 on landing ---- */
#ifndef AC4_HAVE_HUFFMAN /* e0 tables */
#define AC4_HAVE_HUFFMAN 0
#endif
#ifndef AC4_HAVE_ENTROPY /* e1-C section decode */
#define AC4_HAVE_ENTROPY 0
#endif
#ifndef AC4_HAVE_IMDCT /* e2 IMDCT + KBD + OLA */
#define AC4_HAVE_IMDCT 0
#endif

/* ---- error codes (negative; 0 = ok) ---- */
#define AC4_EUNIMPL (-101)  /* stage or shape not implemented this wave */
#define AC4_EBITSTREAM (-102) /* bit overrun / illegal code inside a stage */
#define AC4_ESSF (-103)     /* SSF frontend (S1=1): 0 corpus hits, later wave */
#define AC4_EASPX (-104)    /* A-SPX/A-CPL codec mode: stage-2, not stage-1 */
#define AC4_EAJOC (-105)    /* A-JOC/object leg: Part-2, structural only */
#define AC4_ESWITCH (-106)  /* short-block switch inside frame: e2 followup */

/* ---- per-frame decode context (filled by e3 TOC + audio-head walk) ---- */
typedef struct {
    int fs_index;     /* 0 = 44.1k base, 1 = 48k base */
    int rate_index;   /* 0..13 Table 83/84 row */
    int sf_mult;      /* 0 = base rate, 1 = x2 (96k), 2 = x4 (192k) */
    int ch_mode;      /* TOC ch_mode CODE value (0 mono, 2 stereo, 14 5.1) */
    int nch;          /* decoded channel count (1/2/6) */
    int spec_frontend; /* S1: 0 = ASF, 1 = SSF */
    int codec_mode;   /* S2-S6 value (0 = SIMPLE for mono/stereo/5x) */
    int spf;          /* samples per frame per channel (Table 83/84) */
    int frame_no;     /* 0-based emitted index (post ig-gate) */
    int ig;           /* b_iframe_global of this frame */
} ac4_frame_ctx_t;

/* ---- e0: Huffman tables ----
 * Live API: huffman_book_t + huffman_books[53] (order 0-10 ASF
 * books 1-11, 11 SCALEFAC, 12 SNF, 13-36 ACPL, 37-48 AJOC,
 * 49-52 DE ABS_0/DIFF_0/ABS_1/DIFF_1) +
 * per-book ASF_HCB_* arrays. Codewords: low LEN bits, MSB first. */
#if AC4_HAVE_HUFFMAN
#include "huffman_tables.h" /* e0's header, lands via merge */
#endif

/* ---- e1-C: entropy decode (followup; e1 wave05 = Python) ----
 * payload = audio-substream bytes, bitpos = first section bit.
 * Out: coeffs ch-major [nch][spf] double (e2 native). */
#if AC4_HAVE_ENTROPY
int ac4_entropy_decode(const uint8_t *payload, size_t payload_len,
                       size_t bitpos, const ac4_frame_ctx_t *ctx,
                       double *coeffs /* [nch][spf] */, int *nlines,
                       double *sfgain /* [nch][64] */, size_t *endpos);
#else
static int __attribute__((unused))
ac4_entropy_decode(const uint8_t *p, size_t l, size_t b,
                   const ac4_frame_ctx_t *c, double *cf, int *nl,
                   double *sg, size_t *e) {
    (void)p;
    (void)l;
    (void)b;
    (void)c;
    (void)cf;
    (void)nl;
    (void)sg;
    (void)e;
    return AC4_EUNIMPL;
}
#endif

/* ---- e2: IMDCT + KBD + OLA ----
 * API adopted VERBATIM from e2's src/imdct.h (uniform path is the
 * merge surface; switch path gated by e2's QUESTION-e2-1). */
#if AC4_HAVE_IMDCT
#include "imdct.h" /* e2's header, lands via merge */
#else
#define IMDCT_MAXN 8192
typedef struct {
    double ov[IMDCT_MAXN];
    int ov_len;
    int prev_n;
    int started;
} imdct_state;
static void __attribute__((unused)) imdct_reset(imdct_state *st) {
    (void)st;
}
static double __attribute__((unused)) imdct_alpha(int n, int full) {
    (void)n;
    (void)full;
    return 0.0;
}
static void __attribute__((unused))
imdct_push(imdct_state *st, const double *X, int n, int pn, int nn,
           double alpha, double palpha, double nalpha, double *pcm) {
    (void)st;
    (void)X;
    (void)n;
    (void)pn;
    (void)nn;
    (void)alpha;
    (void)palpha;
    (void)nalpha;
    (void)pcm;
}
static short __attribute__((unused)) imdct_d2s16(double v) {
    (void)v;
    return 0;
}
#endif

#endif /* AC4_STAGES_H */
