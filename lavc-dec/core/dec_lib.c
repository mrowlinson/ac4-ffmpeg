/*
 * AC-4 decoder streaming core (in-tree port; CLI/shim stripped at vendor)
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

/* In-tree port of the dec-lib streaming core: CLI/shim code stripped at
 * vendor time (see lavc-dec/README.md Q1); DSP below is dec-lib verbatim. */
/* ac4dec -- CLI shell per tests/CLI.md. C11, zero deps.
 *
 * Wave e4 (Part-2 structural skip): decode wired to container demux
 * (TS with prefix-strip + PAT/PMT/ES pick, MP4 moof/moov sample tables,
 * raw sync chains, bare raw frame) + full bv2 TOC walk per
 * spec/toc-bitfields.md (multi-presentation incl pres_ver 2-as-v1 shape,
 * channel/A-JOC/object groups, substream index table). A-JOC, object,
 * HSF and immersive-extension substream interiors are SKIPPED
 * structurally: bounds come from TOC sizes and are verified inside the
 * frame; interiors are never parsed. Bed = pres0 first channel-coded
 * group, first chan substream (IMS pres_ver 2 + cm 120/121 = stereo
 * core per Dolby ODK carriage rule, spec/part2-skip-e4.md).
 *
 * Payload is STUB silence (zeros): emitted frames x spf(samples) from
 * the internal grid (spec/mdct-sizes.md S4); sr label = round(spf*fps)
 * (H-SR2 HISTORICAL-Oct-2-oracle basis, drop e4-3; A-84:
 * emission stands on HEADER-TRUTH), fri 13 = nominal fs;
 * ig-gate drops leading
 * pre-I frames (first ig=1 on). Real substream decode = later waves
 * (#31/#32) behind this same surface. Streams with no channel-coded
 * group in any frame (p34 class): exit 0, frames=N parsed, EMPTY PCM,
 * note=no-bed (oracle 0ch agrees). Exit codes + FMT line per
 * tests/CLI.md.
 */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "acpl.h"
#include "aspx_tab.h"
#include "entropy.h"
#include "huffman_tab.h"
#include "imdct.h"
#include "qmf.h"
#include "sfb_offsets.h"
#include "ac4d.h"
#include <stdarg.h>
/* ---- ac4d_ctx: heap instance state (mechanical ctx conversion) ---- */
/* ctx-move from dec_lib.c:4356-4357 */
#define DRC_MAXMODES 8
#define DRC_MAXPTS 7
/* ctx-move from dec_lib.c:4359-4372 */
typedef struct {
    int mid;
    int is_curve; /* polyline valid (transmitted or default-filled) */
    int is_default; /* default-profile leg (filled post-eac3) */
    int nb_lo, nb_hi; /* transmitted curve band counts (f(nb) names) */
    int npts;
    double pl[DRC_MAXPTS]; /* level dB2, ascending */
    double pg[DRC_MAXPTS]; /* gain dB2 */
    int repeat_to; /* -1 = none, else target mode id */
    int gains_cfg; /* -1 = none, else Table-163 config id */
    int has_gains; /* drc_gains decoded (Table-75, SET-A) */
    int gains_nch, gains_nsf, gains_nb; /* decoded dims */
    int gains_db2[3][8][4]; /* dB2 gains [ch][sf][band] (val-64 + diffs) */
} drc_mode_t;
/* ctx-move from dec_lib.c:4374-4379 */
typedef struct {
    int valid; /* carried ndot config present */
    int eac3;
    int nmode;
    drc_mode_t modes[DRC_MAXMODES];
} drc_cfg_t;
/* ctx-move from dec_lib.c:4683-4706 */
typedef struct {
    int present; /* block walked this call */
    unsigned ver, prac;
    int has_dprac;
    unsigned dprac;
    int has_corr;
    unsigned corrtype;
    int has[6];
    unsigned val[6];
    int has_spch_dprac;
    unsigned spch_dprac;
    int has_eos;
    unsigned eos;
    int has_bndy;
    unsigned bndyoff;
    int has_lra;
    unsigned lra, lra_prac;
    int has_mntry;
    unsigned mntry;
    int has_maxmntry;
    unsigned maxmntry;
    int has_rtll;
    unsigned rtll;
} info_loud_t;
/* ctx-move from dec_lib.c:4718-4739 */
typedef struct {
    int present; /* walk completed without overread */
    unsigned audio_size;
    size_t meta_at;
    int b_dialog;
    int has_dlg_maxgain;
    unsigned dlg_maxgain;
    unsigned tools;
    int de_gate;
    int has_de_cfg;
    unsigned de_method, de_maxgain, de_chanconfig;
    unsigned de_skip_bits; /* T78-prep: post-cfg -> tools-end bits (de_data region size) */
    unsigned de_region_bits; /* P2: de_data region bits (cfg-independent) */
    int de_keep; /* P2: -1 n/a, I-frame 0 (implied), non-I 1st region bit */
    int de_par_ok; /* P2 walk: 1 ok, -1 fail, 0 skipped */
    int de_par_keep, de_par_ms, de_par_nch;
    int de_par[3][8];
    unsigned de_par_bits;
    int de_active; /* P2 drive: nch>0 && max|par|>=2 (T=2 pinned) */
    unsigned de_eff_cc; /* P2 drive: effective chanconfig this frame */
    int endok;
} info_audmeta_t;
/* ctx-move from dec_lib.c:2511-2514 */
typedef struct {
    int qmode, start, stop, mfscale, interp, preflat, limiter;
    int noisesbg, envbits, freqres;
} e_ax_cfg_t;
/* ctx-move from dec_lib.c:3082-3082 */
#define E3_CMP_MAXF 4096

typedef struct ac4d_ctx {
    int e_wav;
    int e_wav_bps;
    int e_info;
    int e_verb;
    int e_ajoc_core;
    const char *e_out_cplx;
    int e_render_prov;
    int e_drc_cli;
    int e_drc_or_given;
    double e_drc_or_cli;
    int e_de_given;
    double e_de_cli;
    double e_drc_boost;
    double e_drc_cut;
    uint8_t *e_wav_sp;
    size_t e_wav_sn;
    size_t e_wav_scap;
    int e_wav_serr;
    long e_wav_ch;
    long e_wav_frames;
    long e_crc_bad;
    unsigned e_sync_first;
    imdct_state e3_chst[12];
    double e3_pendco[12][IMDCT_MAXN];
    int e3_pendn[12];
    int e3_prevn[12];
    double e3_pcmbuf[IMDCT_MAXN];
    short e3_s16[2*IMDCT_MAXN];
    double e3_dngain;
    unsigned e3_dn_bits;
    double e3_reflevel;
    double e3_dngain_g0;
    int e3_imm_mode;
    int e3_acpl_nbands;
    int e3_acpl_fine;
    int e3_acpl_have;
    double e3_imm_fold_div;
    int e5_acpl_nbands;
    int e5_acpl_fine;
    int e5_acpl_have;
    int e5_need_postpass;
    int e5_last_acpl;
    int e5_acpl_elig;
    int e3_imm_elig;
    int e3_zeroext;
    double e3_m4buf[IMDCT_MAXN];
    double e3_m4db_mut[13];
    int e3_m4db_ready;
    double e3_shfbuf[IMDCT_MAXN];
    double e3_shfcurve[128];
    int e3_shf_mode;
    int e3_shf_c0;
    int e3_shf_c1;
    double e3_shf_db;
    int e3_shf_parsed;
    e_ax_cfg_t e_ax_cfg;
    int e_ax_have_cfg;
    int e_ax_xover[4];
    int e_ax_prev_stop[7];
    int e_ax_have_prev[7];
    int e3_cmp_n;
    int e3_cmp_on[8];
    unsigned char e3_cmp_onm[E3_CMP_MAXF];
    unsigned char e3_cmp_avg[E3_CMP_MAXF];
    unsigned char e3_cmp_syn[E3_CMP_MAXF];
    int e3_cmp_xov[3][E3_CMP_MAXF];
    int e3_cmp_mfs[E3_CMP_MAXF];
    int e3_cmp_sta[E3_CMP_MAXF];
    int e3_cmp_sto[E3_CMP_MAXF];
    int e3_cmp_codec[E3_CMP_MAXF];
    int e3_cmp_m2[E3_CMP_MAXF];
    int e3_cmp_i0[5][E3_CMP_MAXF];
    int e3_cmp_i1[5][E3_CMP_MAXF];
    int e3_5x_codec;
    int e3_5x_m2;
    int e_dn_have;
    unsigned e_dn_dial;
    int e_dn_saw_pres;
    int e_f0_have_gate;
    int e_f0_gate;
    int e_cmp_f0latched;
    int e_cmp_f0nch;
    int e_cmp_f0sync;
    int e_cmp_f0mask;
    int e_cmp_f0avg;
    int e_5x_f0latched;
    unsigned e_5x_codec;
    unsigned e_5x_cc;
    unsigned e_5x_m2;
    int e_5x_mdct0;
    int e_5x_mdct1;
    int e_5x_ms;
    int e_5x_lms;
    int e_5x_3ch;
    int e_decfg_have;
    size_t e_decfg_frame;
    unsigned e_decfg_chan;
    unsigned e_decfg_tools;
    unsigned e_decfg_maxgain;
    unsigned e_decfg_method;
    long e_ncfg;
    long e_icfg0;
    long e_icfg1;
    int e3_acpl_nb;
    int e3_acpl_st;
    int e3_acpl_qm;
    drc_cfg_t e_drc;
    int e_drc_noted;
    info_loud_t e_loud;
    int e_loud_latched;
    info_audmeta_t e_audmeta;
    int e_audmeta_latched;
    int e_f0_have_dcfg;
    drc_cfg_t e_f0_dcfg;
    int e_dewalk_have;
    unsigned e_dewalk_method;
    unsigned e_dewalk_chan;
    int e_dewalk_prev[3][8];
    int e_dewalk_prevok[3];
    int e_de_cur_active;
    int e_de_cur_mabs;
    unsigned e_de_cur_cc;
    unsigned e_drc_dial;
    ac4e_sap_t xm_sap5a;
    ac4e_sap_t xm_sap5b;
    ac4e_sap_t xm_sap5c0;
    ac4e_sap_t xm_sap5x;
    ac4e_sap_t xm_sap7x;
    ac4e_sap_t xm_tsap[2];
    ac4e_sap_t xm_asq[4];
    asf_chan_t xm_ch[12];
    ac4e_sap_t xm_sap;
    /* ---- streaming API + H1 pipeline state (hand) ---- */
    /* H1 ring sizes (absolute-indexed, fixed): skeleton/spill hold 4
     * frames of 2048; compand raw holds 3 frames + flush margin. */
/* Push-fed ring sizing (sk/tf/spill): worst-case occupancy is
 * skew + (4+need)*spf -- steady 4-frame emit lag (OLA-debt stall +
 * h_need lookahead) plus bursty OLA pushes (telescoping window
 * sizes: up to 3*spf in one frame on long->short changes; 5632
 * observed). spf_max=2048, need=1: 10240 + skew (0 on corpus;
 * 1-spf budget). Tripwires below fail loud past this. */
#define AC4D_SKN (6 * 2048)
#define AC4D_SPN (6 * 2048)
#define AC4D_RAWN (5 * 2048 + 1024)
#define AC4D_APFN 4
    ac4d_opts ao;
    int ao_open;
    int h_fmt;           /* format latched from first feed */
    int h_nch;           /* latched decode nch (1/2/6/8) */
    int h_spf;           /* latched spf */
    int h_bed_cm;        /* latched TOC bed_cm */
    int h_pres_ver;      /* latched pres_ver0 */
    int h_ims;           /* latched IMS class (gap-tolerant stereo) */
    int h_imm_mode;      /* 714 latched mode (-1 = n/a yet) */
    int h_rate;          /* latched fri row */
    int h_sr;            /* output sample-rate label */
    long h_fed;          /* frames fed */
    long h_emit;         /* next emit ordinal */
    long h_ready;        /* frames ready to emit (data-driven) */
    int h_need;          /* analyze lookahead (1; 2 when spf < 577) */
    long h_pp;           /* 7.1.4 frames post-passed (pump) */
    int h_hard_err;      /* sticky hard error (0 = none) */
    const char *h_hard_note;
    const char *h_gap;   /* current gap note (NULL if none) */
    int h_stub;          /* latched stub class (silence path) */
    int h_gap_sticky;    /* whitelisted gap latched (suffix silence) */
    long h_gap_start;    /* first silent ordinal (sticky) */
    int h_flushing;      /* flush path entered */
    int h_flushed;       /* pipeline fully drained */
    unsigned h_dsprev;
    int h_dshave;
    int h_dn_tent;       /* DN latched from I-frame; re-latch on first P */
    int h_dn_dirty;      /* frames emitted/analyzed under tentative gain */
    int h_dn_ordirty;    /* an older epoch ended tentative+dirty (its gain
                          * may still be superseded: batch latches scan
                          * across later splices) */
    double h_dn_org0;    /* that epoch's tentative triple (re-latch */
    double h_dn_orgain;  /* comparison: a later P with a different DN */
    unsigned h_dn_orbits; /* means batch used it: fail loud) */
    double h_dn_pg0;     /* pre-epoch gain triple (re-latch-fail restore, */
    double h_dn_pgain;   /* as batch: failed sample keeps prior gain) */
    unsigned h_dn_pbits;
    long h_oc[12];       /* absolute pushed samples per ch */
    /* skeleton + mode-0 track FIFOs (absolute-indexed rings;
     * write heads are h_oc / spill oc counters) */
    double sk_fifo[8][AC4D_SKN]; /* pre-gain floats (gain at gather:
                                   * DN re-latch on first P stays exact) */
    double tf_fifo[12][AC4D_SKN];
    int tf_used;         /* latched mode-0 path active */
    /* spill FIFOs (post-pass mirror, absolute-indexed rings) */
    double sp3[4][AC4D_SPN];
    double sp5[2][AC4D_SPN];
    /* apf param rings (tagged by frame ordinal) */
    struct {
        acpl_mod_t m[4];
        double fold_div;
        long fno;
        int have;
    } ap3[AC4D_APFN];
    struct {
        acpl_mod_t m[2];
        long fno;
        int have;
    } ap5[AC4D_APFN];
    /* per-frame parse-gap flags for in-flight frames */
    long fr_fno[8];
    int fr_gap[8];
    const char *fr_gnote[8];
    /* post-pass + acpl instance state */
    acpl_state_t acpl;
    acpl_imm_t pp_imm;
    acpl_5x_t pp_5x;
    int pp_imm_on, pp_5x_on;
    double pp_blk[6][2048];
    /* per-frame flags (batch names kept; elig/last_acpl moved by script) */
    int e3_spill_on;
    int e5_spill_on;
    long e3_spill_oc[4];
    long e5_spill_oc[2];
    /* emit-stage frame + compand continuous state */
    short fr_s16[8][2048];
    double fr_flt[8][2048];      /* pre-gain staged frame (pp ring input;
                                   * gain applies at emit for exact DN
                                   * re-latch) */
#define AC4D_PPN (5 * 2048)
    short em_s16[8][2048];       /* emit scratch frame */
    double pp_ring[8][AC4D_PPN]; /* pre-gain postpassed frames (nch6/8
                                   * emit base; gain at emit) */
    double mx_tmp[12][2048];     /* mode-0 mix gather scratch */
    long h_an;                   /* frames analyzed (compand pump) */
    qmf_state_t cmp_qs[8];
    int cmp_qrun[8];
    double cmp_held[8];
    double cmp_folsm[8];
    double cmp_slewsm[8];
    double cmp_gfifo[8][16];
    int cmp_gatefifo[8][16];
    int cmp_gpos[8];
    double complex cmp_Q[(8 * 32)][64];
    double cmp_fp[2048];
    double cmp_raw[8][AC4D_RAWN];
    long cmp_w[8];       /* absolute raw write head per ch */
    /* compand env snapshot (batch reads once per decode) */
    int ce_nocmp, ce_unity, ce_inv, ce_census, ce_fullint, ce_avgall;
    int ce_gainhold, ce_unshift, ce_useflat, ce_hotfill, ce_usofol;
    int ce_onsetavg, ce_floorskip;
    long ce_dumpf, ce_gdel, ce_prewarm, ce_frameoff, ce_frameof2;
    long ce_unityf, ce_unityt0, ce_unityt1, ce_fade0;
    double ce_fol_a, ce_fol_r, ce_slewrate, ce_dampf;
    double ce_G, ce_ex, ce_trim, ce_pre;
    int ce_slots, ce_on;
} ac4d_ctx;

static ac4d_ctx *_ac4d_self;
#define e_wav (_ac4d_self->e_wav)
#define e_wav_bps (_ac4d_self->e_wav_bps)
#define e_info (_ac4d_self->e_info)
#define e_verb (_ac4d_self->e_verb)
#define e_ajoc_core (_ac4d_self->e_ajoc_core)
#define e_out_cplx (_ac4d_self->e_out_cplx)
#define e_render_prov (_ac4d_self->e_render_prov)
#define e_drc_cli (_ac4d_self->e_drc_cli)
#define e_drc_or_given (_ac4d_self->e_drc_or_given)
#define e_drc_or_cli (_ac4d_self->e_drc_or_cli)
#define e_de_given (_ac4d_self->e_de_given)
#define e_de_cli (_ac4d_self->e_de_cli)
#define e_drc_boost (_ac4d_self->e_drc_boost)
#define e_drc_cut (_ac4d_self->e_drc_cut)
#define e_wav_sp (_ac4d_self->e_wav_sp)
#define e_wav_sn (_ac4d_self->e_wav_sn)
#define e_wav_scap (_ac4d_self->e_wav_scap)
#define e_wav_serr (_ac4d_self->e_wav_serr)
#define e_wav_ch (_ac4d_self->e_wav_ch)
#define e_wav_frames (_ac4d_self->e_wav_frames)
#define e_crc_bad (_ac4d_self->e_crc_bad)
#define e_sync_first (_ac4d_self->e_sync_first)
#define e3_chst (_ac4d_self->e3_chst)
#define e3_pendco (_ac4d_self->e3_pendco)
#define e3_pendn (_ac4d_self->e3_pendn)
#define e3_prevn (_ac4d_self->e3_prevn)
#define e3_pcmbuf (_ac4d_self->e3_pcmbuf)
#define e3_s16 (_ac4d_self->e3_s16)
#define e3_dngain (_ac4d_self->e3_dngain)
#define e3_dn_bits (_ac4d_self->e3_dn_bits)
#define e3_reflevel (_ac4d_self->e3_reflevel)
#define e3_dngain_g0 (_ac4d_self->e3_dngain_g0)
#define e3_imm_mode (_ac4d_self->e3_imm_mode)
#define e3_acpl_nbands (_ac4d_self->e3_acpl_nbands)
#define e3_acpl_fine (_ac4d_self->e3_acpl_fine)
#define e3_acpl_have (_ac4d_self->e3_acpl_have)
#define e3_imm_fold_div (_ac4d_self->e3_imm_fold_div)
#define e5_acpl_nbands (_ac4d_self->e5_acpl_nbands)
#define e5_acpl_fine (_ac4d_self->e5_acpl_fine)
#define e5_acpl_have (_ac4d_self->e5_acpl_have)
#define e5_need_postpass (_ac4d_self->e5_need_postpass)
#define e5_last_acpl (_ac4d_self->e5_last_acpl)
#define e5_acpl_elig (_ac4d_self->e5_acpl_elig)
#define e3_imm_elig (_ac4d_self->e3_imm_elig)
#define e3_zeroext (_ac4d_self->e3_zeroext)
#define e3_m4buf (_ac4d_self->e3_m4buf)
#define e3_m4db_mut (_ac4d_self->e3_m4db_mut)
#define e3_m4db_ready (_ac4d_self->e3_m4db_ready)
#define e3_shfbuf (_ac4d_self->e3_shfbuf)
#define e3_shfcurve (_ac4d_self->e3_shfcurve)
#define e3_shf_mode (_ac4d_self->e3_shf_mode)
#define e3_shf_c0 (_ac4d_self->e3_shf_c0)
#define e3_shf_c1 (_ac4d_self->e3_shf_c1)
#define e3_shf_db (_ac4d_self->e3_shf_db)
#define e3_shf_parsed (_ac4d_self->e3_shf_parsed)
#define e_ax_cfg (_ac4d_self->e_ax_cfg)
#define e_ax_have_cfg (_ac4d_self->e_ax_have_cfg)
#define e_ax_xover (_ac4d_self->e_ax_xover)
#define e_ax_prev_stop (_ac4d_self->e_ax_prev_stop)
#define e_ax_have_prev (_ac4d_self->e_ax_have_prev)
#define e3_cmp_n (_ac4d_self->e3_cmp_n)
#define e3_cmp_on (_ac4d_self->e3_cmp_on)
#define e3_cmp_onm (_ac4d_self->e3_cmp_onm)
#define e3_cmp_avg (_ac4d_self->e3_cmp_avg)
#define e3_cmp_syn (_ac4d_self->e3_cmp_syn)
#define e3_cmp_xov (_ac4d_self->e3_cmp_xov)
#define e3_cmp_mfs (_ac4d_self->e3_cmp_mfs)
#define e3_cmp_sta (_ac4d_self->e3_cmp_sta)
#define e3_cmp_sto (_ac4d_self->e3_cmp_sto)
#define e3_cmp_codec (_ac4d_self->e3_cmp_codec)
#define e3_cmp_m2 (_ac4d_self->e3_cmp_m2)
#define e3_cmp_i0 (_ac4d_self->e3_cmp_i0)
#define e3_cmp_i1 (_ac4d_self->e3_cmp_i1)
#define e3_5x_codec (_ac4d_self->e3_5x_codec)
#define e3_5x_m2 (_ac4d_self->e3_5x_m2)
#define e_dn_have (_ac4d_self->e_dn_have)
#define e_dn_dial (_ac4d_self->e_dn_dial)
#define e_dn_saw_pres (_ac4d_self->e_dn_saw_pres)
#define e_f0_have_gate (_ac4d_self->e_f0_have_gate)
#define e_f0_gate (_ac4d_self->e_f0_gate)
#define e_cmp_f0latched (_ac4d_self->e_cmp_f0latched)
#define e_cmp_f0nch (_ac4d_self->e_cmp_f0nch)
#define e_cmp_f0sync (_ac4d_self->e_cmp_f0sync)
#define e_cmp_f0mask (_ac4d_self->e_cmp_f0mask)
#define e_cmp_f0avg (_ac4d_self->e_cmp_f0avg)
#define e_5x_f0latched (_ac4d_self->e_5x_f0latched)
#define e_5x_codec (_ac4d_self->e_5x_codec)
#define e_5x_cc (_ac4d_self->e_5x_cc)
#define e_5x_m2 (_ac4d_self->e_5x_m2)
#define e_5x_mdct0 (_ac4d_self->e_5x_mdct0)
#define e_5x_mdct1 (_ac4d_self->e_5x_mdct1)
#define e_5x_ms (_ac4d_self->e_5x_ms)
#define e_5x_lms (_ac4d_self->e_5x_lms)
#define e_5x_3ch (_ac4d_self->e_5x_3ch)
#define e_decfg_have (_ac4d_self->e_decfg_have)
#define e_decfg_frame (_ac4d_self->e_decfg_frame)
#define e_decfg_chan (_ac4d_self->e_decfg_chan)
#define e_decfg_tools (_ac4d_self->e_decfg_tools)
#define e_decfg_maxgain (_ac4d_self->e_decfg_maxgain)
#define e_decfg_method (_ac4d_self->e_decfg_method)
#define e_ncfg (_ac4d_self->e_ncfg)
#define e_icfg0 (_ac4d_self->e_icfg0)
#define e_icfg1 (_ac4d_self->e_icfg1)
#define e3_acpl_nb (_ac4d_self->e3_acpl_nb)
#define e3_acpl_st (_ac4d_self->e3_acpl_st)
#define e3_acpl_qm (_ac4d_self->e3_acpl_qm)
#define e_drc (_ac4d_self->e_drc)
#define e_drc_noted (_ac4d_self->e_drc_noted)
#define e_loud (_ac4d_self->e_loud)
#define e_loud_latched (_ac4d_self->e_loud_latched)
#define e_audmeta (_ac4d_self->e_audmeta)
#define e_audmeta_latched (_ac4d_self->e_audmeta_latched)
#define e_f0_have_dcfg (_ac4d_self->e_f0_have_dcfg)
#define e_f0_dcfg (_ac4d_self->e_f0_dcfg)
#define e_dewalk_have (_ac4d_self->e_dewalk_have)
#define e_dewalk_method (_ac4d_self->e_dewalk_method)
#define e_dewalk_chan (_ac4d_self->e_dewalk_chan)
#define e_dewalk_prev (_ac4d_self->e_dewalk_prev)
#define e_dewalk_prevok (_ac4d_self->e_dewalk_prevok)
#define e_de_cur_active (_ac4d_self->e_de_cur_active)
#define e_de_cur_mabs (_ac4d_self->e_de_cur_mabs)
#define e_de_cur_cc (_ac4d_self->e_de_cur_cc)
#define e_drc_dial (_ac4d_self->e_drc_dial)
/* Hand-kept batch names (spill flags/counters; elig moved by script). */
#define e3_spill_on (_ac4d_self->e3_spill_on)
#define e5_spill_on (_ac4d_self->e5_spill_on)
#define e3_spill_oc (_ac4d_self->e3_spill_oc)
#define e5_spill_oc (_ac4d_self->e5_spill_oc)

/* Nonzero static initializers, applied by ac4d_open after calloc. */
static void ac4d_state_init(void) {
    e_wav_bps = 2;
    e_verb = 3;
    e_out_cplx = "5.1.2";
    e_drc_or_cli = -31.0;
    e_drc_boost = 100.0;
    e_drc_cut = 100.0;
    e3_dngain = 1.0;
    e3_dn_bits = 96;
    e3_reflevel = -31.0;
    e3_dngain_g0 = 1.0;
    e3_imm_mode = -1;
    e3_acpl_nbands = 15;
    e3_acpl_fine = 1;
    e3_imm_fold_div = 1.4142135623730951;
    e5_acpl_nbands = 15;
    e5_acpl_fine = 1;
    e3_shf_mode = -1;
    e_icfg0 = -1;
    e_icfg1 = -1;
    e_drc_dial = 27;
}

/* H1 absolute-indexed ring helpers (fixed-size; readers must stay within
 * one ring length behind the write head -- enforced by checks at emit). */
static void ac4d_rput_d(double *ring, long ringn, long w, const double *src,
                        int k) {
    long o = w % ringn;
    long first = ringn - o < k ? ringn - o : k;
    if (first > 0)
        memcpy(ring + o, src, (size_t)first * sizeof(double));
    if (k > first)
        memcpy(ring, src + first, (size_t)(k - first) * sizeof(double));
}
/* (deleted: belonged to removed DRC-apply/DE-drive legs) */
static double ac4d_rget_d(const double *ring, long ringn, long pos) {
    return ring[pos % ringn];
}
static short __attribute__((unused))
ac4d_rget_s(const short *ring, long ringn, long pos) {
    return ring[pos % ringn];
}

/* Lib build: route decode-core diagnostics to the log callback
 * (default quiet). All lib-path fprintf targets are stderr. */
static void ac4d_logf2(FILE *fp, const char *fmt, ...) {
    ac4d_ctx *cx = _ac4d_self;
    char msg[1024];
    va_list ap;
    if (fp != stderr)
        return;
    if (!cx || !cx->ao.log)
        return;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    cx->ao.log(5, msg, cx->ao.log_u);
}
#define fprintf(fp, ...) ac4d_logf2(fp, __VA_ARGS__)


/* --wav (G-WAV/E4): RF64 container writer, DRP shape (CR-596: ds64
 * len 28, data -1 size). Chunks: RF64/ds64/fmt/data; mbwf/bext hook
 * reserved (Q-64 Q2). e_wav=0 (default) = raw path byte-identical. */
/* ctx-member */
/* ctx-member */
/* E8 --print-info (decode-to-null + TAGs on stdout; exits 0/1/2). */
/* ctx-member */

/* E9 verbosity (ffmpeg-mirror levels; default info = current behavior).
 * 0 quiet, 1 error, 2 warning, 3 info, 4 verbose, 5 debug. panic/fatal
 * map to 1, trace to 5. FMT prints except at quiet; usage errors always
 * print (ffmpeg behavior); AC4DEC_* env probes stay orthogonal except
 * -v debug+, which also enables the AC4DEC_DEBUG probe surface. */
/* ctx-member */
static int e9_show(int lvl) {
    return e_verb >= lvl;
}
static int e9_debug(void) {
    return getenv("AC4DEC_DEBUG") != NULL || e_verb >= 5;
}
/* D26 provisional route (owner D1b re-sequence; turf 74; re-gate pipe4-C5).
 * Control plane only in v1: select + report; PCM render owed v2.
 * DRP names cited per A-60/A4; oarlib stays disabled. */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* D26-v1 bed leg: report the provisional route AFTER FMT (FMT-first
 * convention: batteries parse stderr head -1 as FMT). */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* D10 DE surface (--ac4dec-de-level, preference steps, -12..12 int,
 * default 0; A-82 Q1 mirror). P1 DRIVE LIVE: bg-duck + hard-cap-9 +
 * chanconfig routing (post-trim pre-emit gain pass). P2 (T78 de_data
 * activity + boost/cut-if-active + fade-in primer) pending. user=0 /
 * user<0 / no-cfg -> pass skipped (PCM-identity by construction).
 * DEDEC diagnostic line under AC4DEC_DEBUG. */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */

/* Effective mode-select ref: CLI > env > -31 default (A-76: mirror
 * of --ac4dec-out-ref-level; range validated at argv parse). */
static double drc_outref_eff(void) {
    if (e_drc_or_given)
        return e_drc_or_cli;
    {
        const char *op = getenv("AC4DEC_DRC_OUTREF");
        if (op && *op)
            return strtod(op, NULL);
    }
    return -31.0;
}
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */

/* (wav spool deleted: shim spools itself; lib emits raw frames) */
/* E3 writer-defined (no DRP ref per A-64/A4): s16 sample -> output
 * width. 24-bit = LE s16 + zero-pad byte (sign-preserving; full
 * 24-bit resolution would need float-stage plumbing -- tracked). 32f
 * = s16/32768 LE float. Shared by --wav spool + raw path (single
 * semantics). Returns bytes written (2/3/4). */
static int wav_s16_to_bytes(short v, int bps, uint8_t out[4]) {
    if (bps == 3) {
        out[0] = (uint8_t)(v & 0xff);
        out[1] = (uint8_t)((v >> 8) & 0xff);
        out[2] = 0;
        return 3;
    }
    if (bps == 4) {
        float f = (float)v / 32768.0f;
        memcpy(out, &f, 4); /* LE host (x86-64/arm64) */
        return 4;
    }
    out[0] = (uint8_t)(v & 0xff);
    out[1] = (uint8_t)((v >> 8) & 0xff);
    return 2;
}




/* Emit RF64 header + pcm (pcmn bytes; NULL = zeros) + odd pad +
 * trailing LIST-INFO (Q-65 Q2 rule: DRP appends empty LIST-INFO
 * 12B after data, always; byte-exactness needs it). nframes =
 * sample frames (per-ch). Returns 0 ok, -1 write fail. */

/* FMT: exactly one line to stderr per attempt. */
/* CRC census (E8 CRC tag (sync word) + decode-verb crc_bad count (A-79
 * Q5: counts belong to decode-verb)). Single-run process: zero-init
 * suffices; re-armed explicitly in main before collection. */
/* ctx-member */
/* ctx-member */


/* Success-note with crc_bad suffix (decode-verb count, A-79 Q5). */


/* ---- sync framing (spec/sync-framing.md; accept rule = census.c) ---- */

static uint16_t rd16be(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static int is_sync(uint16_t w) { return w == 0xAC40 || w == 0xAC41; }

/* CRC-16 G.4.2: poly x^16+x^15+x^2+1, init 0, no reflect/xor. */
static int crc16_ok(const uint8_t *lenbytes, size_t lenlen,
                    const uint8_t *frame, size_t framelen,
                    const uint8_t stored[2]) {
    uint16_t reg = 0;
    const uint8_t *segs[3] = {lenbytes, frame, stored};
    size_t lens[3] = {lenlen, framelen, 2};
    for (int s = 0; s < 3; s++) {
        for (size_t i = 0; i < lens[s]; i++) {
            reg ^= (uint16_t)segs[s][i] << 8;
            for (int b = 0; b < 8; b++)
                reg = (reg & 0x8000) ? (uint16_t)((reg << 1) ^ 0x8005)
                                     : (uint16_t)(reg << 1);
        }
    }
    return reg == 0;
}

/* 0 on valid sync frame at o; out hdrlen/rawlen/has_crc. */
static int parse_sync(const uint8_t *d, size_t n, size_t o, size_t *hdrlen,
                      size_t *rawlen, int *has_crc) {
    if (o + 4 > n)
        return -1;
    uint16_t w = rd16be(d + o);
    if (!is_sync(w))
        return -1;
    size_t hl, rl;
    uint16_t s16 = rd16be(d + o + 2);
    if (s16 == 0xFFFF) {
        if (o + 7 > n)
            return -1;
        rl = ((size_t)d[o + 4] << 16) | ((size_t)d[o + 5] << 8) | d[o + 6];
        hl = 7;
    } else {
        rl = s16;
        hl = 4;
    }
    int crc = (w == 0xAC41);
    size_t total = hl + rl + (crc ? 2 : 0);
    if (o + total > n)
        return -1;
    if (crc && !crc16_ok(d + o + 2, hl - 2, d + o + hl, rl, d + o + hl + rl))
        return -1;
    *hdrlen = hl;
    *rawlen = rl;
    *has_crc = crc;
    return 0;
}

typedef struct {
    size_t off, rawlen, hdrlen;
    int crc;
} syncframe_t;

/* CRC-blind head plausibility at a sync-word position: sized +
 * in-bounds. Out tot = full frame footprint. Returns 1 if plausible. */

/* Chain-walk accept: >=2 chained frames, or 1 frame ending at EOF.
 * Returns malloc'd array (*n_out frames) or NULL with *n_out=0. */

/* Longest contiguous run in a chain_walk list (offset order): out
 * start/count. Kills overlap re-walks (esc-class 2nd chains). */

/* ---- MPEG-TS demux (e4; PAT/PMT/PES port of census.c) ---- */



/* Strip prefix-class padding: first offset with 20x 188-stride
 * 0x47. Returns offset or (size_t)-1. (esc = 112.) */

typedef struct {
    uint8_t *v;
    size_t n, cap;
} bytes_t;


/* section body bounds inside a PUSI payload; -1 = bad */

/* Collect ES pids (PMT order) from stripped TS base. Out count. */

/* Reassemble PES payload for one pid. 0 ok (out owns bytes). */

/* ---- ISOBMFF sample tables (e4; port of census.c) ---- */

typedef struct {
    uint32_t typ;
    size_t off, size, hdr;
} box_t;

typedef struct {
    box_t *v;
    size_t n, cap;
} boxlist_t;



#define TYP(a, b, c, e) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (e))



typedef struct {
    size_t off, size;
} sample_t;

typedef struct {
    sample_t *v;
    size_t n, cap;
    uint64_t elst_head, elst_tail, elst_mdur; /* edit-list trims, media units */
    int elst_state; /* 1 applied-shape, -1 present-complex, 0 absent */
} samplelist_t;


/* moof path: returns 1 if any trun runs found */

/* elst (edit list) trims for the selected moov trak. Out: head/tail trims
 * + media duration, all in MEDIA timescale units. Returns 1 on a supported
 * shape (v0/v1, leading empties + single media entry, rate 1), else 0 with
 * outs zeroed (absent/complex = untrimmed, honest scope). */

/* moov path: first audio trak sample table; 1 ok, 0 none */


/* ---- bv2 TOC walk (e4; C port of tools/toc_census_d2.py bv2 path,
 * field order/widths per spec/toc-bitfields.md S1-S4) ---- */

typedef struct {
    const uint8_t *d;
    size_t nbits, pos;
    int err;
} br_t;

static unsigned br_u(br_t *b, int n) {
    unsigned v = 0;
    for (int i = 0; i < n; i++) {
        if (b->pos >= b->nbits) {
            b->err = 1;
            return 0;
        }
        v = (v << 1) |
            ((b->d[b->pos >> 3] >> (7 - (b->pos & 7))) & 1u);
        b->pos++;
    }
    return v;
}

/* variable_bits(n): P1 Table 3 loop. */
static unsigned br_var(br_t *b, int n) {
    unsigned v = 0;
    for (;;) {
        v += br_u(b, n);
        if (b->err)
            return 0;
        if (!br_u(b, 1) || b->err)
            return v;
        v = (v << n) + (unsigned)(1 << n);
    }
}

#define E4_MAXNS 64
#define E4_MAXPRES 16
#define E4_MAXGRP 16

typedef struct {
    int fs, rate, ig;
    int bv; /* bitstream_version (Table-4 TOC first 2b, verbatim) */
    unsigned seq;
    size_t tocB, pbase, tail;
    int ns;
    unsigned nsub_audio; /* audio substreams (present groups, ch+obj) */
    size_t sizes[E4_MAXNS];
    int npres, pres_ver0;
    int b_pres_ndot; /* TOC b_pres_ndot (fresh-parse trigger; NOT an
                      * I-gate -- A-80: displayed-config = derived rule) */
    int bed_si; /* audio substream index, -1 = none */
    int bed_obj; /* !b_channel_coded of bed group (CH5X obj; no-bed:
                  * first selected group) */
    int bed_ch; /* bed channels, 0 = none */
    int bed_cm;
    int bif; /* bed substream Table-9 b_iframe: ELEMENT I-flag
              * (duet3-dec-writer: TOC b_iframe_global (ig) is the
              * frame gate only; elements take the substream bit).
              * Bit-identical to tool b_audio_ndot (same position/count;
              * passed as audio b_iframe to e3_audmeta_read). */
    int n_ajoc, n_obj; /* skipped object-class substreams */
    int n_hsf; /* groups with b_hsf_ext set (D46 skip census) */
} toc_t;

static unsigned br_substream_index(br_t *b) {
    unsigned si = br_u(b, 2);
    if (si == 3)
        si += br_var(b, 2);
    return si;
}

static void br_content_type(br_t *b) {
    (void)br_u(b, 3); /* classifier */
    if (br_u(b, 1)) {
        if (br_u(b, 1)) {
            (void)br_u(b, 1);  /* b_start_tag */
            (void)br_u(b, 16); /* chunk */
        } else {
            unsigned n = br_u(b, 6);
            if (n)
                (void)br_u(b, (int)(8 * n));
        }
    }
}

static void br_emdf_info(br_t *b) {
    unsigned v = br_u(b, 2);
    if (v == 3)
        (void)br_var(b, 2);
    unsigned k = br_u(b, 3);
    if (k == 7)
        (void)br_var(b, 3);
    if (br_u(b, 1))
        (void)br_substream_index(b);
    unsigned lp = br_u(b, 2), ls = br_u(b, 2);
    unsigned skip = 0;
    if (lp > 0)
        skip += 1u << (2 * (lp - 1));
    if (ls > 0)
        skip += 1u << (2 * (ls - 1));
    if (skip)
        (void)br_u(b, (int)(8 * skip));
}

/* frame_rate_multiply -> factor (P1 T87). */
static int br_frm_factor(br_t *b, int rate) {
    if (rate == 2 || rate == 3 || rate == 4) {
        unsigned m = br_u(b, 1);
        if (m)
            return br_u(b, 1) ? 4 : 2;
        return 1;
    }
    if (rate == 0 || rate == 1 || rate == 7 || rate == 8 || rate == 9)
        return br_u(b, 1) ? 2 : 1;
    return 1;
}

/* channel_mode prefix walk -> wire value (d2 parse_channel_mode). */
static unsigned br_channel_mode(br_t *b) {
    unsigned v = br_u(b, 1);
    if (v == 0)
        return 0;
    v = (v << 1) | br_u(b, 1);
    if (v == 2)
        return v;
    v = (v << 2) | br_u(b, 2);
    if (v == 12 || v == 13 || v == 14)
        return v;
    v = (v << 3) | br_u(b, 3);
    if (v != 126 && v != 127)
        return v;
    v = (v << 1) | br_u(b, 1);
    if (v != 254 && v != 255)
        return v;
    v = (v << 1) | br_u(b, 1);
    if (v != 511)
        return v;
    v += br_var(b, 2);
    return v;
}

static void br_sf_br_tail(br_t *b, int fs) {
    if (fs == 1 && br_u(b, 1))
        (void)br_u(b, 1); /* sf_multiplier */
    if (br_u(b, 1))
        (void)br_u(b, 3); /* bitrate_indicator flat-3 (d2 OPEN-7) */
}

static void br_bed_dyn_obj_assign(br_t *b, unsigned n_sig) {
    if (br_u(b, 1)) /* b_dyn_objects_only */
        return;
    if (br_u(b, 1)) { /* b_isf */
        (void)br_u(b, 3);
        return;
    }
    if (br_u(b, 1)) { /* b_ch_assign_code */
        (void)br_u(b, 3);
        return;
    }
    if (br_u(b, 1)) { /* flags present */
        if (br_u(b, 1))
            (void)br_u(b, 17);
        else
            (void)br_u(b, 10);
        return;
    }
    /* counted list */
    unsigned nbed = 1;
    if (n_sig > 1) {
        unsigned nb = 0, t = n_sig - 1;
        while (t) {
            nb++;
            t >>= 1;
        }
        if (nb < 1)
            nb = 1;
        nbed = br_u(b, (int)nb) + 1;
    }
    for (unsigned i = 0; i < nbed; i++)
        (void)br_u(b, 4);
}

static void br_oamd_common(br_t *b) {
    if (!br_u(b, 1))
        (void)br_u(b, 5);
    (void)br_u(b, 1); /* b_bed_object_chan_distribute */
    if (br_u(b, 1)) {
        unsigned nb = br_u(b, 1) + 1;
        if (nb == 2)
            nb += br_var(b, 2);
        if (nb)
            (void)br_u(b, (int)(8 * nb)); /* interior skipped by count */
    }
}

/* Parse one P2 v1-info record. Out: group_index list, frf, pres_ver,
 * pres ndot. Returns 0 ok, -1 reject (err or cap). */
static int br_pres_v1(br_t *b, int rate, int *gis, int *ngi, int *frf,
                      int *pver, int *pndot) {
    /* FUZZ-DEC-1: cfg==6 early path skipped the else block that assigns
     * ngi and frf, leaving caller stack garbage (UBSan OOB at the gis
     * row scan). Initialize all outputs up front. */
    *ngi = 0;
    *frf = 1;
    *pndot = 0;
    int single = (int)br_u(b, 1);
    int cfg = -1;
    if (!single) {
        cfg = (int)br_u(b, 3);
        if (cfg == 7)
            cfg += (int)br_var(b, 2);
    }
    unsigned ver = 0;
    while (br_u(b, 1) == 1) {
        ver++;
        if (ver > 8 || b->err)
            return -1;
    }
    *pver = (int)ver;
    int add_emdf = 0;
    if (!single && cfg == 6) {
        add_emdf = 1;
    } else {
        (void)br_u(b, 3); /* md_compat */
        if (br_u(b, 1))
            (void)br_var(b, 2); /* pres_id */
        int f = br_frm_factor(b, rate);
        *frf = f;
        if ((rate == 5 || rate == 6 || rate == 7 || rate == 8 ||
             rate == 9) &&
            f == 1) {
            if (br_u(b, 1))
                (void)0; /* fraction 2 */
        }
        if (rate == 10 || rate == 11 || rate == 12) {
            if (br_u(b, 1))
                (void)br_u(b, 1); /* is_4 */
        }
        br_emdf_info(b);
        if (br_u(b, 1))
            (void)br_u(b, 1); /* enable */
        *ngi = 0;
        if (single) {
            unsigned gi = br_u(b, 3);
            if (gi == 7)
                gi += br_var(b, 2);
            gis[(*ngi)++] = (int)gi;
        } else {
            (void)br_u(b, 1); /* b_multi_pid */
            int nmap = -1;
            if (cfg >= 0 && cfg <= 4) {
                static const int m[5] = {2, 2, 2, 3, 3};
                nmap = m[cfg];
            }
            if (nmap > 0) {
                for (int k = 0; k < nmap; k++) {
                    unsigned gi = br_u(b, 3);
                    if (gi == 7)
                        gi += br_var(b, 2);
                    if (*ngi >= E4_MAXGRP || b->err)
                        return -1;
                    gis[(*ngi)++] = (int)gi;
                }
            } else if (cfg == 5) {
                unsigned ng = br_u(b, 2) + 2;
                if (ng == 5)
                    ng += br_var(b, 2);
                if (ng > (unsigned)E4_MAXGRP)
                    return -1;
                for (unsigned k = 0; k < ng; k++) {
                    unsigned gi = br_u(b, 3);
                    if (gi == 7)
                        gi += br_var(b, 2);
                    if (*ngi >= E4_MAXGRP || b->err)
                        return -1;
                    gis[(*ngi)++] = (int)gi;
                }
            } else {
                unsigned nb = br_u(b, 5);
                if (br_u(b, 1))
                    nb += br_var(b, 2) << 5;
                if (nb)
                    (void)br_u(b, (int)(8 * nb));
            }
        }
        (void)br_u(b, 1); /* virt */
        add_emdf = (int)br_u(b, 1);
        (void)br_u(b, 1); /* alt */
        *pndot = (int)br_u(b, 1); /* b_pres_ndot (TOC; NOT I-gate, A-80) */
        (void)br_substream_index(b);
    }
    if (add_emdf) {
        unsigned n = br_u(b, 2);
        if (n == 0)
            n = br_var(b, 2) + 4;
        if (n > 16)
            return -1;
        for (unsigned k = 0; k < n; k++)
            br_emdf_info(b);
    }
    return b->err ? -1 : 0;
}

/* One group_info. Out first-sub chan si/cm (ch path) + ajoc/obj counts.
 * frf0 = pres0 factor for ndot repeats. -1 reject. */
static int br_group(br_t *b, int fs, int frf0, int *ch0, int *si0,
                    unsigned *cm0, int *najoc, int *nobj, int *bif0,
                    unsigned *nlf0, int *nhsf) {
    *bif0 = 0;
    *nlf0 = 0;
    int present = (int)br_u(b, 1);
    int hsf = (int)br_u(b, 1);
    if (hsf)
        (*nhsf)++;
    if (e9_debug())
        fprintf(stderr, "HSF hsf=%d\n", hsf);
    unsigned nlf;
    if (br_u(b, 1)) {
        nlf = 1;
    } else {
        nlf = br_u(b, 2) + 2;
        if (nlf == 5)
            nlf += br_var(b, 2);
        if (nlf > 32)
            return -1;
    }
    int ch = (int)br_u(b, 1);
    *ch0 = ch;
    if (present)
        *nlf0 = nlf;
    if (ch) {
        for (unsigned s = 0; s < nlf; s++) {
            unsigned cm = br_channel_mode(b);
            if (cm == 252 || cm == 253 || cm == 508 || cm == 509) {
                (void)br_u(b, 1);
                (void)br_u(b, 1);
                (void)br_u(b, 2); /* presence matrix */
            }
            br_sf_br_tail(b, fs);
            if (cm >= 122 && cm <= 125)
                (void)br_u(b, 1); /* add_ch_base */
            /* Table 9 b_iframe x frame_rate_factor (was misread as
             * ndot and dropped: duet3-dec-writer fix -- the ELEMENT's
             * I-flag lives here, not in TOC b_iframe_global. First
             * subframe's bit governs our single-element decode. */
            int bif = 0;
            for (int i = 0; i < frf0; i++) {
                unsigned nb = br_u(b, 1);
                if (i == 0)
                    bif = (int)nb;
            }
            unsigned si = 0;
            if (present)
                si = br_substream_index(b);
            if (s == 0) {
                *si0 = (int)si;
                *cm0 = cm;
                *bif0 = bif;
            }
            if (hsf && present)
                (void)br_substream_index(b);
            if (b->err)
                return -1;
        }
    } else {
        if (br_u(b, 1)) { /* b_oamd_substream */
            (void)br_u(b, 1);
            if (present)
                (void)br_substream_index(b);
        }
        for (unsigned s = 0; s < nlf; s++) {
            if (br_u(b, 1)) { /* b_ajoc */
                (*najoc)++;
                (void)br_u(b, 1); /* b_lfe */
                unsigned ndmx;
                if (br_u(b, 1)) {
                    ndmx = 5; /* static */
                } else {
                    ndmx = br_u(b, 4) + 1;
                    br_bed_dyn_obj_assign(b, ndmx);
                }
                if (br_u(b, 1))
                    br_oamd_common(b);
                unsigned numx = br_u(b, 4) + 1;
                if (numx == 16)
                    numx += br_var(b, 3);
                br_bed_dyn_obj_assign(b, numx);
                br_sf_br_tail(b, fs);
                for (int i = 0; i < frf0; i++)
                    (void)br_u(b, 1);
                if (present)
                    (void)br_substream_index(b);
            } else {
                (*nobj)++;
                (void)br_u(b, 3); /* n_objects_code */
                if (br_u(b, 1)) {
                    (void)br_u(b, 1); /* b_lfe */
                } else if (br_u(b, 1)) { /* b_bed_objects */
                    if (br_u(b, 1)) {    /* b_bed_start */
                        if (br_u(b, 1)) {
                            (void)br_u(b, 3);
                        } else {
                            if (br_u(b, 1))
                                (void)br_u(b, 17);
                            else
                                (void)br_u(b, 10);
                        }
                    }
                } else {
                    if (br_u(b, 1)) { /* b_isf */
                        if (br_u(b, 1))
                            (void)br_u(b, 3);
                    } else {
                        unsigned rb = br_u(b, 4);
                        if (rb)
                            (void)br_u(b, (int)(8 * rb));
                    }
                }
                br_sf_br_tail(b, fs);
                for (int i = 0; i < frf0; i++)
                    (void)br_u(b, 1);
                if (present)
                    (void)br_substream_index(b);
            }
            if (hsf && present)
                (void)br_substream_index(b);
            if (b->err)
                return -1;
        }
    }
    if (br_u(b, 1))
        br_content_type(b);
    return b->err ? -1 : 0;
}

/* D47 iframe-flush: splice-aware sequence-counter step classifier
 * (spec/iframe-flush-scte35-flex9.md Sec. 3). Counter runs 1..1020,
 * wraps 1020->1; 0 is the splicer-marked splice frame (valid, NOT
 * a jump); a frame after 0 resumes any counter. Anything else off
 * the +1 chain is an unmarked jump (source change). */
enum { SPL_CONT = 0, SPL_WRAP, SPL_RESUME, SPL_MARKED, SPL_JUMP };
static int e_splice_step(unsigned prev, unsigned cur) {
    if (cur == 0)
        return SPL_MARKED;
    if (prev == 0)
        return SPL_RESUME;
    if (prev == 1020)
        return cur == 1 ? SPL_WRAP : SPL_JUMP;
    if (prev >= 1021)
        return SPL_JUMP; /* out-of-range prev (never emitted) */
    return cur == prev + 1 ? SPL_CONT : SPL_JUMP;
}

/* F4: peek bitstream_version of a raw frame without a full parse.
 * Returns bv (extended form included) or -1 when unreadable. */

/* Full bv2 TOC parse + structural-skip bound verify. 0 ok, -1 reject. */
static int toc_parse_bv2(const uint8_t *raw, size_t rawlen, toc_t *t,
                         int sel) {
    br_t b = {raw, rawlen * 8, 0, 0};
    memset(t, 0, sizeof *t);
    t->bed_si = -1;
    unsigned bv = br_u(&b, 2);
    if (bv == 3)
        bv += br_var(&b, 2);
    if (bv != 2)
        return -1;
    t->bv = (int)bv; /* E8 VER tag (A-79 Q5; v2 deployed, P1-v141 stale) */
    t->seq = br_u(&b, 10); /* sequence_counter (SNF reseed) */
    if (br_u(&b, 1)) {
        unsigned w = br_u(&b, 3);
        if (w > 0)
            (void)br_u(&b, 2);
    }
    int fs = (int)br_u(&b, 1);
    int rate = (int)br_u(&b, 4);
    if (rate > 13)
        return -1;
    int ig = (int)br_u(&b, 1);
    int single_pres = (int)br_u(&b, 1);
    int npres;
    if (single_pres) {
        npres = 1;
    } else {
        if (!br_u(&b, 1))
            return -1; /* empty TOC */
        npres = (int)br_var(&b, 2) + 2;
        if (npres > E4_MAXPRES)
            return -1;
    }
    if (sel >= npres) {
        t->npres = npres;
        return -2; /* sel-range: presentation index beyond stream count */
    }
    size_t pbase = 0;
    if (br_u(&b, 1)) {
        pbase = (size_t)br_u(&b, 5) + 1;
        if (pbase == 0x20)
            pbase += br_var(&b, 3);
    }
    if (br_u(&b, 1)) { /* b_program_id */
        (void)br_u(&b, 16);
        if (br_u(&b, 1))
            (void)br_u(&b, 128);
    }
    int gis[E4_MAXPRES][E4_MAXGRP];
    int ngis[E4_MAXPRES];
    int frfs[E4_MAXPRES];
    int pvers[E4_MAXPRES];
    int pndots[E4_MAXPRES];
    int maxg = -1;
    for (int i = 0; i < npres; i++) {
        frfs[i] = 1;
        if (br_pres_v1(&b, rate, gis[i], &ngis[i], &frfs[i], &pvers[i],
                       &pndots[i]) != 0)
            return -1;
        /* FINDING-1: belt-and-braces — never trust the count against
         * gis[i][E4_MAXGRP]; over-range fails loud (frame reject). */
        if (ngis[i] < 0 || ngis[i] > E4_MAXGRP)
            return -1;
        for (int k = 0; k < ngis[i]; k++)
            if (gis[i][k] > maxg)
                maxg = gis[i][k];
    }
    if (maxg < 0 || maxg >= E4_MAXGRP)
        return -1;
    /* FUZZ-DEC-1: fail loud when the selected presentation carries no
     * groups (cfg==6 EMDF-only shape). Init alone would yield
     * bed_si=-1 downstream (sizes[-1] OOB); reject instead. */
    if (ngis[sel] <= 0)
        return -1;
    int frfsel = frfs[sel];
    if (frfsel < 1 || frfsel > 4)
        return -1;
    int g_ch[E4_MAXGRP], g_si0[E4_MAXGRP];
    unsigned g_cm0[E4_MAXGRP];
    int g_bif0[E4_MAXGRP];
    unsigned g_nlf[E4_MAXGRP];
    unsigned nsub_audio = 0;
    for (int j = 0; j <= maxg; j++) {
        g_ch[j] = 0;
        g_si0[j] = -1;
        g_cm0[j] = 0;
        g_bif0[j] = 0;
        g_nlf[j] = 0;
        if (br_group(&b, fs, frfsel, &g_ch[j], &g_si0[j], &g_cm0[j],
                     &t->n_ajoc, &t->n_obj, &g_bif0[j], &g_nlf[j],
                     &t->n_hsf) != 0)
            return -1;
        nsub_audio += g_nlf[j];
    }
    t->nsub_audio = nsub_audio;
    unsigned ns = br_u(&b, 2);
    if (ns == 0)
        ns = br_var(&b, 2) + 4;
    if (ns < 1 || ns > E4_MAXNS)
        return -1;
    if (ns == 1) {
        if (!br_u(&b, 1))
            return -1; /* E4 needs sizes for skip bounds */
    }
    size_t sum = 0;
    for (unsigned s = 0; s < ns; s++) {
        unsigned more = br_u(&b, 1);
        size_t v = br_u(&b, 10);
        if (more)
            v += (size_t)br_var(&b, 2) << 10;
        t->sizes[s] = v;
        sum += v;
    }
    unsigned pad = (unsigned)((-(int64_t)b.pos) & 7);
    if (pad)
        (void)br_u(&b, (int)pad);
    if (b.err)
        return -1;
    size_t tocB = b.pos / 8;
    if (tocB + pbase + sum > rawlen)
        return -1; /* bounds fail: never overrun */
    /* bed select: pres<sel> first ch-coded group, first chan sub */
    int bed_si = -1, bed_ch = 0, bed_cm = 0, bed_bif = 0;
    for (int k = 0; k < ngis[sel]; k++) {
        int g = gis[sel][k];
        if (g < 0 || g > maxg)
            return -1;
        if (g_ch[g]) {
            bed_si = g_si0[g];
            bed_cm = (int)g_cm0[g];
            bed_bif = g_bif0[g];
            break;
        }
    }
    t->bed_obj = 0;
    if (bed_si < 0 && sel >= 0 && sel < E4_MAXPRES && ngis[sel] > 0) {
        int g0 = gis[sel][0];
        if (g0 >= 0 && g0 <= maxg)
            t->bed_obj = !g_ch[g0];
    }
    if (bed_si >= 0) {
        unsigned cm = (unsigned)bed_cm;
        if ((cm == 120 || cm == 121) && pvers[sel] == 2) {
            bed_ch = 2; /* IMS stereo core (ODK carriage rule) */
        } else if (cm == 0) {
            bed_ch = 1;
        } else if (cm == 2) {
            bed_ch = 2;
        } else if (cm == 12) {
            bed_ch = 3;
        } else if (cm == 13) {
            bed_ch = 5;
        } else if (cm == 14) {
            bed_ch = 6;
        } else if (cm == 120 || cm == 122 || cm == 124) {
            bed_ch = 7;
        } else if (cm == 121 || cm == 123 || cm == 125) {
            bed_ch = 8;
        } else if (cm == 252) {
            bed_ch = 11;
        } else if (cm == 253) {
            bed_ch = 12;
        } else if (cm == 508) {
            bed_ch = 13;
        } else if (cm == 509) {
            bed_ch = 14;
        } else if (cm >= 511) {
            bed_ch = 24; /* 22.2 ext ladder (UNVERIFIED, Q-e4-2) */
        } else {
            return -1; /* unmapped cm (510 class): reject frame */
        }
        if (bed_si < 0 || bed_si >= (int)ns)
            return -1;
        /* NOTE: wave9 AC4DEC_BEDSI substream override stripped per
         * A-test-hygiene-1 (behavior-altering probe residue). */
    }
    t->fs = fs;
    t->rate = rate;
    t->ig = ig;
    t->tocB = tocB;
    t->pbase = pbase;
    t->tail = rawlen - (tocB + pbase + sum);
    t->ns = (int)ns;
    t->npres = npres;
    t->pres_ver0 = pvers[sel];
    t->b_pres_ndot = pndots[sel];
    t->bed_si = bed_si;
    t->bed_ch = bed_ch;
    t->bed_cm = bed_cm;
    if (getenv("AC4DEC_TOCDUMP")) {
        fprintf(stderr, "TOCI seq=%u sel=%d npres=%d pversel=%d ngissel=%d",
                t->seq, sel, npres, pvers[sel], ngis[sel]);
        for (int j = 0; j <= maxg; j++)
            fprintf(stderr, " g%d=ch%d/cm%u/si%d", j, g_ch[j],
                    g_cm0[j], g_si0[j]);
        fprintf(stderr, " ns=%u bed_si=%d bed_obj=%d bed_ch=%d bed_cm=%d hsf=%d tocB=%zu pbase=%zu sizes=",
                ns, bed_si, t->bed_obj, bed_ch, bed_cm, t->n_hsf, tocB,
                pbase);
        for (unsigned s = 0; s < ns; s++)
            fprintf(stderr, "%s%zu", s ? "," : "", t->sizes[s]);
        fprintf(stderr, " ig=%d bif=%d ndot=%d\n", ig, bed_bif,
                t->b_pres_ndot);
    }
    t->bif = bed_bif;
    return 0;
}

/* Internal-grid spf per fri (spec/mdct-sizes.md S4). */
static const int e4_spf[14] = {1920, 1920, 2048, 1536, 1536, 960, 960,
                               1024, 768, 768, 512, 384, 384, 2048};
/* Oracle wav-sr label = round(spf*fps) (drop e4-3); fri13 = nominal fs. */
static const int e4_sr[14] = {46034, 46080, 51200, 46034, 46080, 46034,
                              46080, 51200, 46034, 46080, 51200, 46034,
                              46080, 0};

typedef struct {
    const uint8_t *p;
    size_t len;
} raw_t;

/* ---- wave6 e3-pcm: real SIMPLE mono/stereo ASF bed decode ----
 * Substream walk-in per ANSWER-c3-1 N1/N4 + Tables 19/20/21/23;
 * spectra via entropy.c; windows via imdct.c. One-window output
 * delay carries exact nn (next length) into every bridge; the
 * stream ends with nn=n + flush (e2 end rule).
 * Returns emitted sample-frames, or -1 with *note set (exit-3 gap).
 */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* Output gain (wave8 e3-dolby): dialnorm trim x S16 normalization.
 * trim = 10^((ref-dn)/20), ref -31 (DRP truth default); dn =
 * -dialnorm_bits/4 from substream-0 byte 0 low 7 bits (3/3 vectors
 * match DRP info: 96/96/72). /32768: chain is S16-scale, d2s16
 * needs unit (normative stage home OPEN -- ask-back to memo; output
 * placement is functionally exact). No-s0 fallback: unity. */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
 /* ctx-member */
 /* ctx-member */
/* duet6-dec-writerB 5x (stage 1: parse; stage 2: post-pass wired,
 * live once stage-1 parses every frame): separate ACPL state
 * (2 modules, not 4) + own L/R leg spill (disjoint from e3_spill,
 * no 714-path touch). */
/* ctx-member */
/* H1: e5_apf/n/cap -> ap5 ring in ctx */
/* ctx-member */
/* H1: e5_spill -> sp5 ring; oc/on are ctx members */
/* ctx-member */
/* ctx-member */
/* Immersive post-pass spill + params (mode 3, all-long). Spill holds
 * pre-trim float pushed time for D,E,F,G (mirror of lr s16); apf
 * holds parsed (a,b) per kept frame. elig ANDs to 0 on any gap
 * (skeleton output stands). Freed + reset per decode. */
/* H1: e3_spill/e3_apf -> sp3/ap3 rings; oc/on/elig are ctx members */
/* ctx-member */
/* NOTE: e3_t23ok removed with the spectral Table-23 path (stream-mix
 * era): mode-0 always mixes post-IMDCT over full streams now. */
/* duet3-dec-writerB IMS: interleave width override. 0 = nch (natural).
 * cm120-IMS decodes a 2ch core but emits 6ch (L/R + 4 zeros), matching
 * DRP's --out-ch-config 5.1 render of IMS first-hand (staged 5.1 pins:
 * C/LFE/Ls/Rs exact-0 + LIST-INFO tail artifact outside the window). */
/* ctx-member */

/* WAVE20 p2-r C(dn): per-dn trim-correction lookup, ppm.
 * dn104-TRIM-LAW (wave19 p2-bulk + memo A-p2-bulk-2-SUPP, lane-verified
 * wave20 DROP-p2-r-w20-1): OUR-vs-DRP dn->trim law diverges at entry 104
 * by -660ppm (static, lottery-proof +-5ppm; twin swan-S5 -680/-690 inside
 * +-30; sweep STAYS -660 x3 refs; DET identical). Entry value -660:
 * OUR gain runs 660ppm hot at dn104, so scale by (1-660e-6).
 * Map so far: 80->-695 / 88->0 / 92->+500 / 100->bipolar(non-trim, no entry) / 104->-660 / 116->+505 / 124->0-confirmed.
 * (WAVE21 r2-lever: + 80->-695 from memo-w21 level ladder, s5 +6dB rung
 * dn80 const 237f 0 VAR; staged eps* -695/-700, pooled-optimal -695,
 * halves -700/-675 + -700/-700; knob-validated 109363/117814 -> 19073/20322;
 * 124->0 CONFIRMED (ladder -20/-25, w20 ruling stands, no entry)).
 * TOOTH (memo A-r2-lever-2-SUPP, tooth-w22 lane-repro 5/5 digit-exact):
 * C(dn) spike-comb (A-tooth-1-SUPP: narrow wells/teeth on ~=0 baseline;
 * saw/square/linear-saw all DEAD): 80->-697 / 92->+500 / 100->-52 /
 * 104->-685 / 112->-22 / 116->+505 / 124->-22 (midpoints baseline, NOT
 * entries; comb-mapping = future rungs every 2-4 dn).
 * WAVE22 tooth: 92->+500 + 116->+505 LANDED (A-tooth-2-SUPP twins PIN:
 * P2C t92 +515 / t116 +520, D+20/+25 within +-40, adapted hygiene,
 * X-rung agree; values = round5 3-cell pool (s5ch0,s5ch1,p2c-center):
 * 92 round5(501.7), 116 round5(503.3); LICENSE GRANTED).
 * Cross-dn pooling/averaging INVALID across teeth; same-dn VALID.
 * New entries need a pinned twin + halves-hygiene before landing. */
static double e3_trim_ppm(unsigned db) {
    if (db == 104u)
        return -660.0;
    if (db == 80u)
        return -695.0;
    if (db == 92u)
        return 500.0;
    if (db == 116u)
        return 505.0;
    return 0.0;
}

/* M4 HF rolloff (wave14 e3-m4had, EXPERIMENTAL, env AC4DEC_M4=1).
 * DRP-side response: flat to bin 762, taper 763-774, mute 775+
 * (tools/hadamard/taper.tsv DRPresp_dB + dmap-final.tsv regimes;
 * LTI-confirmed zero-phase, DROP-e3-hadamard-3 S2). Spectral
 * pre-IMDCT multiply; coeff i of an n-coeff block <-> DFT bin
 * fb=(i+0.5)*1024/n (verified first-hand: h14 bin-774 tone peaks
 * coeffs 1547/1548, n=2048). Curve: fb<=762 -> 1.0; 763-774
 * linear-interp in dB between measured bin centers; fb>=775 ->
 * exact 0.0 (m2_integ MUTE-wins rule; taper.tsv 775/776 finite
 * reads = noise floor, signal absent). Edges/coefficients pending
 * memo ruling A-e3-m4had-1. Default OFF: zero behavior change. */
static const double e3_m4db[13] = {-0.20, -0.50, -1.09, -2.11, -3.70,
                                   -6.02, -9.18, -13.31, -18.52, -24.96,
                                   -32.82, -42.18, -51.54}; /* [12] = 775 edge:
                                   slope-extrapolated from 773->774 (-9.36dB);
                                   v0 -200dB sentinel overshot (see LANE). */
/* ctx-member */
/* Compensated-curve solve hook (A-e3-m4had-2(a) blessed): env
 * AC4DEC_M4CURVE=path loads 13 dB values (bins 763-774 + 775 edge),
 * overriding e3_m4db for this process. Whitespace-separated. */
/* ctx-member */
/* ctx-member */

static const double *e3_m4tab(void) {
    if (e3_m4db_ready)
        return e3_m4db_mut;
    const char *cp = getenv("AC4DEC_M4CURVE");
    if (cp) {
        FILE *f = fopen(cp, "r");
        if (f) {
            double v[13];
            int n = 0;
            while (n < 13 && fscanf(f, "%lf", &v[n]) == 1)
                n++;
            fclose(f);
            if (n == 13) {
                memcpy(e3_m4db_mut, v, sizeof v);
                e3_m4db_ready = 1;
                return e3_m4db_mut;
            }
            if (e9_show(2))
                fprintf(stderr, "ac4dec: warn M4CURVE bad (%d/13), builtin\n",
                        n);
        } else {
            if (e9_show(2))
                fprintf(stderr, "ac4dec: warn M4CURVE unreadable, builtin\n");
        }
    }
    memcpy(e3_m4db_mut, e3_m4db, sizeof e3_m4db);
    e3_m4db_ready = 1;
    return e3_m4db_mut;
}

static double e3_m4gain(double fb) {
    const double *T = e3_m4tab();
    if (fb <= 762.0)
        return 1.0;
    if (fb >= 775.0)
        return 0.0;
    if (fb < 763.0) /* (762,763): ramp 0dB -> T[0] */
        return pow(10.0, (T[0] * (fb - 762.0)) / 20.0);
    double t = fb - 763.0; /* 0..12 across bins 763..775 */
    int k = (int)t;        /* taper segment index */
    if (k < 0)
        k = 0;
    if (k > 11)
        k = 11;
    double f = t - (double)k;
    if (f < 0.0)
        f = 0.0;
    if (f > 1.0)
        f = 1.0;
    /* T[k] = bin 763+k, T[12] = 775 edge */
    double db = T[k] * (1.0 - f) + T[k + 1] * f;
    return pow(10.0, db / 20.0);
}

/* SHF short-window rolloff sweep (wave18 p2-resid, EXPERIMENTAL,
 * env AC4DEC_SHF=spec). Applies ONLY to short blocks (n<=256):
 * gain mask over coefficient lines. Spec forms (C/C0/C1 = line
 * indices, dB = attenuation):
 *   z<C>            zero lines C..n-1 (w17 qz = z96)
 *   a<C>:<dB>       attenuate lines C..n-1 by dB (flat)
 *   t<C0>:<C1>      linear taper 1->0 from C0 to C1, zero beyond
 *   d<C0>:<C1>:<dB> linear-dB taper 0->dB from C0 to C1, flat floor beyond
 *   n<C0>:<C1>      notch: zero lines C0..C1 inclusive, keep rest
 *   f<path>         curve file: 128 whitespace-separated gains for n=128
 * AC4DEC_SHFN=<maxn> caps eligible block size (default 256; 128 =
 * short-train only, transition 256s untouched). Chains AFTER M4
 * (operates on post-M4 spectra). Default OFF: zero behavior change.
 * Diagnostic only -- fit, never land as default. */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */

static void e3_shfparse(void) {
    e3_shf_parsed = 1;
    e3_shf_mode = -1;
    const char *sp = getenv("AC4DEC_SHF");
    if (!sp || !sp[0])
        return;
    char m = sp[0];
    if (m == 'z' && sscanf(sp + 1, "%d", &e3_shf_c0) == 1) {
        e3_shf_mode = 0;
    } else if (m == 'a' && sscanf(sp + 1, "%d:%lf", &e3_shf_c0,
                                  &e3_shf_db) == 2) {
        e3_shf_mode = 1;
    } else if (m == 't' && sscanf(sp + 1, "%d:%d", &e3_shf_c0,
                                  &e3_shf_c1) == 2 && e3_shf_c1 > e3_shf_c0) {
        e3_shf_mode = 2;
    } else if (m == 'd' && sscanf(sp + 1, "%d:%d:%lf", &e3_shf_c0,
                                  &e3_shf_c1, &e3_shf_db) == 3 &&
               e3_shf_c1 > e3_shf_c0) {
        e3_shf_mode = 3;
    } else if (m == 'n' && sscanf(sp + 1, "%d:%d", &e3_shf_c0,
                                  &e3_shf_c1) == 2 && e3_shf_c1 >= e3_shf_c0) {
        e3_shf_mode = 4;
    } else if (m == 'f') {
        FILE *f = fopen(sp + 1, "r");
        int n = 0;
        if (f) {
            while (n < 128 && fscanf(f, "%lf", &e3_shfcurve[n]) == 1)
                n++;
            fclose(f);
        }
        if (n == 128) {
            e3_shf_mode = 5;
        } else {
            if (e9_show(2))
                fprintf(stderr, "ac4dec: warn SHF curve bad (%d/128), off\n",
                    n);
        }
    } else {
        if (e9_show(2))
            fprintf(stderr, "ac4dec: warn SHF bad spec '%s', off\n", sp);
    }
}

static double e3_shfgain(int i) {
    switch (e3_shf_mode) {
    case 0:
        return i >= e3_shf_c0 ? 0.0 : 1.0;
    case 1:
        return i >= e3_shf_c0 ? pow(10.0, e3_shf_db / 20.0) : 1.0;
    case 2:
        if (i < e3_shf_c0)
            return 1.0;
        if (i >= e3_shf_c1)
            return 0.0;
        return 1.0 - (double)(i - e3_shf_c0) /
                         (double)(e3_shf_c1 - e3_shf_c0);
    case 3: {
        double g;
        if (i < e3_shf_c0)
            return 1.0;
        if (i >= e3_shf_c1)
            g = e3_shf_db;
        else
            g = e3_shf_db * (double)(i - e3_shf_c0) /
                (double)(e3_shf_c1 - e3_shf_c0);
        return pow(10.0, g / 20.0);
    }
    case 4:
        return (i >= e3_shf_c0 && i <= e3_shf_c1) ? 0.0 : 1.0;
    default:
        return 1.0;
    }
}

static double e3_shfgain_n(int i, int n) {
    if (e3_shf_mode == 5)
        return (n == 128 && i >= 0 && i < 128) ? e3_shfcurve[i] : 1.0;
    return e3_shfgain(i);
}

static const char *e3_push_win(int c, const double *X, int n, int nn,
                               int fam, short *dst, int *ndst) {
    if (e9_debug())
        fprintf(stderr, "PWN c=%d n=%d nn=%d\n", c, n, nn);
    double a = imdct_alpha(n, fam);
    { /* AC4DEC_KBD128=a (duet26 fork-(i-e)): override self-alpha
       * for n==128 short pushes only. Default unset = table. */
        const char *kp = getenv("AC4DEC_KBD128");
        if (kp && kp[0] && n == 128) {
            double ka = strtod(kp, NULL);
            if (ka > 0.0 && ka < 20.0)
                a = ka;
        }
    }
    { /* AC4DEC_KBD2048=a / AC4DEC_KBD1024=a (duet34 window-shape
       * discriminant): override self-alpha for n==2048 / n==1024
       * pushes. Default unset = table (3.0 / 4.0). */
        const char *k2 = getenv("AC4DEC_KBD2048");
        if (k2 && k2[0] && n == 2048) {
            double ka = strtod(k2, NULL);
            if (ka > 0.0 && ka < 20.0)
                a = ka;
        }
        const char *k1 = getenv("AC4DEC_KBD1024");
        if (k1 && k1[0] && n == 1024) {
            double ka = strtod(k1, NULL);
            if (ka > 0.0 && ka < 20.0)
                a = ka;
        }
    }
    int pn = e3_prevn[c];
    double pa = pn > 0 ? imdct_alpha(pn, fam) : a;
    double na = nn > 0 ? imdct_alpha(nn, fam) : a;
    if (a == 0.0 || pa == 0.0 || na == 0.0)
        return "imdct-alpha";
    const double *XX = X;
    { /* M4 spectral multiply (DEFAULT-ON since wave15; kill-switch
       * AC4DEC_M4=0 disables. CODUMP below still records the input X
       * (pre-M4 spectra). */
        const char *mp = getenv("AC4DEC_M4");
        int m4on = !(mp && mp[0] == '0');
        if (m4on && n <= IMDCT_MAXN && n > 0) {
            for (int i = 0; i < n; i++) {
                double fb = ((double)i + 0.5) * 1024.0 / (double)n;
                e3_m4buf[i] = X[i] * e3_m4gain(fb);
            }
            XX = e3_m4buf;
        }
    }
    { /* SHF short-window rolloff (EXPERIMENTAL, AC4DEC_SHF only).
       * Blocks n<=SHFN cap (default 256) only; chains after M4. */
        if (!e3_shf_parsed)
            e3_shfparse();
        int shfn = 256;
        const char *np = getenv("AC4DEC_SHFN");
        if (np && np[0])
            shfn = atoi(np);
        if (e3_shf_mode >= 0 && n <= shfn && n > 0 && n <= IMDCT_MAXN) {
            for (int i = 0; i < n; i++)
                e3_shfbuf[i] = XX[i] * e3_shfgain_n(i, n);
            XX = e3_shfbuf;
        }
    }
    int k = imdct_push(&e3_chst[c], XX, n, pn > 0 ? pn : 0, nn, a, pa, na,
                       e3_pcmbuf);
    /* OLADUMP (duet34 divergence-localizer, temporary): per-push IMDCT
     * block record (env AC4DEC_OLADUMP=1, stderr-only). X = post-M4/SHF
     * IMDCT input (%.17g); k = emitted samples. */
    if (getenv("AC4DEC_OLADUMP")) {
        fprintf(stderr, "OLAP c=%d n=%d pn=%d nn=%d a=%.17g k=%d X=",
                c, n, pn > 0 ? pn : 0, nn, a, k);
        for (int i = 0; i < n; i++)
            fprintf(stderr, "%.17g%c", XX[i], i + 1 < n ? ',' : '\n');
    }
    if (e3_spill_on && c >= 4 && c < 8 && k > 0) {
        /* Post-pass spill: pre-trim float mirror (mode-3 D/E/F/G),
         * absolute-indexed ring (H1: was whole-stream malloc). */
        int q = c - 4;
        ac4d_rput_d(_ac4d_self->sp3[q], AC4D_SPN, e3_spill_oc[q],
                    e3_pcmbuf, k);
        e3_spill_oc[q] += k;
    }
    if (e5_spill_on && c >= 0 && c < 2 && k > 0) {
        /* 5x post-pass spill: pre-trim float L/R legs (ring, H1). */
        ac4d_rput_d(_ac4d_self->sp5[c], AC4D_SPN, e5_spill_oc[c],
                    e3_pcmbuf, k);
        e5_spill_oc[c] += k;
    }
    { /* PCMDUMP float pcm per push (KEPT per A-test-hygiene-1). */
        const char *pp = getenv("AC4DEC_PCMDUMP");
        if (pp) {
            FILE *pf = fopen(pp, "ab");
            if (pf) {
                fwrite(&k, sizeof k, 1, pf);
                fwrite(e3_pcmbuf, sizeof(double), (size_t)(k > 0 ? k : 0),
                       pf);
                fclose(pf);
            }
        }
    }
    { /* CODUMP IMDCT input per push (wave12 M3 textbook-ref;
         * re-added under env gate per A-test-hygiene-1(b) "re-add if
         * a future cell needs one". Record: int32 seq,c,n,pn,nn,fam
         * then n doubles X. Push records (PCMDUMP k>=0, in order)
         * match CODUMP 1:1; PCMDUMP additionally carries nch trailing
         * flush records (negative k, no CODUMP row), so the Nth-record
         * shortcut is FALSE past the first flush (w18 verify-w18). */
        const char *cp = getenv("AC4DEC_CODUMP");
        if (cp) {
            FILE *cf = fopen(cp, "ab");
            if (cf) {
                static int coseq = 0;
                int hdr[6];
                hdr[0] = coseq++;
                hdr[1] = c;
                hdr[2] = n;
                hdr[3] = pn;
                hdr[4] = nn;
                hdr[5] = fam;
                fwrite(hdr, sizeof hdr, 1, cf);
                fwrite(X, sizeof(double), (size_t)n, cf);
                fclose(cf);
            }
        }
    }
    if (k < 0)
        return "imdct-push";
    for (int i = 0; i < k; i++)
        dst[i] = imdct_d2s16(e3_pcmbuf[i] * e3_dngain);
    *ndst = k;
    e3_prevn[c] = n;
    return NULL;
}

/* Mode-0 Table-23 + fold over track streams. Mixes 12 float track
 * streams (s[c][base..base+n], Table-20 already applied spectrally
 * where grids match) into fl[0..7] at oc[0..7]:
 * L = 2A, R = 2B, C = 2C, LFE, Ls = sq2(D+H'') + 2F (fold),
 * Rs = sq2(E+I'') + 2G, backs 0. Pre-gain (streaming: DN re-latch
 * stays exact; the caller applies d2s16(x * e3_dngain) at consume). */
static void e3_mix12p_f(const double *s[12], long base, long n,
                        double *fl[8], long oc[8]) {
    const double sq2 = 1.4142135623730951;
    for (long j = 0; j < n; j++) {
        double o0 = 2.0 * s[0][base + j];
        double o1 = 2.0 * s[1][base + j];
        double o2 = 2.0 * s[2][base + j];
        double o3 = s[3][base + j];
        double o4 = sq2 * (s[4][base + j] + s[8][base + j]) +
                    2.0 * s[6][base + j];
        double o5 = sq2 * (s[5][base + j] + s[9][base + j]) +
                    2.0 * s[7][base + j];
        fl[0][oc[0] + j] = o0;
        fl[1][oc[1] + j] = o1;
        fl[2][oc[2] + j] = o2;
        fl[3][oc[3] + j] = o3;
        fl[4][oc[4] + j] = o4;
        fl[5][oc[5] + j] = o5;
        fl[6][oc[6] + j] = 0.0;
        fl[7][oc[7] + j] = 0.0;
    }
    for (int c = 0; c < 8; c++)
        oc[c] += n;
}

/* variable_bits(7) extension for audio_size (N1 shape). */
static unsigned e3_var7(ac4_ebr_t *b) {
    unsigned v = 0;
    for (;;) {
        v += ebr_u(b, 7);
        if (b->err)
            return 0;
        if (!ebr_u(b, 1) || b->err)
            return v;
        v = (v << 7) + 128;
    }
}

/* LFE max_sfb wire width (own words: ETSI P1 Table 106 family rows;
 * 48k base only; 96k/192k families unmapped = honest gap). */
static int e3_msfbl_bits(int base) {
    if (base == 2048 || base == 1920 || base == 1536)
        return 3;
    if (base == 1024 || base == 960 || base == 768 || base == 384)
        return 2;
    return -1;
}

/* ---- A-SPX core (pipe4-clean-aspx) ----
 * Stereo/mono ASPX codec mode, core-PCM stage: parse companding_control
 * + aspx_config, decode the core (same stereo_data/mono_data as SIMPLE
 * per Tables 20/22), then skip aspx_data_1ch/2ch. The skip is an own
 * port of tools/aspx_walk.py Stage-2A legs (P1 Tables 49-58, Pseudocode
 * 67-70/75a-79, T118/126/189/192/194) to the ebr reader; envelope
 * codewords are validated via src/aspx_tables but values are discarded.
 * Companding gains + HF synthesis are NOT applied (core PCM = low band
 * only); DRP diffs size that remainder per vector. ETSI-literal
 * deviations from aspx_walk.py (Q1/Q2 ask memo concurrence; the
 * audio_size endpos gate arbitrates first-hand):
 *  D1 no qmode wire bit (config-assigned + FIXFIX-single override);
 *  D2 var_bord fixed 2b (Table 53 + 4.3.10.4.5 title);
 *  D3 FIXFIX mode-0 freq_res copied to all groups (Pseudocode 76);
 *  D4 xover persists across non-I frames (else derivation starves);
 *  D5 prev_stop updated every framing call, per channel (Pseudo 76);
 *  D6 VAR num_env 6/7 walked, not failed (frame-drop rule 4e belongs
 *     to the full decoder; failing whole files here would drop 49
 *     packets where the oracle drops 1).
 * State persists across frames within one e3_decode_frames call and is
 * reset at its entry (one input per process).
 * Memo hedges (ANSWER-clean-aspx-pipe4-1-PRELIM; FINAL pending): three
 * readings are one-#define flips below. Current settings = ETSI-literal
 * per the p1-v141.txt extract; the audio_size endpos gate (0..7 fill)
 * arbitrates first-hand on every corpus frame.
 */
/* Q1c: 1 = xover persists across non-I frames (D4); 0 = reset to 0. */
#define E_AX_XOVER_PERSIST 1
/* Q1d: 0 = var_bord fixed 2b, Table 53 + 4.3.10.4.5 title (D2);
 * 1 = wide/narrow (2b iff timeslots>8) per memo PRELIM. */
#define E_AX_VB_WIDE 0
/* Q1e: 1 = prev_stop updated every framing call, Pseudo-76-literal (D5);
 * 0 = VAR-side frames only per memo PRELIM. */
#define E_AX_PREV_ALWAYS 1
static const int e_ax_lo[21] = {
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 24, 26, 28,
    30, 32, 35, 38, 42, 46
};
static const int e_ax_hi[23] = {
    18, 19, 20, 21, 22, 23, 24, 26, 28, 30, 32, 34, 36, 38, 40,
    42, 44, 47, 50, 53, 56, 59, 62
};

/* frame_length_base -> num_aspx_timeslots (QMF slots / slots-per-slot). */
static int e_ax_timeslots(int flb) {
    static const int fl[8] = {2048, 1920, 1536, 1024, 960, 768, 512,
                              384};
    static const int qmf[8] = {32, 30, 24, 16, 15, 12, 8, 6};
    static const int ats[8] = {2, 2, 2, 1, 1, 1, 1, 1};
    for (int i = 0; i < 8; i++)
        if (fl[i] == flb)
            return qmf[i] / ats[i];
    return -1;
}

/* FIXFIX border rows. Returns row of num+1 entries or NULL. */
static const int *e_ax_tab_border(int ts, int num) {
    static const int b6_1[] = {0, 6};
    static const int b6_2[] = {0, 3, 6};
    static const int b6_4[] = {0, 2, 3, 4, 6};
    static const int b8_1[] = {0, 8};
    static const int b8_2[] = {0, 4, 8};
    static const int b8_4[] = {0, 2, 4, 6, 8};
    static const int b12_1[] = {0, 12};
    static const int b12_2[] = {0, 6, 12};
    static const int b12_4[] = {0, 3, 6, 9, 12};
    static const int b15_1[] = {0, 15};
    static const int b15_2[] = {0, 8, 15};
    static const int b15_4[] = {0, 4, 8, 12, 15};
    static const int b16_1[] = {0, 16};
    static const int b16_2[] = {0, 8, 16};
    static const int b16_4[] = {0, 4, 8, 12, 16};
    if (num == 1) {
        if (ts == 6)
            return b6_1;
        if (ts == 8)
            return b8_1;
        if (ts == 12)
            return b12_1;
        if (ts == 15)
            return b15_1;
        if (ts == 16)
            return b16_1;
    } else if (num == 2) {
        if (ts == 6)
            return b6_2;
        if (ts == 8)
            return b8_2;
        if (ts == 12)
            return b12_2;
        if (ts == 15)
            return b15_2;
        if (ts == 16)
            return b16_2;
    } else if (num == 4) {
        if (ts == 6)
            return b6_4;
        if (ts == 8)
            return b8_4;
        if (ts == 12)
            return b12_4;
        if (ts == 15)
            return b15_4;
        if (ts == 16)
            return b16_4;
    }
    return NULL;
}

/* ctx-moved to top (was lines 2511-2514) */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */

/* ctx-member */
/* ctx-member */
/* ctx-member */
                               /* ctx-member */
/* ctx-member */
                               /* ctx-member */
/* ctx-member */

static int e_ax_getbit(void *ctx) {
    ac4_ebr_t *b = (ac4_ebr_t *)ctx;
    if (b->err)
        return -1;
    return (int)ebr_u(b, 1);
}

/* Book select: sig 1=SIGNAL 0=NOISE; ht 0=F0 1=DF 2=DT. */
static int e_ax_booksel(int sig, int qm, int sm, int ht) {
    int fam;
    if (sig)
        fam = sm ? (qm ? 3 : 2) : (qm ? 1 : 0);
    else
        fam = sm ? 5 : 4;
    return aspx_book(fam, ht);
}

/* Master grid + endpoints. tab needs 23 ints. 0 ok, -1 bad. */
static int e_ax_master(const e_ax_cfg_t *c, int *tab, int *num,
                       int *sbz) {
    int n = c->mfscale ? 22 - 2 * c->start - 2 * c->stop
                       : 20 - 2 * c->start - 2 * c->stop;
    const int *t = c->mfscale ? e_ax_hi : e_ax_lo;
    int tlen = c->mfscale ? 23 : 21;
    int base;
    if (n < 1)
        return -1;
    base = 2 * c->start;
    if (base < 0 || base + n + 1 > tlen)
        return -1;
    for (int i = 0; i <= n; i++)
        tab[i] = t[base + i];
    *num = n;
    *sbz = tab[n];
    return 0;
}

/* Per-block group counts from the persisted/parsed xover. */
static int e_ax_counts(const e_ax_cfg_t *c, const int *tab, int num,
                       int sbz, int xover, int *hi, int *lo, int *noise) {
    int h, x, nz;
    double v;
    if (xover < 0 || xover > num)
        return -1;
    h = num - xover;
    if (h < 1)
        return -1;
    x = tab[xover];
    if (sbz <= x)
        return -1;
    v = (double)c->noisesbg * log2((double)sbz / (double)x) + 0.5;
    nz = (int)floor(v);
    if (nz < 1)
        nz = 1;
    *hi = h;
    *lo = h - h / 2;
    *noise = nz;
    return 0;
}

#define E_AX_FIXFIX 0
#define E_AX_FIXVAR 1
#define E_AX_VARFIX 2
#define E_AX_VARVAR 3

typedef struct {
    int ic, num_env, num_noise, tsg_ptr;
    int freq_res[7];
    int atsg[8];
} e_ax_fr_t;

/* Pseudocode-77 mode-2 per-group rule. */
static int e_ax_fr2(const int *atsg, int atsg_i, int tsg, int ts) {
    int dur = atsg[atsg_i + 1] - atsg[atsg_i];
    if ((atsg_i < tsg && ts > 8) ||
        (double)dur > (double)ts / 6.0 + 3.25)
        return 1;
    return 0;
}

/* One aspx_framing call. ch in {0,1} scopes prev_stop. */
/* ctx-member */
static int e_ax_framing(ac4_ebr_t *b, int ch, int iframe,
                        const e_ax_cfg_t *c, int ts, e_ax_fr_t *fr,
                        const char **note) {
    int ic, num_env = 0, tsg = 0;
    if (ebr_u(b, 1) == 0)
        ic = E_AX_FIXFIX;
    else if (ebr_u(b, 1) == 0)
        ic = E_AX_FIXVAR;
    else if (ebr_u(b, 1) == 0)
        ic = E_AX_VARFIX;
    else
        ic = E_AX_VARVAR;
    if (b->err) {
        *note = "aspx-trunc";
        return -1;
    }
    fr->ic = ic;
    if (ic == E_AX_FIXFIX) {
        int envbits = c->envbits + 1;
        unsigned tmp = ebr_u(b, envbits);
        const int *row;
        int f0 = 0;
        if (b->err) {
            *note = "aspx-trunc";
            return -1;
        }
        num_env = 1 << (int)tmp;
        if (num_env != 1 && num_env != 2 && num_env != 4) {
            *note = "aspx-badnumenv";
            return -1;
        }
        if (c->freqres == 0) {
            f0 = (int)ebr_u(b, 1);
            if (b->err) {
                *note = "aspx-trunc";
                return -1;
            }
        }
        row = e_ax_tab_border(ts, num_env);
        if (!row) {
            *note = "aspx-noflb";
            return -1;
        }
        for (int i = 0; i <= num_env; i++)
            fr->atsg[i] = row[i];
        for (int i = 0; i < num_env; i++)
            fr->freq_res[i] = f0; /* D3: single bit fans out */
    } else {
        int w = ts > 8 ? 2 : 1;
        int vbw = E_AX_VB_WIDE ? w : 2; /* Q1d hedge */
        int nrel_l = 0, nrel_r = 0, vb_l = 0, vb_r = 0;
        int rel_l[3] = {0, 0, 0}, rel_r[3] = {0, 0, 0};
        int ptr_bits = 0, v;
        if (ic == E_AX_FIXVAR) {
            vb_r = (int)ebr_u(b, vbw); /* D2: fixed 2 */
            nrel_r = (int)ebr_u(b, w);
            for (int i = 0; i < nrel_r; i++)
                rel_r[i] = 2 * (int)ebr_u(b, w) + 2;
        } else {
            if (iframe)
                vb_l = (int)ebr_u(b, vbw); /* D2: fixed 2 */
            nrel_l = (int)ebr_u(b, w);
            for (int i = 0; i < nrel_l; i++)
                rel_l[i] = 2 * (int)ebr_u(b, w) + 2;
            if (ic == E_AX_VARVAR) {
                vb_r = (int)ebr_u(b, vbw);
                nrel_r = (int)ebr_u(b, w);
                for (int i = 0; i < nrel_r; i++)
                    rel_r[i] = 2 * (int)ebr_u(b, w) + 2;
            }
        }
        if (b->err) {
            *note = "aspx-trunc";
            return -1;
        }
        num_env = nrel_l + nrel_r + 1;
        if (num_env < 1 || num_env > 7) {
            *note = "aspx-badnumenv";
            return -1;
        }
        /* ptr_bits = ceil(log2(num_env+2)) = bitlen(num_env+1). */
        v = num_env + 1;
        while (v) {
            ptr_bits++;
            v >>= 1;
        }
        tsg = (int)ebr_u(b, ptr_bits) - 1;
        if (b->err) {
            *note = "aspx-trunc";
            return -1;
        }
        if (c->freqres == 0) {
            for (int i = 0; i < num_env; i++)
                fr->freq_res[i] = (int)ebr_u(b, 1);
            if (b->err) {
                *note = "aspx-trunc";
                return -1;
            }
        }
        if (ic == E_AX_FIXVAR) {
            fr->atsg[0] = 0;
            fr->atsg[num_env] = vb_r + ts;
            for (int t = 0; t < nrel_r; t++)
                fr->atsg[num_env - t - 1] = fr->atsg[num_env - t] - rel_r[t];
        } else {
            if (iframe || !e_ax_have_prev[ch])
                fr->atsg[0] = iframe ? vb_l : 0;
            else
                fr->atsg[0] = e_ax_prev_stop[ch] - ts;
            if (ic == E_AX_VARFIX) {
                fr->atsg[num_env] = ts;
                for (int t = 0; t < nrel_l; t++)
                    fr->atsg[t + 1] = fr->atsg[t] + rel_l[t];
            } else {
                fr->atsg[num_env] = vb_r + ts;
                for (int t = 0; t < nrel_l; t++)
                    fr->atsg[t + 1] = fr->atsg[t] + rel_l[t];
                for (int t = 0; t < nrel_r; t++)
                    fr->atsg[num_env - t - 1] =
                        fr->atsg[num_env - t] - rel_r[t];
            }
        }
    }
    if (c->freqres == 1) {
        for (int i = 0; i < num_env; i++)
            fr->freq_res[i] = 0;
    } else if (c->freqres == 3) {
        for (int i = 0; i < num_env; i++)
            fr->freq_res[i] = 1;
    } else if (c->freqres == 2) {
        int tp = ic == E_AX_FIXFIX ? 0 : tsg;
        if (ic == E_AX_FIXFIX) {
            int f0 = e_ax_fr2(fr->atsg, 0, tp, ts);
            for (int i = 0; i < num_env; i++)
                fr->freq_res[i] = f0;
        } else {
            for (int i = 0; i < num_env; i++)
                fr->freq_res[i] = e_ax_fr2(fr->atsg, i, tp, ts);
        }
    }
    fr->num_env = num_env;
    fr->num_noise = num_env > 1 ? 2 : 1;
    fr->tsg_ptr = tsg;
#if E_AX_PREV_ALWAYS
    e_ax_prev_stop[ch] = fr->atsg[num_env]; /* D5: every call */
    e_ax_have_prev[ch] = 1;
#else
    /* Q1e memo rule: VAR-left frames only; FIX-left untouched. */
    if (ic == E_AX_VARFIX || ic == E_AX_VARVAR) {
        e_ax_prev_stop[ch] = fr->atsg[num_env];
        e_ax_have_prev[ch] = 1;
    }
#endif
    if (getenv("AC4DEC_AXFRAME"))
        fprintf(stderr, "AXFR f=%d ch=%d ic=%d ne=%d a0=%d aN=%d\n",
                e3_cmp_n - 1, ch, ic, num_env, fr->atsg[0],
                fr->atsg[num_env]);
    return 0;
}

static int e_ax_delta_dir(ac4_ebr_t *b, const e_ax_fr_t *fr, int *sig,
                          int *noise, const char **note) {
    for (int i = 0; i < fr->num_env; i++)
        sig[i] = (int)ebr_u(b, 1);
    for (int i = 0; i < fr->num_noise; i++)
        noise[i] = (int)ebr_u(b, 1);
    if (b->err) {
        *note = "aspx-trunc";
        return -1;
    }
    return 0;
}

static int e_ax_hfgen_1ch(ac4_ebr_t *b, int nnoise, int nhi, int ts,
                          const char **note) {
    for (int i = 0; i < nnoise; i++)
        ebr_u(b, 2);
    if (ebr_u(b, 1)) {
        for (int i = 0; i < nhi; i++)
            ebr_u(b, 1);
    }
    if (ebr_u(b, 1)) {
        for (int i = 0; i < nhi; i++)
            ebr_u(b, 1);
    }
    if (ebr_u(b, 1)) {
        for (int i = 0; i < ts; i++)
            ebr_u(b, 1);
    }
    if (b->err) {
        *note = "aspx-trunc";
        return -1;
    }
    return 0;
}

static int e_ax_hfgen_2ch(ac4_ebr_t *b, int nnoise, int nhi, int ts,
                          int balance, const char **note) {
    for (int i = 0; i < nnoise; i++)
        ebr_u(b, 2);
    if (!balance) {
        for (int i = 0; i < nnoise; i++)
            ebr_u(b, 2);
    }
    if (ebr_u(b, 1)) {
        for (int i = 0; i < nhi; i++)
            ebr_u(b, 1);
    }
    if (ebr_u(b, 1)) {
        for (int i = 0; i < nhi; i++)
            ebr_u(b, 1);
    }
    if (ebr_u(b, 1)) {
        if (ebr_u(b, 1)) {
            for (int i = 0; i < nhi; i++)
                ebr_u(b, 1);
        }
        if (ebr_u(b, 1)) {
            for (int i = 0; i < nhi; i++)
                ebr_u(b, 1);
        }
    }
    if (ebr_u(b, 1)) {
        int copy = (int)ebr_u(b, 1);
        int left = 0, right = 0;
        if (!copy) {
            left = (int)ebr_u(b, 1);
            right = (int)ebr_u(b, 1);
        }
        if (copy || left) {
            for (int i = 0; i < ts; i++)
                ebr_u(b, 1);
        }
        if (right) {
            for (int i = 0; i < ts; i++)
                ebr_u(b, 1);
        }
    }
    if (b->err) {
        *note = "aspx-trunc";
        return -1;
    }
    return 0;
}

/* One huff_data leg; codewords validated, values dropped. */
static int e_ax_huff_data(ac4_ebr_t *b, int sig, int nsbg, int qm, int sm,
                          int dir, const char **note) {
    if (dir == 0) {
        int bi = e_ax_booksel(sig, qm, sm, 0);
        if (aspx_decode(&aspx_books[bi], e_ax_getbit, b) < 0) {
            *note = "aspx-trunc";
            return -1;
        }
        bi = e_ax_booksel(sig, qm, sm, 1);
        for (int i = 1; i < nsbg; i++) {
            if (aspx_decode(&aspx_books[bi], e_ax_getbit, b) < 0) {
                *note = "aspx-trunc";
                return -1;
            }
        }
    } else {
        int bi = e_ax_booksel(sig, qm, sm, 2);
        for (int i = 0; i < nsbg; i++) {
            if (aspx_decode(&aspx_books[bi], e_ax_getbit, b) < 0) {
                *note = "aspx-trunc";
                return -1;
            }
        }
    }
    if (b->err) {
        *note = "aspx-trunc";
        return -1;
    }
    return 0;
}

static int e_ax_ec(ac4_ebr_t *b, int sig, const e_ax_fr_t *fr, int qm,
                   int sm, const int *dirs, int hi, int lo, int nnoise,
                   const char **note) {
    int nenv = sig ? fr->num_env : fr->num_noise;
    for (int e = 0; e < nenv; e++) {
        int n = sig ? (fr->freq_res[e] ? hi : lo) : nnoise;
        if (n < 1) {
            *note = "aspx-badsbg";
            return -1;
        }
        if (e_ax_huff_data(b, sig, n, qm, sm, dirs[e], note))
            return -1;
    }
    return 0;
}

/* Full aspx_data_1ch skip. xslot/chbase scope persisted state. */
static void e3_cmp_endint(int ch, int a0, int a1);
static int e_ax_data_1ch(ac4_ebr_t *b, int iframe, int ts, int xslot,
                         int chbase, char *stats, size_t stats_n,
                         const char **note) {
    int tab[23], num = 0, sbz = 0;
    int hi = 0, lo = 0, nnoise = 0, xover, qm;
    e_ax_fr_t fr;
    int sig[7], noise[2];
    if (!e_ax_have_cfg) {
        *note = "aspx-nocfg";
        return -1;
    }
    if (iframe) {
        e_ax_xover[xslot] = (int)ebr_u(b, 3);
        if (b->err) {
            *note = "aspx-trunc";
            return -1;
        }
    }
#if E_AX_XOVER_PERSIST
    xover = e_ax_xover[xslot]; /* D4: persisted */
#else
    xover = iframe ? e_ax_xover[xslot] : 0; /* Q1c alt: reset */
#endif
    if (e_ax_master(&e_ax_cfg, tab, &num, &sbz)) {
        *note = "aspx-badcfg";
        return -1;
    }
    if (e_ax_counts(&e_ax_cfg, tab, num, sbz, xover, &hi, &lo, &nnoise)) {
        *note = "aspx-badsbg";
        return -1;
    }
    memset(&fr, 0, sizeof fr);
    if (e_ax_framing(b, chbase, iframe, &e_ax_cfg, ts, &fr, note))
        return -1;
    qm = e_ax_cfg.qmode; /* D1: config, no wire bit */
    if (fr.ic == E_AX_FIXFIX && fr.num_env == 1)
        qm = 0;
    if (e_ax_delta_dir(b, &fr, sig, noise, note))
        return -1;
    if (e_ax_hfgen_1ch(b, nnoise, hi, ts, note))
        return -1;
    if (e_ax_ec(b, 1, &fr, qm, 0, sig, hi, lo, nnoise, note))
        return -1;
    if (e_ax_ec(b, 0, &fr, 0, 0, noise, hi, lo, nnoise, note))
        return -1;
    if (stats)
        snprintf(stats, stats_n, "xov%d c%d/e%d q%d", xover, fr.ic,
                 fr.num_env, qm);
    e3_cmp_endint(chbase, fr.atsg[0], fr.atsg[fr.num_env]);
    return 0;
}

/* Full aspx_data_2ch skip. xslot/chbase scope persisted state. */
static int e_ax_data_2ch(ac4_ebr_t *b, int iframe, int ts, int xslot,
                         int chbase, char *stats, size_t stats_n,
                         const char **note) {
    int tab[23], num = 0, sbz = 0;
    int hi = 0, lo = 0, nnoise = 0, xover;
    e_ax_fr_t fr0, fr1;
    int qm0, qm1, balance, sm1;
    int sig0[7], noise0[2], sig1[7], noise1[2];
    if (!e_ax_have_cfg) {
        *note = "aspx-nocfg";
        return -1;
    }
    if (iframe) {
        e_ax_xover[xslot] = (int)ebr_u(b, 3);
        if (b->err) {
            *note = "aspx-trunc";
            return -1;
        }
    }
#if E_AX_XOVER_PERSIST
    xover = e_ax_xover[xslot]; /* D4: persisted */
#else
    xover = iframe ? e_ax_xover[xslot] : 0; /* Q1c alt: reset */
#endif
    if (e_ax_master(&e_ax_cfg, tab, &num, &sbz)) {
        *note = "aspx-badcfg";
        return -1;
    }
    if (e_ax_counts(&e_ax_cfg, tab, num, sbz, xover, &hi, &lo, &nnoise)) {
        *note = "aspx-badsbg";
        return -1;
    }
    memset(&fr0, 0, sizeof fr0);
    memset(&fr1, 0, sizeof fr1);
    if (e_ax_framing(b, chbase, iframe, &e_ax_cfg, ts, &fr0, note))
        return -1;
    qm0 = qm1 = e_ax_cfg.qmode; /* D1: config, no wire bit */
    if (fr0.ic == E_AX_FIXFIX && fr0.num_env == 1)
        qm0 = qm1 = 0;
    balance = (int)ebr_u(b, 1);
    if (b->err) {
        *note = "aspx-trunc";
        return -1;
    }
    if (!balance) {
        if (e_ax_framing(b, chbase + 1, iframe, &e_ax_cfg, ts, &fr1,
                         note))
            return -1;
        qm1 = e_ax_cfg.qmode;
        if (fr1.ic == E_AX_FIXFIX && fr1.num_env == 1)
            qm1 = 0;
    } else {
        fr1 = fr0;
#if E_AX_PREV_ALWAYS
        e_ax_prev_stop[chbase + 1] = e_ax_prev_stop[chbase];
        e_ax_have_prev[chbase + 1] = 1;
#else
        if (fr0.ic == E_AX_VARFIX || fr0.ic == E_AX_VARVAR) {
            e_ax_prev_stop[chbase + 1] = e_ax_prev_stop[chbase];
            e_ax_have_prev[chbase + 1] = 1;
        }
#endif
    }
    if (e_ax_delta_dir(b, &fr0, sig0, noise0, note))
        return -1;
    if (e_ax_delta_dir(b, &fr1, sig1, noise1, note))
        return -1;
    if (e_ax_hfgen_2ch(b, nnoise, hi, ts, balance, note))
        return -1;
    sm1 = balance ? 1 : 0;
    if (e_ax_ec(b, 1, &fr0, qm0, 0, sig0, hi, lo, nnoise, note))
        return -1;
    if (e_ax_ec(b, 1, &fr1, qm1, sm1, sig1, hi, lo, nnoise, note))
        return -1;
    if (e_ax_ec(b, 0, &fr0, 0, 0, noise0, hi, lo, nnoise, note))
        return -1;
    if (e_ax_ec(b, 0, &fr1, 0, sm1, noise1, hi, lo, nnoise, note))
        return -1;
    if (stats)
        snprintf(stats, stats_n, "xov%d c%d/e%d c%d/e%d bal%d q%d/%d",
                 xover, fr0.ic, fr0.num_env, fr1.ic, fr1.num_env,
                 balance, qm0, qm1);
    e3_cmp_endint(chbase, fr0.atsg[0], fr0.atsg[fr0.num_env]);
    e3_cmp_endint(chbase + 1, fr1.atsg[0], fr1.atsg[fr1.num_env]);
    return 0;
}

/* I-frame aspx_config (split from companding: 5.X reads LFE first). */
static int e_ax_config(ac4_ebr_t *b, int iframe, const char **note) {
    if (iframe) {
        e_ax_cfg.qmode = (int)ebr_u(b, 1);
        e_ax_cfg.start = (int)ebr_u(b, 3);
        e_ax_cfg.stop = (int)ebr_u(b, 2);
        e_ax_cfg.mfscale = (int)ebr_u(b, 1);
        e_ax_cfg.interp = (int)ebr_u(b, 1);
        e_ax_cfg.preflat = (int)ebr_u(b, 1);
        e_ax_cfg.limiter = (int)ebr_u(b, 1);
        e_ax_cfg.noisesbg = (int)ebr_u(b, 2);
        e_ax_cfg.envbits = (int)ebr_u(b, 1);
        e_ax_cfg.freqres = (int)ebr_u(b, 2);
        if (b->err) {
            *note = "aspx-trunc";
            return -1;
        }
        e_ax_have_cfg = 1;
        if (e9_debug())
            fprintf(stderr,
                    "AXCFG qm=%d st=%d sp=%d mf=%d ip=%d pf=%d li=%d "
                    "ns=%d eb=%d fr=%d pos=%zu\n",
                    e_ax_cfg.qmode, e_ax_cfg.start, e_ax_cfg.stop,
                    e_ax_cfg.mfscale, e_ax_cfg.interp,
                    e_ax_cfg.preflat, e_ax_cfg.limiter,
                    e_ax_cfg.noisesbg, e_ax_cfg.envbits,
                    e_ax_cfg.freqres, b->pos);
    }
    return 0;
}

/* companding_control; values parsed and dropped (core PCM only). */
/* Persisted compand-on per channel (stereo slots 0/1; OR across frames). */
/* ctx-member */

/* Per-kept-frame companding record (mono/stereo/5x; slot f aligns with
 * kept-frame f: begin-hook appends exactly one slot per kept frame,
 * compand call overwrites onm/avg/syn, tail snapshots xover + SBG
 * config + 5x codec/m2 + intervals). Reset per decode call.
 * xov[] is xslot-indexed (stereo: [0]; 5x: [0..2]); i0/i1 are
 * wire-indexed (stereo wires = slots 0..1; 5x wires 0..4). */
/* ctx-moved to top (was lines 3082-3082) */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ASPX-interval borders in ATS units, per wire (slots = ATS x
 * num_ts_in_ats). Persist across SIMPLE frames like xover. */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* 5x parse context, snapshot per frame by the tail hook. */
/* ctx-member */
/* ctx-member */

/* E8 verb f0-latches, plain-int half (early: CMP/5X sites precede the
 * DRC types; dcfg half lives after drc_cfg_t). */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
    /* ctx-member */
/* Displayed-config census (I-gate rule; VER ncfg + interval scan). */
/* ctx-member */
/* ctx-member */

static void e3_cmp_reset(void) {
    for (int c = 0; c < 8; c++)
        e3_cmp_on[c] = 0;
    e3_cmp_n = 0;
    e3_5x_codec = 0;
    e3_5x_m2 = -1;
}

/* D47 Sec. 3 joint-rule fix (RULE-113-S3 + RULE-110-S3 DUAL-FORMED):
 * splice-side slot preserve. The companding post-pass v2 gates
 * every frame f from recorded slot f, and e3_cmp_begin() assigns
 * one slot per kept frame -- so slot index MUST stay the kept-frame
 * ordinal across a splice. e3_cmp_reset() here used to restart n
 * at 0, letting B-side frames overwrite A-side recorded slots and
 * retroactively re-process frames 0..22 with post-splice companding
 * (P1 cl 5.7.2 + 6.2.19 violation). This variant resets only the
 * live 5x parse context (B-side parse starts fresh; the per-frame
 * tail hook re-snapshots it) and preserves every recorded slot,
 * e3_cmp_n (B appends after A), and the e3_cmp_on OR run-gate
 * (whole-stream OR: an A-on/B-off splice must still run the
 * post-pass for the A frames). F1 retire-forward is untouched. */
static void e3_cmp_splice(void) {
    e3_5x_codec = 0;
    e3_5x_m2 = -1;
}

/* Append a default (off) slot; xover/cfg persist from previous slot. */
static void e3_cmp_begin(void) {
    if (e3_cmp_n >= E3_CMP_MAXF)
        return;
    int n = e3_cmp_n;
    e3_cmp_onm[n] = 0;
    e3_cmp_avg[n] = 0;
    e3_cmp_syn[n] = 0;
    e3_cmp_codec[n] = 0;
    e3_cmp_m2[n] = -1;
    if (n > 0) {
        for (int x = 0; x < 3; x++)
            e3_cmp_xov[x][n] = e3_cmp_xov[x][n - 1];
        e3_cmp_mfs[n] = e3_cmp_mfs[n - 1];
        e3_cmp_sta[n] = e3_cmp_sta[n - 1];
        e3_cmp_sto[n] = e3_cmp_sto[n - 1];
        for (int c = 0; c < 5; c++) {
            e3_cmp_i0[c][n] = e3_cmp_i0[c][n - 1];
            e3_cmp_i1[c][n] = e3_cmp_i1[c][n - 1];
        }
    } else {
        for (int x = 0; x < 3; x++)
            e3_cmp_xov[x][n] = 0;
        e3_cmp_mfs[n] = e_ax_cfg.mfscale;
        e3_cmp_sta[n] = e_ax_cfg.start;
        e3_cmp_sto[n] = e_ax_cfg.stop;
        for (int c = 0; c < 5; c++) {
            e3_cmp_i0[c][n] = 0;
            e3_cmp_i1[c][n] = -1; /* empty: falls back to full frame */
        }
    }
    e3_cmp_n = n + 1;
}

/* Snapshot post-parse xover + SBG config + 5x context into the
 * current slot. */
static void e3_cmp_endxov(void) {
    if (e3_cmp_n <= 0 || e3_cmp_n > E3_CMP_MAXF)
        return;
    int n = e3_cmp_n - 1;
    for (int x = 0; x < 3; x++)
        e3_cmp_xov[x][n] = e_ax_xover[x];
    e3_cmp_mfs[n] = e_ax_cfg.mfscale;
    e3_cmp_sta[n] = e_ax_cfg.start;
    e3_cmp_sto[n] = e_ax_cfg.stop;
    e3_cmp_codec[n] = e3_5x_codec;
    e3_cmp_m2[n] = e3_5x_m2;
}

/* Snapshot ASPX-interval ATS borders for one channel (no-op when no
 * record is open: 714 decodes never begin one). */
static void e3_cmp_endint(int ch, int a0, int a1) {
    if (ch < 0 || ch > 4)
        return;
    if (e3_cmp_n <= 0 || e3_cmp_n > E3_CMP_MAXF)
        return;
    int n = e3_cmp_n - 1;
    e3_cmp_i0[ch][n] = a0;
    e3_cmp_i1[ch][n] = a1;
}

/* Compand-channel map for one output slot. Returns the compand bit
 * index (0-based into onm) or -1 when the slot never compands. Sets
 * *wire (interval index) and *xslot. Table-212 order (A-18b):
 * compand(5)->[L,R,C,Ls,Rs], compand(3)->[L,R,C]; LFE never. The 5x
 * ASPX legs/intervals are wire-scoped (T180, m2-dependent); the
 * compand bits are channel-ordered, so bit->wire runs inverse-T180:
 * m2=0 wires [L,R,Ls,Rs,C], m2=1 wires [L,Ls,R,Rs,C]. */
static int e3_cmp_slotmap(int nch, int codec, int m2, int slot, int *wire,
                          int *xslot) {
    int ci = -1, w = -1, xs = 0;
    if (nch == 1 || nch == 2) {
        if (slot < 0 || slot >= nch)
            return -1;
        ci = slot;
        w = slot;
        xs = 0;
    } else if (nch == 6 && codec == 1) {
        static const int c2s[5] = {0, 1, 2, 4, 5};
        static const int w0[5] = {0, 1, 4, 2, 3};
        static const int w1[5] = {0, 2, 4, 1, 3};
        const int *ww = m2 == 0 ? w0 : (m2 == 1 ? w1 : NULL);
        if (!ww)
            return -1;
        for (int i = 0; i < 5; i++) {
            if (c2s[i] == slot) {
                ci = i;
                break;
            }
        }
        if (ci < 0)
            return -1;
        w = ww[ci];
        xs = w < 2 ? 0 : (w < 4 ? 1 : 2);
    } else if (nch == 6 && codec == 3) {
        if (slot < 0 || slot > 2)
            return -1;
        ci = slot;
        w = slot;
        xs = slot < 2 ? 0 : 1;
    } else {
        return -1;
    }
    *wire = w;
    *xslot = xs;
    return ci;
}

static int e_ax_compand(ac4_ebr_t *b, int nch, const char **note) {
    int sync = nch > 1 ? (int)ebr_u(b, 1) : 0;
    int nc = sync ? 1 : nch;
    int need_avg = 0;
    int onmask = 0;
    for (int i = 0; i < nc; i++) {
        int on = (int)ebr_u(b, 1);
        if (on) {
            onmask |= (1 << i);
            if (i < 8) e3_cmp_on[i] = 1;
            if (sync)
                for (int c = 0; c < nch && c < 8; c++) e3_cmp_on[c] = 1;
        } else need_avg = 1;
    }
    if (!e_cmp_f0latched) {
        e_cmp_f0latched = 1;
        e_cmp_f0nch = nch;
        e_cmp_f0sync = sync;
        e_cmp_f0mask = onmask;
        e_cmp_f0avg = need_avg;
    }
    if (e9_debug())
        fprintf(stderr, "CMP nch=%d sync=%d onmask=%x needavg=%d\n",
                nch, sync, onmask, need_avg);
    int avgbit = 0;
    if (need_avg)
        avgbit = (int)ebr_u(b, 1);
    if (b->err) {
        *note = "aspx-trunc";
        return -1;
    }
    if ((nch == 1 || nch == 2 || nch == 3 || nch == 5) && e3_cmp_n > 0 &&
        e3_cmp_n <= E3_CMP_MAXF) {
        int n = e3_cmp_n - 1;
        e3_cmp_onm[n] = (unsigned char)(onmask & 0xff);
        e3_cmp_avg[n] = (unsigned char)(avgbit & 1);
        e3_cmp_syn[n] = (unsigned char)(sync & 1);
    }
    return 0;
}

/* Element head for mono/stereo ASPX: config + companding together. */
static int e_ax_head(ac4_ebr_t *b, int iframe, int nch, const char **note) {
    if (e_ax_config(b, iframe, note))
        return -1;
    return e_ax_compand(b, nch, note);
}

/* Persisted ACPL_1ch FULL config (Table 59, I-frame head). */
/* ctx-member */

/* Wave16 p2-unstub: 5.1 SIMPLE element (own words: ETSI P1 Tables
 * 25/26 + T180 output map). Fills ch[0..5] in OUTPUT slot order
 * L R C LFE Ls Rs (Microsoft 5.1 slot guess, UNVERIFIED pending
 * QUESTION-p2-unstub-1 bed). LFE rides wire-first via mono_data(1)
 * with hand-filled long-only info; mains follow coding_config.
 * Only SIMPLE + coding_config 0 wired; every other shape is an
 * honest exit-3 gap note. Returns 0 or -1 with *note set.
 * Pipe4-clean-aspx: 5_X_codec_mode 1 (pure ASPX) shares the SIMPLE
 * core (Table 25): I-frame config, LFE, companding(5), cc0 core,
 * then aspx 2ch/2ch/1ch tails. Modes 2/3/4 (A-CPL) stay stub via
 * 5x-acpl-unimpl; 5..7 reserved. */
static int e3_decode_5x(ac4_ebr_t *b, int spf, unsigned seq, int ig,
                        unsigned asz, asf_chan_t *ch /*[6]*/,
                        asf_info_t *info /*[6]*/, const char **note) {
    unsigned codec = ebr_u(b, 3); /* 5_X_codec_mode */
    if (e9_debug())
        fprintf(stderr, "5XHEAD codec=%u ig=%d\n", codec, ig);
    e3_5x_codec = (int)codec;
    e3_5x_m2 = -1;
    int is_aspx = 0;
    e5_last_acpl = 0;
    if (b->err) {
        *note = "5x-trunc";
        return -1;
    }
    if (codec == 1) {
        is_aspx = 1;
        if (e_ax_config(b, ig, note))
            return -1;
    } else if (codec == 0) {
        ;
    } else if (codec == 3) {
        /* ASPX_ACPL_2 MINIMAL (Table 25 + 5.3.4.3.2/Table 181 + Pseu 117):
         * parse everything (endpos-clean), render L/R/C live via the
         * Table-181 map, Ls/Rs SILENT (QMF A-CPL synthesis not wired;
         * honest partial -- scored, parked as acpl-qmf-unimpl gap). */
        is_aspx = 1; /* s52: codec-3 rides the ASPX bit-path (bank
                         @e8cb1a5): shared post-LFE companding(3) +
                         ASPX tails. Dropped in the d036544 LFE-share
                         reshape -> SFINFO read 3 bits early
                         (sfinfo-fail-5x, 51 DONE-path red). */
        if (ig) {
            /* Table-25 I-frame order: aspx_config then acpl_config_1ch(FULL)
             * (Table 59: id(2)+qm(1); FULL keeps band 0; id->{15,12,9,7}). */
            if (e_ax_config(b, ig, note))
                return -1;
            static const int NB[4] = {15, 12, 9, 7};
            int id = (int)ebr_u(b, 2);
            int aqm = (int)ebr_u(b, 1);
            e3_acpl_qm = aqm;
            if (b->err) { *note = "5x-trunc"; return -1; }
            e3_acpl_nb = NB[id & 3];
            e3_acpl_st = 0;
            e5_acpl_nbands = NB[id & 3]; /* s52: codec-3 acpl-data
                                             consumer below reads e5_* */
            e5_acpl_fine = aqm ? 0 : 1;
            e5_acpl_have = 1;
            if (e9_debug())
                fprintf(stderr, "ACPLCFG5 seq=%u abid=%d aqm=%d\n",
                        seq, id, aqm);
        }
        e5_need_postpass = 1; /* s52: bank set this for codec-3 (stage 2) */
        /* fall through to shared LFE below, then ACPL_2 core label sorts
         * codec==3 (ACPL_2) from codec 0/1 (cc0 core). No goto: LFE shared. */
    } else if (codec <= 4) {
        if (codec != 3) {
            if (e9_debug())
                fprintf(stderr, "5XHEAD codec=%u\n", codec);
            *note = "5x-acpl-unimpl";
            return -1;
        }
        is_aspx = 1;
        if (e_ax_config(b, ig, note))
            return -1;
        if (ig) {
            /* acpl_config_1ch(FULL) mirrored from 714 (Table 59);
             * P-frames inherit. Q-M14 cites exact 5x syntax.
             * AB-17(d): FULL/PARTIAL is context-derived (Table 59
             * NOTE): codec-3 = ACPL_2 = FULL; PARTIAL is ACPL_1
             * (codec-2, stubbed before config). Zero bits read. */
            unsigned abid = ebr_u(b, 2);
            unsigned aqm = ebr_u(b, 1);
            static const int abn[4] = {15, 12, 9, 7};
            e5_acpl_nbands = abn[abid & 3];
            e5_acpl_fine = aqm ? 0 : 1;
            e5_acpl_have = 1;
            if (e9_debug())
                fprintf(stderr, "ACPLCFG5 seq=%u abid=%u aqm=%u\n",
                        seq, abid, aqm);
            if (b->err) {
                *note = "5x-trunc";
                return -1;
            }
        }
        e5_need_postpass = 1; /* stage 1: parse only (stub output) */
        if (e9_debug())
            fprintf(stderr, "5XHEAD codec=%u (acpl, nobypass)\n", codec);
    } else {
        *note = "5x-codec-reserved";
        return -1;
    }
    /* LFE first: sf_info_lfe = long-only + max_sfb in msfbl bits. */
    int mw = e3_msfbl_bits(spf);
    if (mw < 0) {
        *note = "5x-lfe-width";
        return -1;
    }
    unsigned lms = ebr_u(b, mw);
    if (e9_debug())
        fprintf(stderr, "5XLFE lms=%u mw=%d pos=%zu\n", lms, mw, b->pos);
    if (b->err) {
        *note = "5x-trunc";
        return -1;
    }
    memset(&info[3], 0, sizeof info[3]);
    info[3].b_long = 1;
    info[3].max_sfb[0] = (int)lms;
    info[3].max_sfb[1] = (int)lms;
    info[3].num_windows = 1;
    info[3].num_groups = 1;
    info[3].win_to_group[0] = 0;
    info[3].num_win_in_group[0] = 1;
    info[3].win_len[0] = spf;
    info[3].grp_tlidx[0] = 4;
    int r = asf_chan_decode(b, &info[3], spf, seq, &ch[3]);
    if (r == AC4E_SNF) {
        *note = "snf-unimpl";
        return -1;
    }
    if (r != AC4E_OK) {
        *note = "entropy-fail-lfe";
        return -1;
    }
    if (is_aspx) {
        /* AB-14: companding_control width is mode-driven (Table 25):
         * SIMPLE/ASPX (5), ASPX_ACPL_1/2 (3). */
        if (e_ax_compand(b, codec == 3 ? 3 : 5, note))
            return -1;
    }
    /* AB-14: coding_config is 1 BIT on ACPL_1/2 (Table 25 `if
     * (coding_config)`), 2 bits on SIMPLE/ASPX. */
    unsigned cc = codec == 3 ? ebr_u(b, 1) : ebr_u(b, 2);
    if (b->err) {
        *note = "5x-trunc";
        return -1;
    }
    if (cc != 0 && codec != 3) {
        *note = cc == 1 ? "5x-cc1-unimpl"
              : cc == 2 ? "5x-cc2-unimpl"
                        : "5x-cc3-unimpl";
        return -1;
    }
    unsigned m2 = 0;
    unsigned matsel = 0;
    int mdct[2] = {0, 0};
    int is_3ch = (codec == 3 && cc == 1);
    int is_acpl3 = (codec == 3);
    if (!is_acpl3) {
        m2 = ebr_u(b, 1); /* 2ch_mode: pair->speaker map
                             * (SIMPLE/ASPX only; no m2 on ACPL
                             * paths -- AB-14 literal, QB-21). */
        if (b->err) {
            *note = "5x-trunc";
            return -1;
        }
    }
    if (is_3ch) {
        /* three_channel_data (Table 27, AB-14): ONE sf_info shared
         * L/R/C + matsel 4b + 2x chparam + 3x sf_data. Slots
         * [0,1,2] = L,R,C ASSUMED (QB-21); coupling apply TBD
         * (topology QB-21) -- parse only for alignment. */
        r = asf_info_parse(b, spf, &info[0]);
        if (r != AC4E_OK) {
            *note = "sfinfo-fail-5x3";
            return -1;
        }
        info[1] = info[0];
        info[2] = info[0];
        matsel = ebr_u(b, 4);
        if (b->err) {
            *note = "5x-trunc";
            return -1;
        }
        /* ctx-moved: static sap5b -> xm_sap5b */
        r = ac4e_chparam_parse(b, &info[0], &(_ac4d_self->xm_sap5a));
        if (r != AC4E_OK) {
            *note = "chparam-fail-5x3a";
            return -1;
        }
        r = ac4e_chparam_parse(b, &info[0], &(_ac4d_self->xm_sap5b));
        if (r != AC4E_OK) {
            *note = "chparam-fail-5x3b";
            return -1;
        }
        for (int s = 0; s < 3; s++) {
            r = asf_chan_decode(b, &info[s], spf, seq, &ch[s]);
            if (r != AC4E_OK) {
                *note = r == AC4E_SNF ? "snf-unimpl"
                                      : "entropy-fail-5x3";
                return -1;
            }
        }
        if (e9_debug())
            fprintf(stderr, "5X3CH seq=%u matsel=%u\n", seq, matsel);
    } else if (is_acpl3) {
        /* codec-3 cc==0 (AB-14 literal): ONE two_channel_data ->
         * slots 0,1 (L,R ASSUMED, QB-21); mono_data(0) below
         * covers slot 2 (C). mdct[k] stereo flag as usual. */
        mdct[0] = (int)ebr_u(b, 1);
        if (b->err) {
            *note = "5x-trunc";
            return -1;
        }
        if (mdct[0]) {
            r = asf_info_parse(b, spf, &info[0]);
            if (r != AC4E_OK) {
                *note = "sfinfo-fail-5x";
                return -1;
            }
            info[1] = info[0];
            /* ctx-moved: static sap5c0 -> xm_sap5c0 */
            r = ac4e_chparam_parse(b, &info[0], &(_ac4d_self->xm_sap5c0));
            if (r != AC4E_OK) {
                *note = "chparam-fail-5x";
                return -1;
            }
            for (int s = 0; s < 2; s++) {
                r = asf_chan_decode(b, &info[s], spf, seq, &ch[s]);
                if (r != AC4E_OK) {
                    *note = r == AC4E_SNF ? "snf-unimpl"
                                          : "entropy-fail-5x";
                    return -1;
                }
            }
            ac4e_sap_apply(&info[0], &(_ac4d_self->xm_sap5c0), &ch[0], &ch[1]);
        } else {
            for (int s = 0; s < 2; s++) {
                r = asf_info_parse(b, spf, &info[s]);
                if (r != AC4E_OK) {
                    *note = "sfinfo-fail-5x";
                    return -1;
                }
            }
            for (int s = 0; s < 2; s++) {
                r = asf_chan_decode(b, &info[s], spf, seq, &ch[s]);
                if (r != AC4E_OK) {
                    *note = r == AC4E_SNF ? "snf-unimpl"
                                          : "entropy-fail-5x";
                    return -1;
                }
            }
        }
    }
    if (!is_acpl3) {
    e3_5x_m2 = (int)(m2 & 1);
    /* T180 config-0 rows: wire [t0..t4] -> out slots; LFE = slot 3. */
    static const int slot0[5] = {0, 1, 4, 5, 2}; /* m2=0 [L,R,Ls,Rs,C] */
    static const int slot1[5] = {0, 4, 1, 5, 2}; /* m2=1 [L,Ls,R,Rs,C] */
    const int *slot = m2 ? slot1 : slot0;
    for (int k = 0; k < 2; k++) { /* two_channel_data x2, pinned ASF */
        int s0 = slot[2 * k], s1 = slot[2 * k + 1];
        mdct[k] = (int)ebr_u(b, 1);
        if (b->err) {
            *note = "5x-trunc";
            return -1;
        }
        if (mdct[k]) {
            r = asf_info_parse(b, spf, &info[s0]);
            if (r != AC4E_OK) {
                *note = "sfinfo-fail-5x";
                return -1;
            }
            info[s1] = info[s0];
            /* ctx-moved: static sap5x -> xm_sap5x */
            r = ac4e_chparam_parse(b, &info[s0], &(_ac4d_self->xm_sap5x));
            if (r != AC4E_OK) {
                *note = "chparam-fail-5x";
                return -1;
            }
            r = asf_chan_decode(b, &info[s0], spf, seq, &ch[s0]);
            if (r != AC4E_OK) {
                *note = r == AC4E_SNF ? "snf-unimpl"
                                      : "entropy-fail-5x";
                return -1;
            }
            r = asf_chan_decode(b, &info[s1], spf, seq, &ch[s1]);
            if (r != AC4E_OK) {
                *note = r == AC4E_SNF ? "snf-unimpl"
                                      : "entropy-fail-5x";
                return -1;
            }
            ac4e_sap_apply(&info[s0], &(_ac4d_self->xm_sap5x), &ch[s0], &ch[s1]);
        } else {
            for (int q = 0; q < 2; q++) {
                int s = q ? s1 : s0;
                r = asf_info_parse(b, spf, &info[s]);
                if (r != AC4E_OK) {
                    *note = "sfinfo-fail-5x";
                    return -1;
                }
            }
            for (int q = 0; q < 2; q++) {
                int s = q ? s1 : s0;
                r = asf_chan_decode(b, &info[s], spf, seq, &ch[s]);
                if (r != AC4E_OK) {
                    *note = r == AC4E_SNF ? "snf-unimpl"
                                          : "entropy-fail-5x";
                    return -1;
                }
            }
        }
    }
    } /* end !is_acpl3 */
    if (!is_3ch) { /* mono_data(0): wire t4 -> C slot 2 (skipped on
                    * 3ch: NO mono element, AB-14) */
        if (ebr_u(b, 1) != 0) {
            *note = "ssf-unimpl";
            return -1;
        }
        r = asf_info_parse(b, spf, &info[2]);
        if (r != AC4E_OK) {
            *note = "sfinfo-fail-5x";
            return -1;
        }
        r = asf_chan_decode(b, &info[2], spf, seq, &ch[2]);
        if (r == AC4E_SNF) {
            *note = "snf-unimpl";
            return -1;
        }
        if (r != AC4E_OK) {
            *note = "entropy-fail-5x";
            return -1;
        }
    }
    if (is_aspx) {
        char s0[64], s1[64], s2[64];
        int ax_ts = e_ax_timeslots(spf);
        size_t ax_end;
        s0[0] = s1[0] = s2[0] = '\0';
        if (ax_ts < 0) {
            *note = "aspx-noflb";
            return -1;
        }
        /* AB-14(c): ACPL_1/2 tails carry ONE 2ch + ONE 1ch (codec-1
         * keeps 2ch/2ch/1ch). Tails xslot/chbase on ACPL paths
         * ASSUMED sequential (QB-21). */
        if (e_ax_data_2ch(b, ig, ax_ts, 0, 0, s0, sizeof s0, note))
            return -1;
        if (!is_acpl3) {
            if (e_ax_data_2ch(b, ig, ax_ts, 1, 2, s1, sizeof s1, note))
                return -1;
            if (e_ax_data_1ch(b, ig, ax_ts, 2, 4, s2, sizeof s2, note))
                return -1;
        } else {
            if (e_ax_data_1ch(b, ig, ax_ts, 1, 2, s2, sizeof s2, note))
                return -1;
        }
        ax_end = ((size_t)asz + 2) * 8;
        if (codec == 1 &&
            (b->err || b->pos > ax_end || ax_end - b->pos > 7)) {
            *note = "aspx-endpos";
            return -1;
        }
        if (e9_debug() && ig)
            fprintf(stderr,
                    "ASPXCFG5 q%d st%d/sp%d m%d nb%d eb%d fr%d\n",
                    e_ax_cfg.qmode, e_ax_cfg.start, e_ax_cfg.stop,
                    e_ax_cfg.mfscale, e_ax_cfg.noisesbg, e_ax_cfg.envbits,
                    e_ax_cfg.freqres);
        if (e9_debug())
            fprintf(stderr, "ASPX5 ig=%d [%s] [%s] [%s] gap=%zu\n", ig,
                    s0, s1, s2, ax_end - b->pos);
    }
    if (codec == 3) {
        /* ACPL data (5x mode 3): 2x acpl_data_1ch (AB-5), parsed +
         * stored per frame for the 5x post-pass (stage 2). Tails
         * never gate (mirror 714). Q-M14 cites exact syntax. */
        int ok5 = 1;
        acpl_mod_t pm5[2];
        for (int m = 0; m < 2 && ok5; m++) {
            const char *an5 = NULL;
            if (acpl_parse_1ch(b, e5_acpl_nbands, e5_acpl_fine, m,
                               &pm5[m], &an5, &_ac4d_self->acpl))
                ok5 = 0;
        }
        if (ok5) {
            /* H1: record into the tagged apf ring (was whole-stream
             * array). fno = kept-frame ordinal at step time. */
            long fno = _ac4d_self->h_fed;
            int slot = (int)(fno % AC4D_APFN);
            for (int m = 0; m < 2; m++)
                _ac4d_self->ap5[slot].m[m] = pm5[m];
            _ac4d_self->ap5[slot].fno = fno;
            _ac4d_self->ap5[slot].have = 1;
            e5_last_acpl = 1;
        }
        if (getenv("AC4DEC_ACPLPROBE")) {
            for (int m = 0; m < 2; m++) {
                fprintf(stderr, "5XACPL f=%u m=%d np=%d it=%d\n",
                        seq, m, pm5[m].npset, pm5[m].itype);
                for (int ps = 0; ps < pm5[m].npset; ps++) {
                    fprintf(stderr, "  a:");
                    for (int i = 0; i < pm5[m].nbands; i++)
                        fprintf(stderr, " %.3f", pm5[m].a[ps][i]);
                    fprintf(stderr, "\n  b:");
                    for (int i = 0; i < pm5[m].nbands; i++)
                        fprintf(stderr, " %.4f", pm5[m].b[ps][i]);
                    fprintf(stderr, "\n");
                }
            }
            fprintf(stderr, "5XACPLGAP f=%u gap=%zu ok=%d\n", seq,
                    b->nbits - b->pos, ok5);
        }
        if (!ok5) {
            *note = "5x-acpl-unimpl";
            return -1;
        }
        /* Codec-3 core is 3-wire (L,R,C): Ls/Rs (slots 4,5) ride
         * silent skeleton (long, zero spectra; the 5XPOST pass
         * overwrites lr[4,5] wholesale). Info mirrors slot 0 so
         * the push stage sees valid long-block grouping. */
        info[4] = info[0];
        info[5] = info[0];
        memset(&ch[4], 0, sizeof ch[4]);
        memset(&ch[5], 0, sizeof ch[5]);
        ch[4].nwin = 1;
        ch[4].win_len[0] = spf;
        ch[5].nwin = 1;
        ch[5].win_len[0] = spf;
    }
    if (!e_5x_f0latched) {
        e_5x_f0latched = 1;
        e_5x_codec = codec;
        e_5x_cc = cc;
        e_5x_m2 = m2;
        e_5x_mdct0 = mdct[0];
        e_5x_mdct1 = mdct[1];
        e_5x_ms = (int)matsel;
        e_5x_lms = lms;
        e_5x_3ch = is_3ch ? 1 : 0;
    }
    if (e9_debug()) {
        fprintf(stderr, "5X codec=%u cc=%u m2=%u mdct=%d/%d ms=%u%s lfems=%u",
                codec, cc, m2, mdct[0], mdct[1], matsel,
                is_3ch ? " 3ch" : "", lms);
        for (int c = 0; c < 6; c++)
            fprintf(stderr, " ch%d=%c%dw%dg", c,
                    info[c].b_long ? 'L' : 'S', info[c].num_windows,
                    info[c].num_groups);
        fprintf(stderr, "\n");
    }
    if (b->err) {
        *note = "elem-trunc-5x";
        return -1;
    }
    e3_cmp_endxov();
    return 0;
}

/* duet3-dec-writerB 714: immersive 7.1.4 core (own words: ETSI P2
 * 6.2.4.1 + 5.2.3.2/5.2.3.3 + Tables 19/20/73-75). Decodes the 7.1
 * mains (LFE + 5ch core + Lb/Rb pair) into ch[0..7] (out slots
 * L R C LFE Ls Rs Lb Rb); height data (SCPL pairs / ACPL tails) is
 * NEVER consumed (Table 20: mains don't depend on heights).
 * Modes 0 SCPL + 3 ASPX_ACPL_2 (observed); 1/2/4 = honest gap.
 * Grouping 0 only (1/2/3 = honest gap: three/four/five-wide blocks
 * unlanded). STOPs after the Lb/Rb pair (mode 0) or ASPX legs
 * (mode 3); tails unread by design (no endpos gate).
 * Returns immersive_codec_mode (0..4) or -1 with *note set. */
static int e3_decode_714(ac4_ebr_t *b, int spf, unsigned seq, int ig,
                         asf_chan_t *ch /*[8]*/,
                         asf_info_t *info /*[8]*/, const char **note) {
    unsigned m1 = ebr_u(b, 1); /* immersive_codec_mode_code (Table 73) */
    if (b->err) {
        *note = "714-trunc";
        return -1;
    }
    unsigned mode = m1 ? 4 : ebr_u(b, 2);
    if (b->err) {
        *note = "714-trunc";
        return -1;
    }
    if (getenv("AC4DEC_714MODE"))
        fprintf(stderr, "714MODE seq=%u ig=%d mode=%u\n", seq, ig, mode);
    int is_aspx = 0;
    if (mode == 0) {
        ;
    } else if (mode == 3) {
        is_aspx = 1;
        if (e_ax_config(b, ig, note))
            return -1;
        if (ig) {
            /* acpl_config_1ch(FULL): 2b band-id + 1b quant (Table 59:
             * bands_id -> 15/12/9/7, quant 0 = fine). Stored for the
             * ACPL data probe/port (P-frames inherit). AB-17(d): the
             * FULL/PARTIAL mode is context-derived, never signalled
             * (Table 59 NOTE helper): this ACPL_2-class path is FULL;
             * PARTIAL exists only on ACPL_1 (5x codec-2, stubbed
             * before any config parse). Zero bits read. */
            unsigned abid = ebr_u(b, 2);
            unsigned aqm = ebr_u(b, 1);
            static const int abn[4] = {15, 12, 9, 7};
            if (!e3_acpl_have &&
                (abn[abid & 3] != 15 || aqm != 0))
                e3_imm_elig = 0; /* pre-I default mismatch: reparse gap */
            e3_acpl_nbands = abn[abid & 3];
            e3_acpl_fine = aqm ? 0 : 1;
            e3_acpl_have = 1;
            if (e9_debug())
                fprintf(stderr, "ACPLCFG seq=%u abid=%u aqm=%u\n",
                        seq, abid, aqm);
            if (b->err) {
                *note = "714-trunc";
                return -1;
            }
        }
    } else {
        *note = "714-mode-unimpl";
        return -1;
    }
    /* LFE first: sf_info_lfe = long-only + max_sfb in msfbl bits. */
    int mw = e3_msfbl_bits(spf);
    if (mw < 0) {
        *note = "714-lfe-width";
        return -1;
    }
    unsigned lms = ebr_u(b, mw);
    if (b->err) {
        *note = "714-trunc";
        return -1;
    }
    memset(&info[3], 0, sizeof info[3]);
    info[3].b_long = 1;
    info[3].max_sfb[0] = (int)lms;
    info[3].max_sfb[1] = (int)lms;
    info[3].num_windows = 1;
    info[3].num_groups = 1;
    info[3].win_to_group[0] = 0;
    info[3].num_win_in_group[0] = 1;
    info[3].win_len[0] = spf;
    info[3].grp_tlidx[0] = 4;
    int r = asf_chan_decode(b, &info[3], spf, seq, &ch[3]);
    if (r == AC4E_SNF) {
        *note = "snf-unimpl";
        return -1;
    }
    if (r != AC4E_OK) {
        *note = "entropy-fail-lfe";
        return -1;
    }
    /* (no companding_control: AJCC-only per 6.2.4.1.) */
    unsigned grp = ebr_u(b, 2); /* core_5ch_grouping */
    if (b->err) {
        *note = "714-trunc";
        return -1;
    }
    if (grp != 0) {
        *note = "714-grp-unimpl";
        return -1;
    }
    unsigned m2 = ebr_u(b, 1); /* 2ch_mode: pair->speaker map */
    if (b->err) {
        *note = "714-trunc";
        return -1;
    }
    /* Table 19 grouping-0 rows: wire [t0..t4] -> out slots; LFE = 3. */
    static const int slot0[5] = {0, 1, 4, 5, 2}; /* m2=0 [L,R,Ls,Rs,C] */
    static const int slot1[5] = {0, 4, 1, 5, 2}; /* m2=1 [L,Ls,R,Rs,C] */
    const int *slot = m2 ? slot1 : slot0;
    /* ctx-moved: static sap7x -> xm_sap7x */
    int mdct[2] = {0, 0};
    for (int k = 0; k < 2; k++) { /* two_channel_data x2, pinned ASF */
        int s0 = slot[2 * k], s1 = slot[2 * k + 1];
        mdct[k] = (int)ebr_u(b, 1);
        if (b->err) {
            *note = "714-trunc";
            return -1;
        }
        if (mdct[k]) {
            r = asf_info_parse(b, spf, &info[s0]);
            if (r != AC4E_OK) {
                *note = "714-sfinfo";
                return -1;
            }
            info[s1] = info[s0];
            r = ac4e_chparam_parse(b, &info[s0], &(_ac4d_self->xm_sap7x));
            if (r != AC4E_OK) {
                *note = "714-chparam";
                return -1;
            }
            r = asf_chan_decode(b, &info[s0], spf, seq, &ch[s0]);
            if (r != AC4E_OK) {
                *note = r == AC4E_SNF ? "snf-unimpl"
                                      : "714-entropy";
                return -1;
            }
            r = asf_chan_decode(b, &info[s1], spf, seq, &ch[s1]);
            if (r != AC4E_OK) {
                *note = r == AC4E_SNF ? "snf-unimpl"
                                      : "714-entropy";
                return -1;
            }
            ac4e_sap_apply(&info[s0], &(_ac4d_self->xm_sap7x), &ch[s0], &ch[s1]);
        } else {
            for (int q = 0; q < 2; q++) {
                int s = q ? s1 : s0;
                r = asf_info_parse(b, spf, &info[s]);
                if (r != AC4E_OK) {
                    *note = "714-sfinfo";
                    return -1;
                }
            }
            for (int q = 0; q < 2; q++) {
                int s = q ? s1 : s0;
                r = asf_chan_decode(b, &info[s], spf, seq, &ch[s]);
                if (r != AC4E_OK) {
                    *note = r == AC4E_SNF ? "snf-unimpl"
                                          : "714-entropy";
                    return -1;
                }
            }
        }
    }
    { /* mono_data(0): wire t4 -> C slot 2 */
        if (ebr_u(b, 1) != 0) {
            *note = "ssf-unimpl";
            return -1;
        }
        r = asf_info_parse(b, spf, &info[2]);
        if (r != AC4E_OK) {
            *note = "714-sfinfo";
            return -1;
        }
        r = asf_chan_decode(b, &info[2], spf, seq, &ch[2]);
        if (r == AC4E_SNF) {
            *note = "snf-unimpl";
            return -1;
        }
        if (r != AC4E_OK) {
            *note = "714-entropy";
            return -1;
        }
    }
    /* 7CH_STATIC extra: Lb/Rb pair -> slots 6/7. b_use_sap_add_ch=1
     * (coupled against Ls/Rs per 5.2.3.2 steps 3-4) is an honest gap:
     * unobserved (514 content carries Lb/Rb silence, flag expected 0). */
    unsigned u = ebr_u(b, 1);
    if (b->err) {
        *note = "714-trunc";
        return -1;
    }
    if (u) {
        *note = "714-addch-unimpl";
        return -1;
    }
    int mdct2 = (int)ebr_u(b, 1);
    if (b->err) {
        *note = "714-trunc";
        return -1;
    }
    if (mdct2) {
        r = asf_info_parse(b, spf, &info[6]);
        if (r != AC4E_OK) {
            *note = "714-sfinfo";
            return -1;
        }
        info[7] = info[6];
        r = ac4e_chparam_parse(b, &info[6], &(_ac4d_self->xm_sap7x));
        if (r != AC4E_OK) {
            *note = "714-chparam";
            return -1;
        }
        r = asf_chan_decode(b, &info[6], spf, seq, &ch[6]);
        if (r != AC4E_OK) {
            *note = r == AC4E_SNF ? "snf-unimpl" : "714-entropy";
            return -1;
        }
        r = asf_chan_decode(b, &info[7], spf, seq, &ch[7]);
        if (r != AC4E_OK) {
            *note = r == AC4E_SNF ? "snf-unimpl" : "714-entropy";
            return -1;
        }
        ac4e_sap_apply(&info[6], &(_ac4d_self->xm_sap7x), &ch[6], &ch[7]);
    } else {
        for (int q = 6; q < 8; q++) {
            r = asf_info_parse(b, spf, &info[q]);
            if (r != AC4E_OK) {
                *note = "714-sfinfo";
                return -1;
            }
        }
        for (int q = 6; q < 8; q++) {
            r = asf_chan_decode(b, &info[q], spf, seq, &ch[q]);
            if (r != AC4E_OK) {
                *note = r == AC4E_SNF ? "snf-unimpl" : "714-entropy";
                return -1;
            }
        }
    }
    /* Mode-0 (SCPL) extension: 4th + 5th pairs (H,I,J,K per Table 19)
     * + 4 trailing chparams (Table-20 a'0..a'3). b_5fronts = 0 (514:
     * 11 tracks, no 6th pair). Internal tracks (module inputs), not
     * outputs. PROBE MODE: parse + print, Table-20 pending. */
    /* ctx-moved: static asq -> xm_asq */
    int tmdct[2] = {0, 0};
    if (mode == 0) {
        for (int k = 0; k < 2; k++) {
            int s0 = 8 + 2 * k, s1 = 8 + 2 * k + 1;
            tmdct[k] = (int)ebr_u(b, 1);
            if (b->err) {
                *note = "714-trunc";
                return -1;
            }
            if (tmdct[k]) {
                r = asf_info_parse(b, spf, &info[s0]);
                if (r != AC4E_OK) {
                    *note = "714-sfinfo-T";
                    return -1;
                }
                info[s1] = info[s0];
                r = ac4e_chparam_parse(b, &info[s0], &(_ac4d_self->xm_tsap)[k]);
                if (r != AC4E_OK) {
                    *note = "714-chparam-T";
                    return -1;
                }
                r = asf_chan_decode(b, &info[s0], spf, seq, &ch[s0]);
                if (r != AC4E_OK) {
                    *note = r == AC4E_SNF ? "snf-unimpl" : "714-entropy-T";
                    return -1;
                }
                r = asf_chan_decode(b, &info[s1], spf, seq, &ch[s1]);
                if (r != AC4E_OK) {
                    *note = r == AC4E_SNF ? "snf-unimpl" : "714-entropy-T";
                    return -1;
                }
                ac4e_sap_apply(&info[s0], &(_ac4d_self->xm_tsap)[k], &ch[s0], &ch[s1]);
            } else {
                for (int q = 0; q < 2; q++) {
                    int s = q ? s1 : s0;
                    r = asf_info_parse(b, spf, &info[s]);
                    if (r != AC4E_OK) {
                        *note = "714-sfinfo-T";
                        return -1;
                    }
                }
                for (int q = 0; q < 2; q++) {
                    int s = q ? s1 : s0;
                    r = asf_chan_decode(b, &info[s], spf, seq, &ch[s]);
                    if (r != AC4E_OK) {
                        *note = r == AC4E_SNF ? "snf-unimpl" : "714-entropy-T";
                        return -1;
                    }
                }
            }
        }
        for (int q = 0; q < 4; q++) {
            r = ac4e_chparam_parse(b, &info[q < 2 ? 8 : 10], &(_ac4d_self->xm_asq)[q]);
            if (r != AC4E_OK) {
                *note = "714-chparam-A";
                return -1;
            }
        }
        if (e9_debug()) {
            /* T-probe: H-pair RMS + a'0 mat-a per band (g0) + gap. */
            double e2 = 0;
            int nn = 0;
            for (int w = 0; w < info[8].num_windows; w++)
                for (int l = 0; l < info[8].win_len[w]; l++) {
                    e2 += ch[8].co[w][l] * ch[8].co[w][l];
                    nn++;
                }
            fprintf(stderr, "T714 tmdct=%d/%d Hrms=%.1f gap=%zu a0=",
                    tmdct[0], tmdct[1], nn ? sqrt(e2 / nn) : 0,
                    b->nbits - b->pos);
            for (int s = 0; s < 8; s++)
                fprintf(stderr, "%.2f,", (_ac4d_self->xm_asq)[0].mat[0][s][0]);
            fprintf(stderr, "\n");
        }
        /* Table 20 (spectral, per-tile): H'' = a'0*D' + H' etc., with
         * a' = mat-a - 1 (sap_gain; numerically forced: a'0 = 1.00 on
         * 768k <=> Ls fit 2.8284; QB-2 addendum pending memo confirm).
         * u = 0 => step-4 identity (D' = D). Geometry: D/E/F/G infos
         * (ch[4..7]); H-K bands align (same windows: verified below). */
        static const int t20d[4] = {4, 5, 6, 7};
        for (int q = 0; q < 4; q++) {
            asf_info_t *di = &info[t20d[q]];
            asf_info_t *hi = &info[q + 8];
            if (di->num_windows != hi->num_windows) {
                *note = "714-t20-geom";
                return -1;
            }
            /* Same window count but different lengths/grouping: the
             * spectral add is grid-meaningless (reads D-grid coeffs
             * into H-grid slots). Skip coupling for this pair
             * (H''=H) rather than corrupt; G6 f=118 hits this. */
            int t20ok = di->num_groups == hi->num_groups;
            for (int w = 0; t20ok && w < di->num_windows; w++)
                if (di->win_len[w] != hi->win_len[w] ||
                    di->win_to_group[w] != hi->win_to_group[w])
                    t20ok = 0;
            if (!t20ok) {
                if (e9_debug())
                    fprintf(stderr, "T20SKIP seq=%u q=%d\n", seq, q);
                continue;
            }
            for (int g = 0; g < di->num_groups; g++) {
                int wlen = di->win_len[0], nw = 0;
                for (int w = 0; w < di->num_windows; w++)
                    if (di->win_to_group[w] == g) {
                        wlen = di->win_len[w];
                        nw++;
                    }
                const int *so = sfb_offsets48(wlen);
                if (!so) {
                    *note = "714-t20-geom";
                    return -1;
                }
                int win = 0;
                for (int w = 0; w < di->num_windows &&
                                di->win_to_group[w] != g;
                     w++)
                    win++;
                int ms = di->max_sfb[0];
                if (ms >= AC4E_MAXBANDS)
                    ms = AC4E_MAXBANDS - 1;
                for (int s = 0; s < ms; s++) {
                    double ap = (_ac4d_self->xm_asq)[q].mat[g][s][0] - 1.0;
                    if (ap == 0.0)
                        continue;
                    for (int w = 0; w < nw; w++)
                        for (int l = so[s]; l < so[s + 1]; l++)
                            ch[q + 8].co[win + w][l] +=
                                ap * ch[t20d[q]].co[win + w][l];
                }
            }
        }
    }
    if (is_aspx) {
        /* 4 ASPX legs (2ch,2ch,2ch,1ch); parsed, HF unapplied
         * (core-PCM scope, like 5x-mode-1). NO endpos gate: ACPL
         * tails follow by design. */
        char s0[64], s1[64], s2[64], s3[64];
        int ax_ts = e_ax_timeslots(spf);
        s0[0] = s1[0] = s2[0] = s3[0] = '\0';
        if (ax_ts < 0) {
            *note = "aspx-noflb";
            return -1;
        }
        if (e_ax_data_2ch(b, ig, ax_ts, 0, 0, s0, sizeof s0, note))
            return -1;
        if (e_ax_data_2ch(b, ig, ax_ts, 1, 2, s1, sizeof s1, note))
            return -1;
        if (e_ax_data_2ch(b, ig, ax_ts, 2, 4, s2, sizeof s2, note))
            return -1;
        if (e_ax_data_1ch(b, ig, ax_ts, 3, 6, s3, sizeof s3, note))
            return -1;
        if (e9_debug())
            fprintf(stderr, "ASPX7 ig=%d [%s] [%s] [%s] [%s] pos=%zu\n",
                    ig, s0, s1, s2, s3, b->pos);
        /* ACPL data (mode 3): 4x acpl_data_1ch (b_5fronts 0), parsed
         * + stored per frame for the immersive post-pass. Always
         * parsed (defaults 15/FINE pre-first-I; a differing first-I
         * config clears eligibility). Errors clear eligibility only
         * (skeleton output stands); tails never gate the decode. */
        {
            int ok = 1;
            acpl_mod_t pm[4];
            for (int m = 0; m < 4 && ok; m++) {
                const char *an = NULL;
                if (acpl_parse_1ch(b, e3_acpl_nbands, e3_acpl_fine, m,
                                   &pm[m], &an, &_ac4d_self->acpl))
                    ok = 0;
            }
            if (ok) {
                /* H1: tagged apf ring (was whole-stream array). */
                long fno = _ac4d_self->h_fed;
                int slot = (int)(fno % AC4D_APFN);
                for (int m = 0; m < 4; m++)
                    _ac4d_self->ap3[slot].m[m] = pm[m];
                _ac4d_self->ap3[slot].fold_div = e3_imm_fold_div;
                _ac4d_self->ap3[slot].fno = fno;
                _ac4d_self->ap3[slot].have = 1;
            } else {
                e3_imm_elig = 0;
            }
            if (getenv("AC4DEC_ACPLPROBE")) {
                for (int m = 0; m < 4; m++) {
                    fprintf(stderr, "ACPL f=%u m=%d np=%d it=%d ts=%d/%d\n",
                            seq, m, pm[m].npset, pm[m].itype,
                            pm[m].timeslot[0], pm[m].timeslot[1]);
                    for (int ps = 0; ps < pm[m].npset; ps++) {
                        fprintf(stderr, "  adt=%d bdt=%d a:",
                                pm[m].adt[ps], pm[m].bdt[ps]);
                        for (int i = 0; i < pm[m].nbands; i++)
                            fprintf(stderr, " %.3f", pm[m].a[ps][i]);
                        fprintf(stderr, "\n  b:");
                        for (int i = 0; i < pm[m].nbands; i++)
                            fprintf(stderr, " %.4f", pm[m].b[ps][i]);
                        fprintf(stderr, "\n");
                    }
                }
                {
                    size_t gg = b->nbits - b->pos;
                    fprintf(stderr, "ACPLGAP f=%u gap=%zu\n", seq, gg);
                    if (getenv("AC4DEC_GAPBITS")) {
                        size_t nb = gg > 512 ? 512 : gg;
                        fprintf(stderr, "GAPBITS f=%u n=%zu hex=", seq,
                                gg);
                        unsigned acc = 0;
                        int have = 0;
                        for (size_t i = 0; i < nb; i++) {
                            size_t p = b->pos + i;
                            int bit = (b->buf[p >> 3] >>
                                       (7 - (p & 7))) & 1;
                            acc = (acc << 1) | (unsigned)bit;
                            if (++have == 4) {
                                fprintf(stderr, "%x", acc);
                                acc = 0;
                                have = 0;
                            }
                        }
                        if (have)
                            fprintf(stderr, "%x",
                                    acc << (4 - have));
                        if (gg > nb)
                            fprintf(stderr, "...");
                        fprintf(stderr, "\n");
                    }
                }
            }
        }
    }
    if (e9_debug()) {
        fprintf(stderr, "IM714 mode=%u grp=%u m2=%u mdct=%d/%d/%d lfems=%u",
                mode, grp, m2, mdct[0], mdct[1], mdct2, lms);
        for (int c = 0; c < 8; c++)
            fprintf(stderr, " ch%d=%c%dw%dg", c,
                    info[c].b_long ? 'L' : 'S', info[c].num_windows,
                    info[c].num_groups);
        fprintf(stderr, " pos=%zu/%zu\n", b->pos, b->nbits);
    }
    if (b->err) {
        *note = "714-trunc";
        return -1;
    }
    if (mode == 0 && e9_debug()) {
        /* Geometry diagnostic only. The Table-23 + fold mix happens
         * post-IMDCT on full track streams (e3_mix12p): per-frame
         * mixing is clock-incoherent under mixed grids (proven
         * first-hand on G6: empty-LFE zero-fill breaks oc totals). */
        int t23ok = 1;
        for (int q = 0; q < 8; q++) {
            if (!info[q].b_long || info[q].num_windows != 1)
                t23ok = 0;
        }
        int nd = info[4].win_len[0], ne = info[5].win_len[0];
        int nf = info[6].win_len[0], ng = info[7].win_len[0];
        if (nd != ne || nd != nf || nd != ng || nd <= 0)
            t23ok = 0;
        if (!t23ok)
            fprintf(stderr,
                    "T23GAP seq=%u nd=%d/%d/%d/%d H=%c%d/%d "
                    "J=%c%d/%d\n",
                    seq, nd, ne, nf, ng,
                    info[8].b_long ? 'L' : 'S',
                    info[8].num_windows, info[8].win_len[0],
                    info[10].b_long ? 'L' : 'S',
                    info[10].num_windows, info[10].win_len[0]);
    }
    if (mode == 3) {
        /* P2 5.5.2 Pseudocode 2 (b_5fronts 0) skeleton: fronts
         * z0 = 2x0, z2 = 2x1, z4 = 2x2; surrounds z5 = sqrt(2) *
         * Module(2x5, 0, y0). IMDCT chain is linear so spectral
         * scaling is exact. Module body (= pass at (a,b) = (1,0))
         * and F/G-track second path pending full ACPL port (QB-2).
         * Verified first-hand: fronts 2.0015/2.0013/2.0009 (~1 LSB)
         * on 514-2s; surround D-coef 2.8284 exact on 514-2s AND
         * height-frontsurround (F-coef varies: 2.00/1.54/1.34 =
         * content (a,b), the port's job). Backs (z6/z8) zeroed:
         * staged 7.1 truths carry exact-0 backs on both vectors
         * (render-level: Lb/Rb dropped in 7.1 map). */
        static const int fg[3] = {0, 1, 2};
        for (int q = 0; q < 3; q++)
            for (int w = 0; w < info[fg[q]].num_windows; w++)
                for (int l = 0; l < info[fg[q]].win_len[w]; l++)
                    ch[fg[q]].co[w][l] *= 2.0;
        for (int q = 4; q < 6; q++)
            for (int w = 0; w < info[q].num_windows; w++)
                for (int l = 0; l < info[q].win_len[w]; l++)
                    ch[q].co[w][l] *= 2.0 * 1.4142135623730951;
        /* Backs (z6/z8) zeroed at OUTPUT COPY (caller, mode 3): F/G
         * spectra stay live here -- the pending ACPL port consumes
         * their post-IMDCT time signals as module inputs. */
    }
    return (int)mode;
}

/* Dialnorm trim sample from the presentation substream (first non-bed;
 * DEE order pres-audio, mp4 order audio-pres). Sets *db (byte0 low 7 =
 * dialnorm_bits, -4x law) and *g (linear trim at e3_reflevel).
 * Returns 1 when sampled, 0 when unavailable (caller keeps unity). */
static int e3_dn_sample(const raw_t *r, const toc_t *t, unsigned *db,
                        double *g) {
    int dn_si = -1;
    for (int s = 0; s < t->ns; s++) {
        if (s != t->bed_si) {
            dn_si = s;
            break;
        }
    }
    if (dn_si < 0 || t->sizes[dn_si] < 2)
        return 0;
    size_t s0o = t->tocB + t->pbase;
    for (int s = 0; s < dn_si; s++)
        s0o += t->sizes[s];
    if (s0o + 2 > r->len)
        return 0;
    *db = (unsigned)r->p[s0o] & 0x7Fu;
    double dn = -(double)*db / 4.0;
    *g = pow(10.0, (e3_reflevel - dn) / 20.0) / 32768.0;
    return 1;
}

/* DRC wire-curve store + Table-166 eval (G-DRC step 4: wire-driven
 * curves). Carried ndot config (persists across frames like the
 * reference carried-state). Per-mode polyline in dB2 (native units:
 * linear = 2^(G/6); level dB2 = dB/1.0034). Table-166 formulas per
 * A-66 relay. Transmitted curve OVERRIDES eac3-None (WJCT eac3=0 +
 * m0-CURVE ACTIVE mechanism); default legs use default-profile[eac3]
 * (Film-light EXACT for eac3=2; eac3=0/1/3 = unity + note (gap:
 * pure-None unobserved, Film-standard/Music unrelayed)). Mode select:
 * largest id whose Table-161 window contains system ref (default
 * -31 -> m0 (only window containing -31); m1+ windows placeholder).
 * Ballistics stay FITTED (behavior-effective; wire TCs = defaults
 * (A-66) don't capture the effective response (hb62-71)). */
/* ctx-moved to top (was lines 4356-4357) */
/* ctx-moved */

/* ctx-moved to top (was lines 4359-4372) */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */

/* ctx-moved to top (was lines 4374-4379) */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */

/* ctx-member */
/* ctx-member */

/* ---- E8 display rules (DRP-mirroring names; A-77/A-79/A-80) ---- */

/* Table-160 eac3 profile names (A-77 Q1, verbatim). */

/* Table-171 chanconfig display names (A-79 Q2; DRP spellings; 2/3/4/5/7
 * analogy-marked with "*" until DRP-observed (A-80 Q5)). */

/* Transmitted-leg display name, f(nb) (A-80 Q1: KDAF 10/10->light,
 * WJCT/believer 0/5->standard; f-mid + eac3-rule dead). NULL = unmapped. */

/* Displayed-config rule (A-80 Q2-proposed, 8/8; A-81 pending): gate AND
 * (eac3 != None OR any transmitted-curve leg). atsc3 (ea0+all-def) and
 * mpa20-f0 (gate 0) correctly yield none. */
static int e3_shows_config(int gate, const drc_cfg_t *c) {
    if (!gate || !c->valid)
        return 0;
    if (c->eac3 != 0)
        return 1;
    for (int k = 0; k < c->nmode; k++) {
        const drc_mode_t *m = &c->modes[k];
        if (m->is_curve && !m->is_default && m->repeat_to < 0 &&
            m->gains_cfg < 0)
            return 1;
    }
    return 0;
}

/* Target-row name for a mode id (window->mid direct; repeat-resolved by
 * mid; sole-mode configs use the sole leg for every target). *tbd set
 * when the name is unmapped (unknown nb / gains / missing mid). */

/* Loudness display scale (Q-79 P1, proven x5+): dB = (raw-1024)/10. */

/* Film-light default polyline, dB2 (A-63 A1 = Table-162 native dB2:
 * nullband [-10,+10]; boost (+6 @ -22, 0 sections); cut (-24 max,
 * 1 section (+20,-5), maxcut +40)). */
static const double drc_film_light_l[] = {-22.0, -10.0, 10.0, 20.0, 40.0};
static const double drc_film_light_g[] = {6.0, 0.0, 0.0, -5.0, -24.0};
#define DRC_FILM_N 5

/* Table-166 eval (A-66 relay): wire fields -> dB2 polyline.
 * nr_b=0 / nr_c=1 EXACT (all observed B/D vectors); nr_b=1 / nr_c=0
 * MIRROR-PLACEHOLDER (symmetric guess, flagged: no B/D observation).
 * Out: ascending (pl, pg), npts. */
static void drc_eval_curve(unsigned nblo, unsigned nbhi, unsigned mb,
                           unsigned levb, unsigned nrb, unsigned gb,
                           unsigned lb, unsigned mc, unsigned levc,
                           unsigned nrc, unsigned gc, unsigned lc,
                           double *pl, double *pg, int *npts) {
    double l0lo = -(double)nblo, l0hi = (double)nbhi;
    int n = 0;
    /* boost leg (below nullband), if any. */
    if (mb) {
        double lmax, lsec = l0lo, gmax = (double)mb;
        if (nrb) {
            /* mirror-placeholder (mpa20 has nr_b=1; no B/D pair). */
            double gs = 1.0 + (double)gb;
            double ls = l0lo - (1.0 + (double)lb);
            lmax = ls - (1.0 + (double)levb);
            pl[n] = lmax;
            pg[n] = gmax;
            n++;
            pl[n] = ls;
            pg[n] = gs;
            n++;
        } else {
            lmax = lsec - (1.0 + (double)levb);
            pl[n] = lmax;
            pg[n] = gmax;
            n++;
        }
    }
    /* nullband. */
    pl[n] = l0lo;
    pg[n] = 0.0;
    n++;
    if (l0hi != l0lo) {
        pl[n] = l0hi;
        pg[n] = 0.0;
        n++;
    }
    /* cut leg (above nullband), if any. */
    if (mc) {
        double gmax = -(double)mc;
        if (nrc) {
            double gs = -(1.0 + (double)gc);
            double ls = l0hi + (1.0 + (double)lc);
            double lmax = ls + (1.0 + (double)levc);
            pl[n] = ls;
            pg[n] = gs;
            n++;
            pl[n] = lmax;
            pg[n] = gmax;
            n++;
        } else {
            /* mirror-placeholder (no B/D observation). */
            double lmax = l0hi + (1.0 + (double)levc);
            pl[n] = lmax;
            pg[n] = gmax;
            n++;
        }
    }
    *npts = n;
}

/* Polyline lookup (dB2 in -> dB2 gain; clamp flat outside). */
/* (deleted: belonged to removed DRC-apply/DE-drive legs) */

/* Fill default-profile legs ( transmitted curves already stored).
 * eac3=2 Film-light EXACT; else unity + note (gap, flagged). */
static void drc_fill_defaults(drc_cfg_t *c) {
    for (int k = 0; k < c->nmode; k++) {
        drc_mode_t *m = &c->modes[k];
        if (m->is_curve || m->repeat_to >= 0 || m->gains_cfg >= 0)
            continue;
        /* default-profile leg. */
        if (c->eac3 == 2) {
            for (int i = 0; i < DRC_FILM_N; i++) {
                m->pl[i] = drc_film_light_l[i];
                m->pg[i] = drc_film_light_g[i];
            }
            m->npts = DRC_FILM_N;
            m->is_curve = 1;
        } else {
            /* unity (pure-None / unrelayed profile gap). */
            m->pl[0] = -100.0;
            m->pg[0] = 0.0;
            m->pl[1] = 100.0;
            m->pg[1] = 0.0;
            m->npts = 2;
            m->is_curve = 1;
        }
    }
    /* repeat legs resolve to target polylines. */
    for (int k = 0; k < c->nmode; k++) {
        drc_mode_t *m = &c->modes[k];
        if (m->repeat_to < 0 || m->is_curve)
            continue;
        for (int q = 0; q < c->nmode; q++) {
            if (c->modes[q].mid == m->repeat_to &&
                c->modes[q].is_curve) {
                int keep = m->mid;
                int keepr = m->repeat_to;
                *m = c->modes[q];
                m->mid = keep;
                m->repeat_to = keepr;
                break;
            }
        }
    }
}

/* Mode select: largest id whose Table-161 window contains ref.
 * EXACT per A-77 Q2 (Table 161 transcribed): m0 Home Theatre
 * [-31,-27]; m1 Flat-panel [-26,-17]; m2/m3 Portable [-16,0].
 * (Prior m1+ placeholder-above removed; out-of-range ref now
 * selects nothing -> fallback, instead of m3-default.)
 * Returns mode index or -1. */
static int drc_select_mode(const drc_cfg_t *c, double ref) {
    if (c->nmode == 1)
        return 0; /* sole mode: no window arbitration needed */
    static const double wmin[DRC_MAXMODES] = {-31, -26, -16, -16,
                                              -16, -16, -16, -16};
    static const double wmax[DRC_MAXMODES] = {-27, -17, 0, 0,
                                              0, 0, 0, 0};
    int best = -1, bestid = -1;
    for (int k = 0; k < c->nmode; k++) {
        int id = c->modes[k].mid;
        if (id < 0 || id >= DRC_MAXMODES)
            continue;
        if (ref >= wmin[id] && ref <= wmax[id] && id > bestid) {
            bestid = id;
            best = k;
        }
    }
    return best;
}

/* DRC-config census dump (G-DRC step 1: parse only, PCM-neutral).
 * Walks the pres-substream DRC region per tools/drc_wire_probe.py
 * pres_substream+drc_parse+drc_curve (field order/widths) and prints
 * one DRC header per frame + per-mode curve legs on ndot gate=1
 * frames. Called only under AC4DEC_DRCDBG (zero behavior change
 * otherwise). Beds: spec/survey-drc-de.md S7 (KDAF/WJCT/mpa). */
/* Pres-side loudness keep (E8 --print-info; G4/51/KDAF first-hand).
 * Same walk as tools/drc_wire_probe.py loudness(r, sus=1,
 * pres_ldn=1); values KEPT (were dropped). Order of the 6 named
 * fields: relgat, spchgat, strm3s, maxstrm3s, truepk, maxtruepk.
 * Display scale (Q-79 P1, proven): dB = (raw-1024)/10. */
/* ctx-moved to top (was lines 4683-4706) */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */

/* ctx-member */
/* ctx-member */

/* Bed-substream audio metadata keep (E8 --print-info stage-2).
 * C port of tools/drc_wire_probe.py audio_metadata + basic_p2 +
 * ext_p2 + de_config (sus=1 path; P2 6.2.7). Codec peek NOT ported
 * (print-info needs no codec_mode; skipping it keeps wide/IMS cm
 * probeable where the tool errors). Keeps: b_dialog, tools,
 * de_gate, de_cfg{method,maxgain,chanconfig}; rest walked-only.
 * endok = tools-bound end-alignment (self-check vs mis-walk). */
/* ctx-moved to top (was lines 4718-4739) */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */
/* ctx-moved */

/* ctx-member */
/* ctx-member */

/* E8 verb f0 drc-config latch (first-frame state; mpa20-absent => 0).
 * (Sibling plain-int latches live with the compand globals above.) */
/* ctx-member */
/* ctx-member */

/* Raw channel_mode -> metadata cm enum (tool CMENUM:
 * 0->0, 2->1, 12->2, 13->3, 14->4; else -1 = minimal walk). */
static int e3_cm_map(int raw) {
    switch (raw) {
    case 0:
        return 0;
    case 2:
        return 1;
    case 12:
        return 2;
    case 13:
        return 3;
    case 14:
        return 4;
    default:
        return -1;
    }
}

/* Table 171 chanconfig -> de_nr_channels. */
static int e3_de_nch(unsigned cc) {
    static const int m[8] = { 0, 1, 1, 2, 1, 2, 2, 3 };
    return m[cc & 7];
}

/* contains(cm, which): 0=c, 1=lr, 2=lsrs, 3=lfe. cm>4: none. */
static int e3_contains(int cm, int which) {
    switch (cm) {
    case 0:
        return which == 0;
    case 1:
        return which == 1;
    case 2:
        return which == 0 || which == 1;
    case 3:
    case 4:
        return which != 3 || cm == 4;
    default:
        return 0;
    }
}

/* Substream loudness walk-only (tool loudness(r, sus=1, pres_ldn=0):
 * dg + 6 named + lra/mntry/maxmntry + rtll + e-ext). */
static void e3_subloud_skip(br_t *b) {
    (void)br_u(b, 1); /* dg */
    for (int k = 0; k < 6; k++) {
        if (br_u(b, 1)) {
            (void)br_u(b, 11);
            if (k == 1)
                (void)br_u(b, 3);
        }
    }
    if (br_u(b, 1)) {
        (void)br_u(b, 10);
        (void)br_u(b, 3);
    }
    if (br_u(b, 1))
        (void)br_u(b, 11);
    if (br_u(b, 1))
        (void)br_u(b, 11);
    if (br_u(b, 1))
        (void)br_u(b, 8); /* rtll */
    if (br_u(b, 1)) {
        unsigned e = br_u(b, 5);
        if (e == 31)
            e += br_var(b, 4);
        if (e)
            (void)br_u(b, (int)e);
    }
}

/* basic_p2 sus=1 walk-only. */
static void e3_basic_skip(br_t *b, int cm) {
    if (!br_u(b, 1))
        return;
    if (br_u(b, 1)) {
        (void)br_u(b, 8); /* sub_loud */
        if (br_u(b, 1))
            e3_subloud_skip(b);
    }
    if (cm == 1) {
        if (br_u(b, 1)) {
            (void)br_u(b, 3);
            (void)br_u(b, 2);
        }
    }
    if (cm > 1) {
        if (cm == 3 || cm == 4) {
            if (br_u(b, 1))
                (void)br_u(b, 3);
            if (br_u(b, 1))
                (void)br_u(b, 4);
        }
        if (cm >= 5 && cm <= 10) {
            if (br_u(b, 1))
                (void)br_u(b, 2);
        }
        (void)br_u(b, 2); /* phmc */
        (void)br_u(b, 1); /* surr */
        (void)br_u(b, 1); /* lfeatt */
    }
    if (br_u(b, 1))
        (void)br_u(b, 1); /* dc */
}

/* ext_p2 sus=1: keep b_dialog (+dlg_maxgain), walk rest. */
static void e3_ext_read(br_t *b, int cm, info_audmeta_t *o) {
    o->b_dialog = (int)br_u(b, 1);
    if (o->b_dialog) {
        if (br_u(b, 1)) {
            o->dlg_maxgain = br_u(b, 2);
            o->has_dlg_maxgain = 1;
        }
        if (br_u(b, 1)) {
            if (cm == 0) {
                (void)br_u(b, 8);
            } else {
                (void)br_u(b, 8);
                (void)br_u(b, 8);
                (void)br_u(b, 2);
            }
        }
    }
    if (br_u(b, 1)) {
        if (e3_contains(cm, 0) && br_u(b, 1))
            (void)br_u(b, 1);
        if (e3_contains(cm, 1)) {
            if (br_u(b, 1))
                (void)br_u(b, 1);
            if (br_u(b, 1))
                (void)br_u(b, 1);
        }
        if (e3_contains(cm, 2)) {
            (void)br_u(b, 1);
            (void)br_u(b, 1);
        }
        if (e3_contains(cm, 3))
            (void)br_u(b, 1);
    }
    if (br_u(b, 1))
        (void)br_u(b, 4); /* evp */
}

/* ---- P2 T78 de_data walk (diagnostic-only; Table 78 method-0) ----
 * Runs on a COPY of the bitreader bounded at region end; the main
 * walk is untouched (PCM-neutral). de_par_prev carried across
 * frames, reset per stream with the e_decfg block. Only method 0
 * is walked (corpus is all method-0 per A-82); other methods and
 * missing carried cfg are skip-legs. Returns bits consumed, -1 on
 * any failure (truncation, bad codeword, end-misalign). */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* P2 drive activity bridge (metadata-time -> emit-time, same frame). */
/* ctx-member */
/* ctx-member */
/* ctx-member */
/* H1: DE whole-stream maps removed (DE-on unsupported in streaming v1) */

static int e3_br_getbit(void *ctx) {
    br_t *b = ctx;
    if (b->pos >= b->nbits)
        return -1;
    int bit = (b->d[b->pos >> 3] >> (7 - (b->pos & 7))) & 1;
    b->pos++;
    return bit;
}

static int e3_de_walk(const br_t *r, size_t endb, int b_iframe,
                      unsigned method, int nch, int *keep_o, int *ms_o,
                      int par[3][8]) {
    *keep_o = -1;
    *ms_o = 0;
    if (nch <= 0)
        return (endb == r->pos) ? 0 : -1;
    if (method != 0)
        return -1;
    if (endb <= r->pos || endb > r->nbits)
        return -1;
    br_t w = { r->d, endb, r->pos, 0 };
    int m = (int)(method % 2);
    int keep = b_iframe ? 0 : (int)br_u(&w, 1);
    *keep_o = keep;
    if (keep) {
        if (w.err || w.pos != endb)
            return -1;
        /* repeated params = previous (display + activity inherit) */
        for (int ch = 0; ch < nch; ch++)
            for (int band = 0; band < 8; band++)
                par[ch][band] = e_dewalk_prevok[ch] ?
                    e_dewalk_prev[ch][band] : 0;
        return (int)(w.pos - r->pos);
    }
    int ms = 0;
    if (nch == 2) {
        ms = (int)br_u(&w, 1);
        *ms_o = ms;
    }
    int abs_book = huffman_de(0, m);
    int diff_book = huffman_de(1, m);
    int absoff = huffman_books[abs_book].off;
    int diffoff = huffman_books[diff_book].off;
    int ref = 0;
    for (int ch = 0; ch < nch - ms; ch++) {
        if (b_iframe && ch == 0) {
            int ai = hcb_decode(&huffman_books[abs_book],
                                e3_br_getbit, &w);
            if (ai < 0)
                return -1;
            ref = ai - absoff;
            par[0][0] = ref;
            e_dewalk_prev[0][0] = ref;
            for (int band = 1; band < 8; band++) {
                int di = hcb_decode(&huffman_books[diff_book],
                                    e3_br_getbit, &w);
                if (di < 0)
                    return -1;
                ref += di - diffoff;
                par[0][band] = ref;
                e_dewalk_prev[0][band] = ref;
            }
        } else {
            for (int band = 0; band < 8; band++) {
                int di = hcb_decode(&huffman_books[diff_book],
                                    e3_br_getbit, &w);
                if (di < 0)
                    return -1;
                int base = b_iframe ? ref :
                    (e_dewalk_prevok[ch] ? e_dewalk_prev[ch][band] : 0);
                par[ch][band] = base + di - diffoff;
                if (b_iframe)
                    ref = par[ch][band];
                e_dewalk_prev[ch][band] = par[ch][band];
            }
        }
        e_dewalk_prevok[ch] = 1;
        ref = par[ch][0];
    }
    if (w.err || w.pos != endb)
        return -1;
    return (int)(w.pos - r->pos);
}

/* Full bed-metadata read. sub/slen = bed substream bytes. */
static void e3_audmeta_read(const uint8_t *sub, size_t slen, int cm,
                            int b_iframe, info_audmeta_t *o) {
    memset(o, 0, sizeof *o);
    o->de_keep = -1;
    /* Drive bridge safe defaults (parse-fail -> inactive, routing
     * holds carried cfg); overwritten at normal end. */
    e_de_cur_active = 0;
    e_de_cur_mabs = 0;
    e_de_cur_cc = e_dewalk_have ? e_dewalk_chan : 0;
    if (slen < 2)
        return;
    br_t a = {sub, slen * 8, 0, 0};
    unsigned asz = br_u(&a, 15);
    if (br_u(&a, 1))
        asz += br_var(&a, 7) << 15;
    size_t meta_at = 2 + (size_t)asz;
    o->audio_size = asz;
    o->meta_at = meta_at;
    if (a.err || meta_at >= slen)
        return;
    br_t r = {sub + meta_at, (slen - meta_at) * 8, 0, 0};
    e3_basic_skip(&r, cm);
    e3_ext_read(&r, cm, o);
    unsigned tsz = br_u(&r, 7);
    if (br_u(&r, 1))
        tsz += br_var(&r, 3) << 7;
    o->tools = tsz;
    size_t tools_at = r.pos;
    o->de_gate = (int)br_u(&r, 1);
    if (o->de_gate) {
        if (b_iframe || br_u(&r, 1)) {
            o->de_method = br_u(&r, 2);
            o->de_maxgain = br_u(&r, 2);
            o->de_chanconfig = br_u(&r, 3);
            o->has_de_cfg = 1;
        }
    }
    /* tools-bound jump + emdf + end-align check. */
    size_t endb = tools_at + (size_t)tsz;
    if (r.err || endb > r.nbits)
        return;
    if (o->de_gate && o->has_de_cfg && endb > r.pos)
        o->de_skip_bits = (unsigned)(endb - r.pos);
    size_t de_at = r.pos;
    if (o->de_gate && endb > r.pos) {
        o->de_region_bits = (unsigned)(endb - r.pos);
        o->de_keep = b_iframe ? 0 : (int)br_u(&r, 1);
    } else if (o->de_gate && b_iframe) {
        o->de_keep = 0;
    }
    /* P2 T78 walk on the untouched region (diag-only). Re-config
     * tracked (last cfg wins); DEDATA nch reads the same state. */
    if (o->has_de_cfg) {
        e_dewalk_have = 1;
        e_dewalk_method = o->de_method;
        e_dewalk_chan = o->de_chanconfig;
    }
    o->de_par_nch = -1;
    if (o->de_gate && (o->has_de_cfg || e_dewalk_have)) {
        unsigned wm = o->has_de_cfg ? o->de_method : e_dewalk_method;
        int wnch = e3_de_nch(o->has_de_cfg ? o->de_chanconfig :
                             e_dewalk_chan);
        o->de_par_nch = wnch;
        if (wm == 0) {
            br_t rs = r;
            rs.pos = de_at;
            rs.err = 0;
            int used = e3_de_walk(&rs, endb, b_iframe, wm, wnch,
                                  &o->de_par_keep, &o->de_par_ms,
                                  o->de_par);
            if (used >= 0) {
                o->de_par_ok = 1;
                o->de_par_bits = (unsigned)used;
            } else {
                o->de_par_ok = -1;
            }
        }
    }
    /* P2 drive activity (T=2 pinned hb74-81; ok==1 required so a
     * failed walk never gates active on partial pars). */
    o->de_eff_cc = (o->has_de_cfg || e_dewalk_have) ?
        (o->has_de_cfg ? o->de_chanconfig : e_dewalk_chan) : 0;
    {
        int mabs = 0;
        for (int mc = 0; mc < 3; mc++)
            for (int mb = 0; mb < 8; mb++) {
                int v = o->de_par[mc][mb];
                if (v < 0)
                    v = -v;
                if (v > mabs)
                    mabs = v;
            }
        o->de_active = (o->de_par_ok == 1 && o->de_par_nch > 0 &&
                        mabs >= 2);
        e_de_cur_mabs = (o->de_par_ok == 1 && o->de_par_nch > 0) ?
            mabs : 0;
    }
    e_de_cur_active = o->de_active;
    e_de_cur_cc = o->de_eff_cc;
    r.pos = endb;
    (void)br_u(&r, 1); /* emdf */
    if (r.err)
        return;
    o->present = 1;
    o->endok = ((meta_at * 8 + r.pos + 7) / 8 == slen);
}

static void e3_drc_loudread(br_t *b, info_loud_t *o) {
    o->present = 1;
    o->ver = br_u(b, 2);
    if (o->ver == 3)
        o->ver += br_u(b, 4);
    o->prac = br_u(b, 4);
    if (o->prac != 0) {
        if (br_u(b, 1)) {
            o->dprac = br_u(b, 3);
            o->has_dprac = 1;
        }
        o->corrtype = br_u(b, 1);
        o->has_corr = 1;
    }
    for (int k = 0; k < 6; k++) {
        if (br_u(b, 1)) {
            o->val[k] = br_u(b, 11);
            o->has[k] = 1;
            if (k == 1) {
                o->spch_dprac = br_u(b, 3);
                o->has_spch_dprac = 1;
            }
        }
    }
    if (br_u(b, 1)) {
        while (br_u(b, 1) == 0 && !b->err)
            ;
        o->eos = br_u(b, 1);
        o->has_eos = 1;
        if (br_u(b, 1)) {
            o->bndyoff = br_u(b, 11);
            o->has_bndy = 1;
        }
    }
    if (br_u(b, 1)) {
        o->lra = br_u(b, 10);
        o->lra_prac = br_u(b, 3);
        o->has_lra = 1;
    }
    if (br_u(b, 1)) {
        o->mntry = br_u(b, 11);
        o->has_mntry = 1;
    }
    if (br_u(b, 1)) {
        o->maxmntry = br_u(b, 11);
        o->has_maxmntry = 1;
    }
    if (br_u(b, 1)) {
        o->rtll = br_u(b, 8);
        o->has_rtll = 1;
    }
    if (br_u(b, 1)) {
        unsigned e = br_u(b, 5);
        if (e == 31)
            e += br_var(b, 4);
        if (e)
            (void)br_u(b, (int)e);
    }
}

static void e3_drc_curve_dump(br_t *b, size_t fi, unsigned mid) {
    unsigned nblo = br_u(b, 4), nbhi = br_u(b, 4);
    unsigned mb = br_u(b, 4);
    unsigned levb = 0, nrb = 0, gb = 0, lb = 0;
    if (mb) {
        levb = br_u(b, 5);
        nrb = br_u(b, 1);
        if (nrb) {
            gb = br_u(b, 4);
            lb = br_u(b, 5);
        }
    }
    unsigned mc = br_u(b, 5);
    unsigned levc = 0, nrc = 0, gc = 0, lc = 0;
    if (mc) {
        levc = br_u(b, 6);
        nrc = br_u(b, 1);
        if (nrc) {
            gc = br_u(b, 5);
            lc = br_u(b, 5);
        }
    }
    unsigned tcdef = br_u(b, 1);
    unsigned tc[4] = {0, 0, 0, 0};
    int havethr = 0;
    unsigned th0 = 0, th1 = 0;
    if (!tcdef) {
        for (int k = 0; k < 4; k++)
            tc[k] = br_u(b, 8);
        if (br_u(b, 1)) {
            havethr = 1;
            th0 = br_u(b, 5);
            th1 = br_u(b, 5);
        }
    }
    fprintf(stderr,
            "DRC i=%zu m%u=curve nb=%u/%u boost=%u/%u/%u", fi, mid,
            nblo, nbhi, mb, levb, nrb);
    if (mb && nrb)
        fprintf(stderr, "/%u/%u", gb, lb);
    fprintf(stderr, " cut=%u/%u/%u", mc, levc, nrc);
    if (mc && nrc)
        fprintf(stderr, "/%u/%u", gc, lc);
    fprintf(stderr, " tcdef=%u", tcdef);
    if (!tcdef) {
        fprintf(stderr, " tc=%u,%u,%u,%u", tc[0], tc[1], tc[2], tc[3]);
        if (havethr)
            fprintf(stderr, " thr=%u,%u", th0, th1);
    }
    fprintf(stderr, "%s\n", b->err ? " ERR" : "");
}

/* SET-A (duet74 t273): Table-75 drc_gains decode.
 * nch = Table-168 groups, nsf = Table-169 subframes, nb = Table-163.
 * Stores dB2 gains (first val - 64 per 4.3.13.6.1 + DRC_HCB diffs
 * per 4.3.13.6.2, ref-chain verbatim Table-75) into m. cfg0 = single
 * 7b val, no diffs. Returns 0 ok, -1 on Huffman error. */
static int drc_gain_nband(int cfg) {
    return cfg == 2 ? 2 : cfg == 3 ? 4 : 1;
}

static int drc_gain_nsub(int spf) {
    switch (spf) {
    case 384:
        return 1;
    case 512:
        return 2;
    case 768:
    case 960:
        return 3;
    case 1024:
        return 4;
    case 1536:
    case 1920:
        return 6;
    case 2048:
        return 8;
    default:
        return -1;
    }
}

static int drc_decode_gains(br_t *b, int cfg, int nch, int nsf,
                            drc_mode_t *m) {
    int nb = drc_gain_nband(cfg);
    unsigned v0 = br_u(b, 7);
    if (b->err)
        return -1;
    int g0 = (int)v0 - 64;
    m->gains_db2[0][0][0] = g0;
    m->gains_nch = m->gains_nsf = m->gains_nb = 1;
    m->has_gains = 1;
    if (cfg <= 0)
        return 0;
    if (nch < 1 || nch > 3 || nsf < 1 || nsf > 8 || nb < 1 || nb > 4)
        return -1;
    {
        int off = huffman_books[HUFFMAN_DRC].off;
        int ref = g0;
        for (int ch = 0; ch < nch; ch++) {
            for (int band = 0; band < nb; band++) {
                for (int sf = 0; sf < nsf; sf++) {
                    if (sf != 0 || band != 0 || ch != 0) {
                        int di = hcb_decode(&huffman_books[HUFFMAN_DRC],
                                            e3_br_getbit, b);
                        if (di < 0)
                            return -1;
                        m->gains_db2[ch][sf][band] = ref + di - off;
                    }
                    ref = m->gains_db2[ch][sf][band];
                }
                ref = m->gains_db2[ch][0][band];
            }
            ref = m->gains_db2[ch][0][0];
        }
        m->gains_nch = nch;
        m->gains_nsf = nsf;
        m->gains_nb = nb;
    }
    return 0;
}

static void e3_drc_dump(const raw_t *r, const toc_t *t) {
    size_t fi = (size_t)_ac4d_self->h_fed; /* kept ordinal */
    int psi = -1;
    for (int s = 0; s < t->ns; s++) {
        if (s != t->bed_si) {
            psi = s;
            break;
        }
    }
    if (psi < 0 || t->sizes[psi] < 2) {
        fprintf(stderr, "DRC i=%zu unavail\n", fi);
        return;
    }
    size_t so = t->tocB + t->pbase;
    for (int s = 0; s < psi; s++)
        so += t->sizes[s];
    size_t plen = t->sizes[psi];
    if (so + plen > r->len || so + 2 > r->len) {
        fprintf(stderr, "DRC i=%zu unavail\n", fi);
        return;
    }
    br_t b = {r->p + so, plen * 8, 0, 0};
    if (br_u(&b, 1)) {
        fprintf(stderr, "DRC i=%zu add-data\n", fi);
        return;
    }
    unsigned dial = br_u(&b, 7);
    int loud = 0;
    if (br_u(&b, 1)) {
        loud = 1;
        info_loud_t discard;
        memset(&discard, 0, sizeof discard);
        e3_drc_loudread(&b, &discard);
    }
    unsigned dz = br_u(&b, 5);
    if (br_u(&b, 1))
        dz += br_var(&b, 3) << 5;
    size_t start = b.pos;
    unsigned gate = br_u(&b, 1);
    int pndot = t->b_pres_ndot;
    fprintf(stderr, "DRC i=%zu ndot=%d dial=%u loud=%d size=%u gate=%u",
            fi, pndot, dial, loud, dz, gate);
    if (b.err) {
        fprintf(stderr, " ERR\n");
        return;
    }
    if (!gate) {
        if (dz > 1)
            fprintf(stderr, " SIZEBOUND");
        fprintf(stderr, "\n");
        return;
    }
    if (!pndot) {
        fprintf(stderr, " carried\n");
        return;
    }
    /* pndot gate=1: config + data, size-bound verified. */
    unsigned nrm = br_u(&b, 3);
    unsigned mids[8];
    int iscurve[8];
    int repid[8];
    int gaincfg[8];
    unsigned nmode = nrm + 1 > 8 ? 8 : nrm + 1;
    fprintf(stderr, " nmodes=%u", nrm + 1);
    for (unsigned k = 0; k < nmode; k++) {
        unsigned mid = br_u(&b, 3);
        mids[k] = mid;
        iscurve[k] = 0;
        repid[k] = -1;
        gaincfg[k] = -1;
        if (mid > 3) {
            (void)br_u(&b, 5);
            (void)br_u(&b, 5);
        }
        if (br_u(&b, 1)) {
            repid[k] = (int)br_u(&b, 3);
            fprintf(stderr, "\nDRC i=%zu m%u=repeat=%d", fi, mid,
                    repid[k]);
        } else if (br_u(&b, 1)) {
            fprintf(stderr, "\nDRC i=%zu m%u=default", fi, mid);
            iscurve[k] = 1; /* default-profile = curve leg */
        } else if (br_u(&b, 1)) {
            fprintf(stderr, "\n");
            e3_drc_curve_dump(&b, fi, mid);
            iscurve[k] = 1;
            continue;
        } else {
            unsigned gc = br_u(&b, 2);
            gaincfg[k] = (int)gc;
            fprintf(stderr, "\nDRC i=%zu m%u=gainscfg=%u", fi, mid, gc);
        }
        fprintf(stderr, "%s", b.err ? " ERR" : "");
    }
    /* repeat legs resolve against target modes (any order). */
    for (unsigned k = 0; k < nmode; k++) {
        if (repid[k] < 0)
            continue;
        for (unsigned q = 0; q < nmode; q++) {
            if (q != k && mids[q] == (unsigned)repid[k]) {
                iscurve[k] = iscurve[q];
                break;
            }
        }
    }
    unsigned eac3 = br_u(&b, 3);
    /* drc_data per mode shapes (SET-A: gains decoded, not skipped). */
    int anycurve = 0;
    for (unsigned k = 0; k < nmode; k++) {
        if (iscurve[k]) {
            anycurve = 1;
            continue;
        }
        unsigned gz = br_u(&b, 6);
        if (br_u(&b, 1))
            gz += br_var(&b, 2) << 6;
        unsigned ver = br_u(&b, 2); /* version */
        if (gz < 2) {
            b.err = 1;
            continue;
        }
        if (ver == 0) {
            size_t pend = b.pos + (gz - 2);
            drc_mode_t gm;
            int nch = t->bed_ch <= 2 ? 1 : 3;
            int rate = t->rate;
            int nsf = (rate >= 0 && rate < 14) ?
                drc_gain_nsub(e4_spf[rate]) : -1;
            memset(&gm, 0, sizeof gm);
            gm.gains_cfg = gaincfg[k];
            int ok = drc_decode_gains(&b, gaincfg[k], nch, nsf, &gm);
            if (ok < 0)
                b.err = 1;
            if (!b.err && b.pos <= pend)
                (void)br_u(&b, (int)(pend - b.pos));
            if (!b.err && b.pos != pend)
                b.err = 1;
            fprintf(stderr, "\nDRC i=%zu m%u gains g0=%d n=%d/%d/%d%s",
                    fi, mids[k], gm.gains_db2[0][0][0], gm.gains_nch,
                    gm.gains_nsf, gm.gains_nb, ok < 0 ? " ERR" : "");
        } else {
            (void)br_u(&b, (int)(gz - 2)); /* drc2-bits skipped */
        }
    }
    if (anycurve) {
        (void)br_u(&b, 1); /* reset */
        (void)br_u(&b, 2); /* resv */
    }
    fprintf(stderr, "\nDRC i=%zu eac3=%u dataok=%d\n", fi, eac3,
            (!b.err && b.pos - start == dz) ? 1 : 0);
}

/* G-DRC step 4: store-walker. Same field walk as e3_drc_dump (kept
 * in sync by construction: identical widths/order), but stores the
 * ndot gate=1 config into e_drc for the apply stage. Carried frames
 * (gate=0 / ndot=0 / add-data) keep the previous config. Runs
 * unconditionally per frame (cheap header walk; PCM-neutral until
 * apply wires in). Beds: survey-drc-de.md S7. */
/* ctx-member */

static void drc_parse_cfg(const raw_t *r, const toc_t *t) {
    size_t fi = (size_t)_ac4d_self->h_fed; /* kept ordinal */
    drc_cfg_t *c = &e_drc;
    int psi = -1;
    for (int s = 0; s < t->ns; s++) {
        if (s != t->bed_si) {
            psi = s;
            break;
        }
    }
    if (psi < 0 || t->sizes[psi] < 2)
        return; /* keep previous */
    size_t so = t->tocB + t->pbase;
    for (int s = 0; s < psi; s++)
        so += t->sizes[s];
    size_t plen = t->sizes[psi];
    if (so + plen > r->len || so + 2 > r->len)
        return;
    br_t b = {r->p + so, plen * 8, 0, 0};
    e_dn_saw_pres = 1;
    if (br_u(&b, 1))
        return; /* add-data: keep previous */
    e_drc_dial = br_u(&b, 7);
    if (!e_dn_have) { /* F7: first kept frame with parsed dial */
        e_dn_dial = e_drc_dial;
        e_dn_have = 1;
    }
    if (br_u(&b, 1)) {
        info_loud_t lo;
        memset(&lo, 0, sizeof lo);
        e3_drc_loudread(&b, &lo);
        if (!e_loud_latched) {
            e_loud = lo;
            e_loud_latched = 1;
        }
        if (e9_debug())
            fprintf(stderr,
                    "LOUD i=%zu ver=%u prac=%u rel=%d/%u spch=%d/%u "
                    "strm=%d/%u maxs=%d/%u tp=%d/%u maxtp=%d/%u "
                    "rtll=%d/%u\n",
                    fi, lo.ver, lo.prac, lo.has[0], lo.val[0],
                    lo.has[1], lo.val[1], lo.has[2], lo.val[2],
                    lo.has[3], lo.val[3], lo.has[4], lo.val[4],
                    lo.has[5], lo.val[5], lo.has_rtll, lo.rtll);
    }
    unsigned dz = br_u(&b, 5);
    if (br_u(&b, 1))
        dz += br_var(&b, 3) << 5;
    size_t start = b.pos;
    unsigned gate = br_u(&b, 1);
    if (fi == 0 && !e_f0_have_gate) {
        e_f0_gate = (int)gate;
        e_f0_have_gate = 1;
    }
    if (!gate || !t->b_pres_ndot)
        return; /* carried: keep previous */
    /* ndot gate=1: fresh config. Explicit-init (no zero reliance). */
    c->valid = 0;
    c->nmode = 0;
    c->eac3 = 1;
    for (int k = 0; k < DRC_MAXMODES; k++) {
        c->modes[k].mid = -1;
        c->modes[k].is_curve = 0;
        c->modes[k].is_default = 0;
        c->modes[k].nb_lo = c->modes[k].nb_hi = 0;
        c->modes[k].npts = 0;
        c->modes[k].repeat_to = -1;
        c->modes[k].gains_cfg = -1;
        c->modes[k].has_gains = 0;
        c->modes[k].gains_nch = c->modes[k].gains_nsf =
            c->modes[k].gains_nb = 0;
    }
    unsigned nrm = br_u(&b, 3);
    unsigned nmode = nrm + 1 > 8 ? 8 : nrm + 1;
    for (unsigned k = 0; k < nmode; k++) {
        drc_mode_t *m = &c->modes[k];
        unsigned mid = br_u(&b, 3);
        m->mid = (int)mid;
        if (mid > 3) {
            (void)br_u(&b, 5);
            (void)br_u(&b, 5);
        }
        if (br_u(&b, 1)) {
            m->repeat_to = (int)br_u(&b, 3);
        } else if (br_u(&b, 1)) {
            /* default-profile leg: filled post-eac3. */
            m->is_default = 1;
        } else if (br_u(&b, 1)) {
            unsigned nblo = br_u(&b, 4), nbhi = br_u(&b, 4);
            unsigned mb = br_u(&b, 4);
            unsigned levb = 0, nrb = 0, gb = 0, lb = 0;
            if (mb) {
                levb = br_u(&b, 5);
                nrb = br_u(&b, 1);
                if (nrb) {
                    gb = br_u(&b, 4);
                    lb = br_u(&b, 5);
                }
            }
            unsigned mc = br_u(&b, 5);
            unsigned levc = 0, nrc = 0, gc = 0, lc = 0;
            if (mc) {
                levc = br_u(&b, 6);
                nrc = br_u(&b, 1);
                if (nrc) {
                    gc = br_u(&b, 5);
                    lc = br_u(&b, 5);
                }
            }
            if (!br_u(&b, 1)) { /* !tcdef: tc+thr dropped (fitted
                                 * ballistics used; wire-tc future) */
                for (int q = 0; q < 4; q++)
                    (void)br_u(&b, 8);
                if (br_u(&b, 1)) {
                    (void)br_u(&b, 5);
                    (void)br_u(&b, 5);
                }
            }
            drc_eval_curve(nblo, nbhi, mb, levb, nrb, gb, lb, mc,
                           levc, nrc, gc, lc, m->pl, m->pg,
                           &m->npts);
            m->is_curve = 1;
            m->nb_lo = (int)nblo;
            m->nb_hi = (int)nbhi;
        } else {
            m->gains_cfg = (int)br_u(&b, 2);
        }
        if (b.err)
            return; /* corrupt: previous config dropped (valid=0) */
    }
    c->nmode = (int)nmode;
    c->eac3 = (int)br_u(&b, 3);
    /* drc_data region: walked (validates size bound); SET-A decodes
     * Table-75 gainsets into the mode store (curve legs keep the
     * fitted-ballistics + wire-curve path). */
    {
        int repid[8], iscurve[8];
        for (unsigned k = 0; k < nmode; k++) {
            repid[k] = c->modes[k].repeat_to;
            iscurve[k] = c->modes[k].is_curve ||
                        c->modes[k].gains_cfg < 0;
        }
        for (unsigned k = 0; k < nmode; k++) {
            if (repid[k] < 0)
                continue;
            for (unsigned q = 0; q < nmode; q++) {
                if (q != k &&
                    c->modes[q].mid == repid[k]) {
                    iscurve[k] = iscurve[q];
                    break;
                }
            }
        }
        int anycurve = 0;
        for (unsigned k = 0; k < nmode; k++) {
            if (iscurve[k]) {
                anycurve = 1;
                continue;
            }
            unsigned gz = br_u(&b, 6);
            if (br_u(&b, 1))
                gz += br_var(&b, 2) << 6;
            unsigned ver = br_u(&b, 2);
            if (gz < 2) {
                b.err = 1;
                continue;
            }
            if (ver == 0) {
                /* SET-A: decode Table-75 gainset, skip pad to gz end. */
                size_t pend = b.pos + (gz - 2);
                int nch = t->bed_ch <= 2 ? 1 : 3;
                int rate = t->rate;
                int nsf = (rate >= 0 && rate < 14) ?
                    drc_gain_nsub(e4_spf[rate]) : -1;
                if (drc_decode_gains(&b, c->modes[k].gains_cfg, nch,
                                     nsf, &c->modes[k]) < 0)
                    b.err = 1;
                if (!b.err && b.pos <= pend)
                    (void)br_u(&b, (int)(pend - b.pos));
                if (!b.err && b.pos != pend)
                    b.err = 1;
            } else {
                /* drc2-bits (4.3.13.5.4): versioned payload skipped. */
                (void)br_u(&b, (int)(gz - 2));
            }
        }
        if (anycurve) {
            (void)br_u(&b, 1);
            (void)br_u(&b, 2);
        }
    }
    if (b.err || b.pos - start != dz)
        return; /* size-bound fail: config dropped (valid=0) */
    drc_fill_defaults(c);
    c->valid = 1;
    if (fi == 0 && !e_f0_have_dcfg) {
        e_f0_dcfg = *c;
        e_f0_have_dcfg = 1;
    }
    if (e3_shows_config((int)gate, c)) {
        if (e_ncfg == 0)
            e_icfg0 = (long)fi;
        else if (e_ncfg == 1)
            e_icfg1 = (long)fi;
        e_ncfg++;
    }
}

/* D10 P1 duck curve (A-82 Q2 bed, KDAF-bg = G4-all): 3->-0.75,
 * 6->-3.5, 9->-6.2 dB; piecewise-linear between (provisional --
 * beds pin only 3/6/9 + origin); HARD CAP at 9 (A-82 Q1:
 * de12==de9 byte-identical = Gmax). */
/* D10 P2 laws (A-85 EXACT pins; piecewise-lin between; caller caps
 * a at 9 = R3 both-signs cap). duck: bg-ch (sd=0.00). boost/cut: P2
 * compromise single laws (content-split residuals -> P3). */
/* (deleted: belonged to removed DRC-apply/DE-drive legs) */

/* (deleted: belonged to removed DRC-apply/DE-drive legs) */

/* (deleted: belonged to removed DRC-apply/DE-drive legs) */

/* Per-frame settled target (dB) for channel c. act_db/inact_db =
 * dialogue active/inactive (boost/duck or cut/unity); bg_db = bg. */
/* (de_is_dialogue deleted with DE-drive leg) */
/* (deleted: belonged to removed DRC-apply/DE-drive legs) */

/* Table-171 dialogue bits (A-79 names + A-82 confirm 1=C 6=L,R):
 * bit0=C bit1=R bit2=L. Output slots: 5.1 = L R C LFE Ls Rs;
 * stereo = L R. 1 = dialogue target (P2: boost/cut-if-active,
 * duck/unity-if-inactive, per-frame cc from activity map). */
/* (deleted: belonged to removed DE-drive leg) */

/* D47: drop de_data walk context (prior-frame decode state, Sec. 4.1).
 * Called per-stream at decode head and per-splice mid-stream. */
static void e_dewalk_reset(void) {
    e_dewalk_have = 0;
    e_dewalk_method = e_dewalk_chan = 0;
    for (int q = 0; q < 3; q++) {
        e_dewalk_prevok[q] = 0;
        for (int b = 0; b < 8; b++)
            e_dewalk_prev[q][b] = 0;
    }
}

/* ================= streaming decode core (H1) =================
 * Batch e3_decode_frames (whole-stream lr[]/actmap/spill/apf arrays +
 * buffer-all-then-fallback) is split into: ac4d_open (one-shot inits),
 * ac4d_step (bounded per-frame decode into FIFOs), drain + emit
 * pipeline (post-pass/compand incremental at a fixed frame delay),
 * ac4d_flush. No whole-stream malloc; no rewind; sticky latches from
 * the first fed frame. Gap frames in the tolerant classes (5.1 / IMS /
 * 7.1.4) latch sticky silence (batch would rewind to full stub). */

/* Gap-fallback whitelist (batch gapfb[] in main; lib scope: frame-gap
 * classification for the tolerant classes). */
static const char *const ac4d_gapfb[] = {
    "5x-acpl-unimpl", "5x-codec-reserved", "5x-cc1-unimpl",
    "5x-cc2-unimpl", "5x-cc3-unimpl", "5x-lfe-width", "snf-unimpl",
    "ssf-unimpl", "aspx-unimpl", "aspx-acpl-unimpl",
    "8ch-unimpl", "714-mode-unimpl", "714-grp-unimpl",
    "714-addch-unimpl", "714-lfe-width", NULL};
static int ac4d_note_is_gap(const char *note) {
    int q;
    if (!note)
        return 0;
    for (q = 0; ac4d_gapfb[q]; q++)
        if (strcmp(note, ac4d_gapfb[q]) == 0)
            return 1;
    return 0;
}

/* Forward declarations (defined below the step body). */
static int ac4d_step(const uint8_t *pkt, size_t pkt_len, const toc_t *toc,
                     const char **note);
static int ac4d_drain(const char **note);
static void ac4d_cmp_snapshot(void);
static int ac4d_cmp_sb1(long f, int xs);
static void ac4d_cmp_analyze(long f);
static void ac4d_cmp_flush(void);
static int ac4d_cmp_emit(long e, short (*dst)[2048]);
static int ac4d_pp_imm_emit(long e, double (*dst)[2048]);
static int ac4d_pp_5x_emit(long e, double (*dst)[2048], int *gap);
static int ac4d_latch(const toc_t *toc, const char **note);
static int ac4d_emit_one(int *is_gap);
static int ac4d_pump(void);
static int ac4d_gather(long f, short (*dst)[2048]);
static int ac4d_length_check(const char **note);

/* Bounded DN latch (batch e3_dn_latch_from: first kept P-frame (!ig)
 * at/after start, else first kept frame; a failed sample keeps the
 * prior gain). Streaming: latch the epoch-first fed frame, tentative
 * when it is I (h_dn_tent); the first fed P re-latches via
 * ac4d_dn_relatch; flush with no P keeps the tentative (= batch
 * first-kept fallback). Skeleton/pp stay pre-gain floats (gain at
 * emit), so eager analyze is exact; ready freezes while tentative
 * (no emits under a superseded gain) and analyze additionally
 * stalls when compand is engaged (its QMF state bakes the gain). */
static void ac4d_dn_latch_single(const raw_t *r, const toc_t *t) {
    ac4d_ctx *cx = _ac4d_self;
    unsigned dnb = 0;
    double dng = 0.0;
    if (cx->h_dn_tent && cx->h_dn_dirty) {
        /* Older epoch ended tentative+dirty: record its gain; a
         * later P with a different DN means batch (whose latches
         * scan across splices) used that P for the old epoch. */
        cx->h_dn_ordirty = 1;
        cx->h_dn_org0 = e3_dngain_g0;
        cx->h_dn_orgain = e3_dngain;
        cx->h_dn_orbits = e3_dn_bits;
    }
    cx->h_dn_pg0 = e3_dngain_g0;
    cx->h_dn_pgain = e3_dngain;
    cx->h_dn_pbits = e3_dn_bits;
    cx->h_dn_dirty = 0; /* new epoch: prior tentative use is settled */
    if (e3_dn_sample(r, t, &dnb, &dng)) {
        e3_dngain_g0 = dng;
        e3_dn_bits = dnb;
        e3_dngain = dng * (1.0 + e3_trim_ppm(dnb) / 1e6);
        /* EXPERIMENTAL wave11 e3-gainfloor knob (same as batch). */
        const char *gp = getenv("AC4DEC_GAINPPM");
        if (gp)
            e3_dngain *= 1.0 + strtod(gp, NULL) / 1e6;
    }
    cx->h_dn_tent = t->ig ? 1 : 0;
}

/* First-P re-latch while tentative (batch: first !ig wins; a failed
 * sample restores the pre-epoch gain, as batch's keep-prior rule). */
static void ac4d_dn_relatch(const raw_t *r, const toc_t *t) {
    ac4d_ctx *cx = _ac4d_self;
    unsigned dnb = 0;
    double dng = 0.0;
    if (e3_dn_sample(r, t, &dnb, &dng)) {
        e3_dngain_g0 = dng;
        e3_dn_bits = dnb;
        e3_dngain = dng * (1.0 + e3_trim_ppm(dnb) / 1e6);
        const char *gp = getenv("AC4DEC_GAINPPM");
        if (gp)
            e3_dngain *= 1.0 + strtod(gp, NULL) / 1e6;
    } else {
        e3_dngain_g0 = cx->h_dn_pg0;
        e3_dngain = cx->h_dn_pgain;
        e3_dn_bits = cx->h_dn_pbits;
    }
    cx->h_dn_tent = 0;
}

/* D47 splice flush, bounded (batch rescanned raws/ts; here the current
 * frame only; sample pipeline kept per Sec. 4.3). */
static void ac4d_splice_flush_single(const raw_t *r, const toc_t *t) {
    e_dewalk_reset();
    e_ax_have_cfg = 0;
    for (int q = 0; q < 4; q++)
        e_ax_xover[q] = 0;
    for (int q = 0; q < 7; q++) {
        e_ax_have_prev[q] = 0;
        e_ax_prev_stop[q] = 0;
    }
    e3_cmp_splice(); /* Sec. 3 joint rule: preserve A-side recorded slots */
    e_cmp_f0latched = 0;
    e_cmp_f0nch = e_cmp_f0sync = e_cmp_f0mask = e_cmp_f0avg = 0;
    e_5x_f0latched = 0;
    e_5x_codec = e_5x_cc = e_5x_m2 = 0;
    e_5x_mdct0 = e_5x_mdct1 = e_5x_ms = e_5x_lms = e_5x_3ch = 0;
    e_drc.valid = 0;
    e_drc_noted = 0;
    e_drc_dial = 27;
    e_decfg_have = 0;
    e_decfg_frame = 0;
    e_decfg_chan = e_decfg_tools = e_decfg_maxgain = e_decfg_method = 0;
    ac4d_dn_latch_single(r, t);
}

/* ---- public API ---- */
const char *ac4d_strerror(int rc) {
    switch (rc) {
    case AC4D_OK:
        return "ok";
    case AC4D_NEED_MORE:
        return "need more input";
    case AC4D_GAP:
        return "gap frame (silence emitted)";
    case AC4D_NO_MEM:
        return "out of memory";
    case AC4D_BAD_PACKET:
        return "bad packet";
    case AC4D_CONFIG_CHANGE:
        return "mid-stream config change";
    case AC4D_BITSTREAM:
        return "bitstream error";
    case AC4D_UNSUPPORTED:
        return "unsupported option (DRC/DE need whole-stream legs)";
    default:
        return "decode failure";
    }
}

int ac4d_open(ac4d_ctx **cp, const ac4d_opts *o) {
    ac4d_ctx *c;
    int q, ch;
    if (!cp)
        return AC4D_ERR;
    *cp = NULL;
    c = calloc(1, sizeof *c);
    if (!c)
        return AC4D_NO_MEM;
    _ac4d_self = c;
    ac4d_state_init();
    if (o)
        c->ao = *o;
    else {
        memset(&c->ao, 0, sizeof c->ao);
        c->ao.pres_sel = -1;
    }
    if (c->ao.bits == 0)
        c->ao.bits = 16;
    if (c->ao.bits != 16 && c->ao.bits != 24 && c->ao.bits != 32) {
        free(c);
        if (_ac4d_self == c)
            _ac4d_self = NULL;
        return AC4D_ERR;
    }
    if (c->ao.drc_boost_pct == 0)
        c->ao.drc_boost_pct = 100;
    if (c->ao.drc_cut_pct == 0)
        c->ao.drc_cut_pct = 100;
    /* Whole-stream legs unsupported in streaming v1 (H1). */
    {
        const char *dp = getenv("AC4DEC_DRC");
        if (c->ao.drc_on || (dp && dp[0] == '1')) {
            free(c);
            if (_ac4d_self == c)
                _ac4d_self = NULL;
            return AC4D_UNSUPPORTED;
        }
    }
    if (c->ao.de_given && c->ao.de_level != 0.0) {
        free(c);
        if (_ac4d_self == c)
            _ac4d_self = NULL;
        return AC4D_UNSUPPORTED;
    }
    /* CLI-mirror globals from opts. */
    e_drc_cli = c->ao.drc_on;
    e_drc_or_given = c->ao.drc_ref_given;
    e_drc_or_cli = (double)c->ao.drc_out_ref;
    e_drc_boost = (double)c->ao.drc_boost_pct;
    e_drc_cut = (double)c->ao.drc_cut_pct;
    e_de_given = c->ao.de_given;
    e_de_cli = c->ao.de_level;
    e_ajoc_core = c->ao.ajoc_core;
    e_wav_bps = c->ao.bits == 24 ? 3 : c->ao.bits == 32 ? 4 : 2;
    /* One-shot stream inits (batch e3_decode_frames head). */
    e_ax_have_cfg = 0;
    e3_cmp_reset();
    e_drc.valid = 0;
    e_drc_noted = 0;
    e_drc_dial = 27;
    memset(&e_loud, 0, sizeof e_loud);
    e_loud_latched = 0;
    memset(&e_audmeta, 0, sizeof e_audmeta);
    e_audmeta_latched = 0;
    e_dn_have = 0;
    e_dn_dial = 0;
    e_dn_saw_pres = 0;
    e_f0_have_gate = 0;
    e_f0_gate = 0;
    e_f0_have_dcfg = 0;
    memset(&e_f0_dcfg, 0, sizeof e_f0_dcfg);
    e_cmp_f0latched = 0;
    e_cmp_f0nch = e_cmp_f0sync = e_cmp_f0mask = e_cmp_f0avg = 0;
    e_5x_f0latched = 0;
    e_5x_codec = e_5x_cc = e_5x_m2 = 0;
    e_5x_mdct0 = e_5x_mdct1 = e_5x_ms = e_5x_lms = e_5x_3ch = 0;
    e_decfg_have = 0;
    e_decfg_frame = 0;
    e_decfg_chan = e_decfg_tools = e_decfg_maxgain = e_decfg_method = 0;
    e_dewalk_reset();
    e_ncfg = 0;
    e_icfg0 = e_icfg1 = -1;
    for (q = 0; q < 4; q++)
        e_ax_xover[q] = 0;
    for (q = 0; q < 7; q++) {
        e_ax_have_prev[q] = 0;
        e_ax_prev_stop[q] = 0;
    }
    for (ch = 0; ch < 12; ch++) {
        /* imdct_set_max deferred to latch (needs spf). */
        e3_pendn[ch] = 0;
        e3_prevn[ch] = 0;
    }
    e3_dngain = 1.0;
    e3_imm_mode = -1;
    e3_acpl_have = 0;
    acpl_reset(&c->acpl);
    for (q = 0; q < 4; q++)
        e3_spill_oc[q] = 0;
    e3_imm_fold_div = 1.4142135623730951;
    e5_acpl_nbands = 15;
    e5_acpl_fine = 1;
    e5_acpl_have = 0;
    e5_need_postpass = 0;
    for (q = 0; q < 2; q++)
        e5_spill_oc[q] = 0;
    e5_spill_on = 0;
    e5_last_acpl = 0;
    e5_acpl_elig = 1;
    e3_spill_on = 0;
    e3_imm_elig = 1;
    for (q = 0; q < 8; q++)
        c->fr_fno[q] = -1;
    acpl_imm_init(&c->pp_imm);
    acpl_5x_init(&c->pp_5x);
    for (ch = 0; ch < 8; ch++) {
        qmf_init(&c->cmp_qs[ch]);
        c->cmp_held[ch] = 1.0;
        for (q = 0; q < 16; q++)
            c->cmp_gfifo[ch][q] = 1.0;
    }
    ac4d_cmp_snapshot();
    /* EXPERIMENTAL wave11 e3-gainfloor (same as batch head). */
    {
        const char *rp = getenv("AC4DEC_REFLEVEL");
        if (rp)
            e3_reflevel = strtod(rp, NULL);
    }
    c->ao_open = 1;
    *cp = c;
    return AC4D_OK;
}

void ac4d_close(ac4d_ctx *c) {
    if (!c)
        return;
    if (_ac4d_self == c)
        _ac4d_self = NULL;
    free(c);
}

const char *ac4d_gap_note(const ac4d_ctx *c) {
    if (!c)
        return "no-ctx";
    if (c->h_gap)
        return c->h_gap;
    if (c->h_hard_note)
        return c->h_hard_note;
    return "ok";
}

static uint64_t ac4d_layout_mask(int nch) {
    /* ffmpeg AV_CH_* bit values (stable ABI). */
    switch (nch) {
    case 1:
        return 0x4; /* FRONT_CENTER */
    case 2:
        return 0x1 | 0x2; /* FL|FR */
    case 6:
        return 0x3F; /* 5.1 */
    case 8:
        return 0x63F; /* 7.1 */
    default:
        return 0;
    }
}

int ac4d_get_info(const ac4d_ctx *c, ac4d_info *i) {
    int wch;
    if (!c || !i)
        return AC4D_ERR;
    if (!c->h_fmt)
        return AC4D_NEED_MORE;
    /* (zeroext is 6 iff cm120-IMS w/o override, else 0: latch rule) */
    wch = (c->h_bed_cm == 120 && c->h_ims && c->ao.ch_override == 0)
        ? 6
        : c->h_nch;
    if (c->ao.ch_override > 0)
        wch = c->ao.ch_override;
    i->sample_rate = c->h_sr;
    i->channels = c->h_stub ? (c->ao.ch_override > 0 ? c->ao.ch_override
                                                    : c->h_nch)
                            : wch;
    i->bits = c->ao.bits;
    i->spf = c->h_spf;
    i->bed_cm = c->h_bed_cm;
    i->ch_layout_mask = ac4d_layout_mask(i->channels);
    i->real = !c->h_stub;
    return AC4D_OK;
}

/* Sticky format latch from the first fed frame. Returns AC4D_OK, or a
 * hard error code with *note set. */
static int ac4d_latch(const toc_t *toc, const char **note) {
    ac4d_ctx *c = _ac4d_self;
    int nch = 0, ims = 0, stub = 0;
    const char *snote = "stub-class";
    long out_sr;
    int ch;
    if (toc->bed_ch == 0) {
        stub = 1;
        snote = "no-bed";
        nch = 0;
    } else if (toc->bed_cm == 0) {
        nch = 1;
    } else if (toc->bed_cm == 2) {
        nch = 2;
    } else if (toc->bed_cm == 14) {
        nch = 6;
    } else if ((toc->bed_cm == 120 || toc->bed_cm == 121) &&
               toc->pres_ver0 == 2) {
        nch = 2;
        ims = 1;
    } else if (toc->bed_cm == 253) {
        nch = 8;
    } else {
        stub = 1;
        nch = toc->bed_ch > 8 ? 8 : toc->bed_ch;
    }
    if (toc->rate < 0 || toc->rate > 13) {
        *note = "rate-unimpl";
        return AC4D_BAD_PACKET;
    }
    c->h_spf = e4_spf[toc->rate];
    c->h_rate = toc->rate;
    c->h_nch = nch;
    c->h_bed_cm = toc->bed_cm;
    c->h_pres_ver = toc->pres_ver0;
    c->h_ims = ims;
    c->h_stub = stub;
    if (stub)
        c->h_gap = snote;
    /* Sample-rate label (batch main rule incl --sr override). */
    if (c->ao.sr_override > 0) {
        out_sr = c->ao.sr_override;
    } else if (toc->rate == 13) {
        out_sr = toc->fs == 0 ? 44100 : 48000;
    } else {
        out_sr = e4_sr[toc->rate];
    }
    c->h_sr = (int)out_sr;
    /* Channel override (batch want_real clash rule: mismatch = stub). */
    if (!stub && c->ao.ch_override > 0) {
        int ok = (c->ao.ch_override == nch);
        if (toc->bed_cm == 120 && ims && c->ao.ch_override == 6)
            ok = 1;
        if (toc->bed_cm == 253 && ims == 0 && c->ao.ch_override == 8)
            ok = 1;
        if (!ok) {
            c->h_stub = 1;
            c->h_gap = "ch-override-clash";
        }
    }
    /* cm120-IMS presents as 6ch (batch probe rule). */
    if (toc->bed_cm == 120 && ims && c->ao.ch_override == 0)
        e3_zeroext = 6;
    else
        e3_zeroext = 0;
    for (ch = 0; ch < 12; ch++)
        imdct_set_max(&e3_chst[ch], c->h_spf);
    /* Readiness is data-driven (h_ready); priming pushes make fixed
     * frame delays wrong (ims-class lag ~2.5 frames). Writeback reads
     * raw through (e+1)*spf+576, so emits need analyze 1 frame ahead
     * (2 when spf < 577: the +577 sample falls two frames out). */
    c->h_need = c->h_spf < 577 ? 2 : 1;
    /* Compand derived constants (batch post-pass head). */
    c->ce_slots = qmf_slots(c->h_spf);
    c->ce_G = pow(2.0, 1.0 / 0.65);
    c->ce_ex = (1.0 - 0.65) / 0.65;
    c->ce_on = (!c->ce_nocmp && (nch == 1 || nch == 2 || nch == 6) &&
                c->ce_slots > 0 && c->ce_slots <= 32 &&
                (long)c->ce_slots * 64 == (long)c->h_spf)
        ? 1
        : 0;
    c->h_imm_mode = -1;
    c->h_fmt = 1;
    return AC4D_OK;
}

int ac4d_feed(ac4d_ctx *c, const uint8_t *pkt, size_t len) {
    size_t hl = 0, rl = 0;
    int has_crc = 0;
    toc_t toc;
    const uint8_t *pay;
    const char *note = "ok";
    int rc, sel;
    raw_t r1;
    if (!c || !c->ao_open || !pkt || len == 0)
        return AC4D_ERR;
    _ac4d_self = c;
    if (c->h_hard_err)
        return c->h_hard_err;
    if (c->h_flushing)
        return AC4D_ERR;
    /* Sync + length + AC41 CRC (batch parse_sync rule, exact fit). */
    if (parse_sync(pkt, len, 0, &hl, &rl, &has_crc) != 0)
        return AC4D_BAD_PACKET;
    if (hl + rl + (has_crc ? (size_t)2 : (size_t)0) != len)
        return AC4D_BAD_PACKET;
    pay = pkt + hl;
    sel = c->ao.pres_sel < 0 ? 0 : c->ao.pres_sel;
    if (toc_parse_bv2(pay, rl, &toc, sel) != 0)
        return AC4D_BAD_PACKET;
    r1.p = pay;
    r1.len = rl;
    if (!c->h_fmt) {
        rc = ac4d_latch(&toc, &note);
        if (rc != AC4D_OK) {
            c->h_hard_err = rc;
            c->h_hard_note = note;
            return rc;
        }
        ac4d_dn_latch_single(&r1, &toc);
    } else {
        /* Config constancy (batch: geometry constancy rejects). */
        if (toc.bed_cm != c->h_bed_cm || toc.rate != c->h_rate ||
            toc.pres_ver0 != c->h_pres_ver) {
            c->h_hard_err = AC4D_CONFIG_CHANGE;
            c->h_hard_note = "config-change";
            return AC4D_CONFIG_CHANGE;
        }
        if (c->h_dn_tent && !toc.ig) {
            double tg0 = e3_dngain_g0, tg = e3_dngain;
            unsigned tb = e3_dn_bits;
            ac4d_dn_relatch(&r1, &toc);
            if ((c->h_dn_dirty &&
                 (e3_dngain_g0 != tg0 || e3_dngain != tg ||
                  e3_dn_bits != tb)) ||
                (c->h_dn_ordirty &&
                 (e3_dngain_g0 != c->h_dn_org0 ||
                  e3_dngain != c->h_dn_orgain ||
                  e3_dn_bits != c->h_dn_orbits))) {
                /* Frames went out under a superseded gain (long
                 * leading-I run + DN change, possibly across a
                 * splice): loud, never silent. */
                c->h_hard_err = AC4D_ERR;
                c->h_hard_note = "dn-relatch-dirty";
                return AC4D_ERR;
            }
        }
    }
    if (c->h_stub || c->h_gap_sticky) {
        /* Silence path: consume the ordinal, emit zeros at emit. */
        c->h_fed++;
        c->h_ready = c->h_fed;
        return AC4D_OK;
    }
    rc = ac4d_step(pay, rl, &toc, &note);
    if (rc == 0) {
        c->h_fed++;
        return AC4D_OK;
    }
    /* Step failed: tolerant class + whitelisted note = sticky gap
     * (batch would rewind to full stub); else sticky hard error. */
    {
        int tolerant =
            (c->h_nch == 6 || c->h_ims || c->h_bed_cm == 253) ? 1 : 0;
        if (tolerant && ac4d_note_is_gap(note)) {
            c->h_gap = note;
            c->h_gap_sticky = 1;
            c->h_gap_start = c->h_fed;
            c->h_fed++;
            return AC4D_OK;
        }
        if (strcmp(note, "config-change") == 0)
            rc = AC4D_CONFIG_CHANGE;
        else if (strcmp(note, "ring-overrun") == 0)
            rc = AC4D_ERR;
        else if (strcmp(note, "no-mem") == 0)
            rc = AC4D_NO_MEM;
        else
            rc = AC4D_BITSTREAM;
        c->h_hard_err = rc;
        c->h_hard_note = note;
        return rc;
    }
}

/* Bounded per-frame decode step. pkt = header-stripped payload (batch
 * raws[i]), toc = parsed TOC. Returns 0 ok / -1 (note set). */
static int ac4d_step(const uint8_t *pkt, size_t pkt_len, const toc_t *toc,
                     const char **note) {
    ac4d_ctx *cx = _ac4d_self;
    long fno = cx->h_fed;
    size_t i = (size_t)fno; /* batch-ism compat (debug prints, ordinals) */
    int nch = cx->h_nch;
    int spf = cx->h_spf;
    int fam = spf;
    raw_t r1;
    (void)fam;
    r1.p = pkt;
    r1.len = pkt_len;
        /* (batch keep-gate dropped: shim feeds kept cx->h_fed only) */

        if (cx->h_dshave) {
            int dsk = e_splice_step(cx->h_dsprev, toc->seq);
            if (dsk == SPL_MARKED || dsk == SPL_JUMP) {
                ac4d_splice_flush_single(&r1, toc); /* Sec. 4.1/4.4-4.6 */
                if (e9_debug())
                    fprintf(stderr, "SPLF i=%zu seq=%u kind=%s\n", i,
                            toc->seq,
                            dsk == SPL_MARKED ? "marked" : "jump");
            }
        }
        cx->h_dsprev = toc->seq;
        cx->h_dshave = 1;
        /* Dialnorm constancy asserted in DEBUG (latched pre-loop). */
        if (e9_debug()) {
            unsigned db = 0;
            double g = 0.0;
            if (e3_dn_sample(&r1, toc, &db, &g))
                fprintf(stderr, "DN i=%zu bits=%u g=%.8f%s\n", i, db,
                        g, g == e3_dngain_g0 ? "" : " (VAR!)");
        }
        /* DRC-config census (G-DRC step 1, parse-only, PCM-neutral). */
        int had_drc = e_drc.valid;
        drc_parse_cfg(&r1, toc); /* step 4: store (PCM-neutral) */
        if (getenv("AC4DEC_DRCDBG")) {
            e3_drc_dump(&r1, toc);
            /* Would-select witness (mpa20 discriminant: stub path
             * skips the apply leg, so selection is observed here
             * through the REAL select + REAL carried config). */
            if (e_drc.valid && !had_drc) {
                double oref = drc_outref_eff();
                int wmi = drc_select_mode(&e_drc, oref);
                fprintf(stderr,
                        "DRCSEL i=%zu outref=%.1f mi=%d mid=%d "
                        "curve=%d\n",
                        i, oref, wmi,
                        wmi >= 0 ? e_drc.modes[wmi].mid : -1,
                        wmi >= 0 ? e_drc.modes[wmi].is_curve : -1);
            }
        }
        size_t so = toc->tocB + toc->pbase;
        for (int s = 0; s < toc->bed_si; s++)
            so += toc->sizes[s];
        size_t sl = toc->sizes[toc->bed_si];
        if (so + sl > pkt_len) {
            *note = "substream-bounds";
            return -1;
        }
        { /* Bed audio-metadata keep (E8 stage-2; first-frame latch
           * for --print-info; PCM-neutral). */
            info_audmeta_t am;
            e3_audmeta_read(pkt + so, sl,
                            e3_cm_map(toc->bed_cm),
                            toc->bif, &am);
            if (i == 0 && !e_audmeta_latched && am.present) {
                e_audmeta = am;
                e_audmeta_latched = 1;
            }
            if (!e_decfg_have && am.has_de_cfg) {
                e_decfg_have = 1;
                e_decfg_frame = i;
                e_decfg_chan = am.de_chanconfig;
                e_decfg_tools = am.tools;
                e_decfg_maxgain = am.de_maxgain;
                e_decfg_method = am.de_method;
            }
            if (e9_debug())
                fprintf(stderr,
                        "AUDMETA i=%zu cm=%d/%d pres=%d endok=%d "
                        "asz=%u mat=%zu bdialog=%d tools=%u "
                        "de_gate=%d de_cfg=%d "
                        "method=%u maxgain=%u chan=%u\n",
                        i, toc->bed_cm, e3_cm_map(toc->bed_cm),
                        am.present, am.endok, am.audio_size,
                        am.meta_at, am.b_dialog, am.tools,
                        am.de_gate, am.has_de_cfg, am.de_method,
                        am.de_maxgain, am.de_chanconfig);
            /* D10 DE decision diagnostic (H-DE inputs; NO application
             * until A-82 beds pin the drive). gmax=-1 when no cfg.
             * eff = H-DE predicted gain (PROVISIONAL: min(user,Gmax)?
             * iff DI&&gate&&user!=0; A-82 decides truth). dsk =
             * post-cfg skip bits (de_data region size; Q4 variance
             * observable). Diagnostic text only -- PCM-neutral. */
            if (e9_debug()) {
                int de_gmax = am.has_de_cfg ?
                    (int)(am.de_maxgain + 1) * 3 : -1;
                int de_di = am.has_de_cfg &&
                    am.de_chanconfig != 0 ? 1 : 0;
                double de_eff = (de_di && am.de_gate &&
                        e_de_cli != 0.0 && de_gmax >= 0) ?
                    (e_de_cli < de_gmax ? e_de_cli : de_gmax) : 0.0;
                fprintf(stderr,
                        "DEDEC i=%zu user=%.1f given=%d gmax=%d chan=%u "
                        "di=%d gate=%d cfg=%d eff=%.1f dsk=%u\n",
                        i, e_de_cli, e_de_given, de_gmax,
                        am.has_de_cfg ? am.de_chanconfig : 0,
                        de_di, am.de_gate, am.has_de_cfg,
                        de_eff, am.de_skip_bits);
            }
            /* P2 T78 activity probe (diagnostic text only --
             * PCM-neutral). nch via Table 171 (own cfg, else
             * carried e_decfg). keep: 0 I-implied, wire bit
             * non-I, -1 no region. */
            if (e9_debug()) {
                int de_nch = -1;
                if (am.has_de_cfg)
                    de_nch = e3_de_nch(am.de_chanconfig);
                else if (e_dewalk_have)
                    de_nch = e3_de_nch(e_dewalk_chan);
                fprintf(stderr,
                        "DEDATA i=%zu bif=%d gate=%d cfg=%d nch=%d "
                        "keep=%d dbits=%u\n",
                        i, toc->bif, am.de_gate, am.has_de_cfg,
                        de_nch, am.de_keep, am.de_region_bits);
                if (am.de_gate) {
                    fprintf(stderr,
                            "DEPAR i=%zu ok=%d nch=%d keep=%d ms=%d "
                            "bits=%u",
                            i, am.de_par_ok, am.de_par_nch,
                            am.de_par_keep, am.de_par_ms,
                            am.de_par_bits);
                    for (int dc = 0; dc < am.de_par_nch; dc++)
                        fprintf(stderr, " c%d=%d,%d,%d,%d,%d,%d,%d,%d",
                                dc, am.de_par[dc][0], am.de_par[dc][1],
                                am.de_par[dc][2], am.de_par[dc][3],
                                am.de_par[dc][4], am.de_par[dc][5],
                                am.de_par[dc][6], am.de_par[dc][7]);
                    fprintf(stderr, "\n");
                }
            }
        }
        { /* frame_no feeds kept SAPDUMP only (A-test-hygiene-1:
           * gated, was unconditional TEMP-TUPDUMP store). */
            if (getenv("AC4DEC_SAPDUMP")) {
                extern long ac4e_frame_no;
                ac4e_frame_no = cx->h_fed;
            }
        }
        ac4_ebr_t b;
        ebr_init(&b, pkt + so, sl);
        if (e9_debug() && cx->h_fed < 1000000 &&
            toc->ns >= 1 && toc->bed_si > 0) {
            /* S0 bytes (KEPT per A-test-hygiene-1: stage-0 visibility). */
            size_t s0o = toc->tocB + toc->pbase;
            size_t s0l = toc->sizes[0] < 16 ? toc->sizes[0] : 16;
            fprintf(stderr, "S0 i=%zu s0o=%zu s0l=%zu hex=", i, s0o, s0l);
            for (size_t q = 0; q < s0l; q++)
                fprintf(stderr, "%02x", pkt[s0o + q]);
            fprintf(stderr, "\n");
        }
        /* NOTE: wave9 AC4DEC_TUPDUMP/TUPBYTES + AC4DEC_SI1DUMP
         * stripped per A-test-hygiene-1 (probe residue). */
        { /* Height-gain index (SI0 byte 15 top-2-bits, long
             * form >= 19B; LATCHED across short-SI0 cx->h_fed;
             * decode-init sqrt(2) covers short-form vectors).
             * duet6-dec-writerB Finding 12/13 ladder. */
            if (toc->ns >= 1 && toc->sizes[0] >= 19) {
                size_t gb = toc->tocB + toc->pbase;
                unsigned gidx = (pkt[gb + 15] >> 6) & 3;
                e3_imm_fold_div = 1.4142135623730951;
                if (gidx == 0)
                    e3_imm_fold_div = 1.0;
                else if (gidx == 2)
                    e3_imm_fold_div = 2.0;
                if (e9_debug())
                    fprintf(stderr, "GAINDBG i=%zu idx=%u div=%.4f\n",
                            i, gidx, e3_imm_fold_div);
            }
        }
        if (getenv("AC4DEC_SI02DUMP") && toc->ns >= 1) {
            size_t b0 = toc->tocB + toc->pbase;
            fprintf(stderr, "SI0 i=%zu n=%zu hex=", i,
                    toc->sizes[0]);
            for (size_t q = 0; q < toc->sizes[0]; q++)
                fprintf(stderr, "%02x", pkt[b0 + q]);
            fprintf(stderr, "\n");
            if (toc->ns > 2) {
                size_t o2 = b0 + toc->sizes[0] + toc->sizes[1];
                fprintf(stderr, "SI2 i=%zu n=%zu hex=", i,
                        toc->sizes[2]);
                for (size_t q = 0; q < toc->sizes[2]; q++)
                    fprintf(stderr, "%02x", pkt[o2 + q]);
                fprintf(stderr, "\n");
            }
        }
        unsigned asz = ebr_u(&b, 15);
        if (ebr_u(&b, 1))
            asz += e3_var7(&b) << 15;
        ebr_align(&b);
        if (e9_debug() && cx->h_fed < 1000000)
            fprintf(stderr,
                    "FRM i=%zu seq=%u so=%zu sl=%zu asz=%u alpos=%zu "
                    "tocB=%zu pbase=%zu ns=%d s0=%zu s1=%zu\n",
                    i, toc->seq, so, sl, asz, b.pos, toc->tocB,
                    toc->pbase, toc->ns, toc->sizes[0],
                    toc->ns > 1 ? toc->sizes[1] : 0);
        if (b.err) {
            *note = "lead-trunc";
            return -1;
        }
        /* ctx-moved: static ch -> xm_ch */
        asf_info_t info[12];
        if (nch <= 2 || nch == 6)
            e3_cmp_begin(); /* per-frame compand slot (mono/stereo/5x) */
        if (nch == 6) { /* 5.1 bed: e3_decode_5x fills out slots */
            int r5 = e3_decode_5x(&b, spf, toc->seq, toc->bif, asz,
                                      (_ac4d_self->xm_ch), info, note);
            if (r5 != 0)
                return -1;
        } else if (nch == 8) { /* 7.1 mains of a 7.1.4 bed (writerB 714:
                                * heights never consumed) */
            if (toc->bed_cm != 253) {
                *note = "8ch-unimpl";
                return -1;
            }
            int r7 = e3_decode_714(&b, spf, toc->seq, toc->ig, (_ac4d_self->xm_ch),
                                   info, note);
            if (r7 < 0)
                return -1;
            e3_imm_mode = r7;
            if (r7 != 3)
                e3_imm_elig = 0;
        } else if (nch == 1) {
            unsigned mcm = ebr_u(&b, 1); /* mono_codec_mode */
            int m_aspx = 0;
            if (mcm == 1) {
                m_aspx = 1;
                if (e_ax_head(&b, toc->bif, 1, note))
                    return -1;
            } else if (mcm != 0) {
                *note = "aspx-unimpl";
                return -1;
            }
            if (ebr_u(&b, 1) != 0) { /* spec_frontend */
                *note = "ssf-unimpl";
                return -1;
            }
            int r = asf_info_parse(&b, spf, &info[0]);
            if (r != AC4E_OK) {
                *note = "sfinfo-fail";
                return -1;
            }
            r = asf_chan_decode(&b, &info[0], spf, toc->seq, &(_ac4d_self->xm_ch)[0]);
            if (r == AC4E_SNF) {
                *note = "snf-unimpl";
                return -1;
            }
            if (r != AC4E_OK) {
                *note = "entropy-fail";
                return -1;
            }
            if (m_aspx) {
                char axst[64];
                int ax_ts = e_ax_timeslots(spf);
                size_t ax_end;
                axst[0] = '\0';
                if (ax_ts < 0) {
                    *note = "aspx-noflb";
                    return -1;
                }
                if (e_ax_data_1ch(&b, toc->bif, ax_ts, 0, 0, axst,
                                  sizeof axst, note))
                    return -1;
                e3_cmp_endxov();
                ax_end = ((size_t)asz + 2) * 8;
                if (b.err || b.pos > ax_end || ax_end - b.pos > 7) {
                    *note = "aspx-endpos";
                    return -1;
                }
                if (e9_debug() && cx->h_fed < 1000000)
                    fprintf(stderr, "ASPX1 i=%zu ig=%d %s gap=%zu\n", i,
                            toc->bif, axst, ax_end - b.pos);
            }
        } else {
            unsigned scm = ebr_u(&b, 2); /* stereo_codec_mode */
            if (e9_debug() && cx->h_fed < 3)
                fprintf(stderr, "SCM i=%zu scm=%u\n", i, scm);
            int is_aspx = 0;
            if (scm == 1) {
                is_aspx = 1;
                if (e_ax_head(&b, toc->bif, 2, note))
                    return -1;
            } else if (scm != 0) {
                *note = "aspx-acpl-unimpl";
                return -1;
            }
            int use_sap = (int)ebr_u(&b, 1); /* b_enable_mdct_stereo_proc */
            if (use_sap) {
                /* Table 23 SAP branch: frontends implied ASF, ONE
                 * common sf_info + chparam_info, then 2x sf_data. */
                int r = asf_info_parse(&b, spf, &info[0]);
                if (r != AC4E_OK) {
                    *note = "sfinfo-fail-sap";
                    return -1;
                }
                info[1] = info[0];
                /* ctx-moved: static sap -> xm_sap */
                r = ac4e_chparam_parse(&b, &info[0], &(_ac4d_self->xm_sap));
                if (r != AC4E_OK) {
                    *note = "chparam-fail";
                    return -1;
                }
                for (int c = 0; c < 2; c++) {
                    r = asf_chan_decode(&b, &info[0], spf, toc->seq,
                                        &(_ac4d_self->xm_ch)[c]);
                    if (r == AC4E_SNF) {
                        *note = "snf-unimpl";
                        return -1;
                    }
                    if (r != AC4E_OK) {
                        *note = "entropy-fail";
                        return -1;
                    }
                }
                ac4e_sap_apply(&info[0], &(_ac4d_self->xm_sap), &(_ac4d_self->xm_ch)[0], &(_ac4d_self->xm_ch)[1]);
            } else {
                for (int c = 0; c < 2; c++) {
                    if (ebr_u(&b, 1) != 0) {
                        *note = "ssf-unimpl";
                        return -1;
                    }
                    int r = asf_info_parse(&b, spf, &info[c]);
                    if (r != AC4E_OK) {
                        *note = "sfinfo-fail";
                        return -1;
                    }
                }
                for (int c = 0; c < 2; c++) {
                    int r = asf_chan_decode(&b, &info[c], spf, toc->seq,
                                            &(_ac4d_self->xm_ch)[c]);
                    if (r == AC4E_SNF) {
                        *note = "snf-unimpl";
                        return -1;
                    }
                    if (r != AC4E_OK) {
                        *note = "entropy-fail";
                        return -1;
                    }
                }
            }
            if (is_aspx) {
                char axst[64];
                int ax_ts = e_ax_timeslots(spf);
                size_t ax_end;
                axst[0] = '\0';
                if (ax_ts < 0) {
                    *note = "aspx-noflb";
                    return -1;
                }
                if (e_ax_data_2ch(&b, toc->bif, ax_ts, 0, 0, axst,
                                  sizeof axst, note))
                    return -1;
                e3_cmp_endxov();
                ax_end = ((size_t)asz + 2) * 8;
                if (b.err || b.pos > ax_end || ax_end - b.pos > 7) {
                    *note = "aspx-endpos";
                    return -1;
                }
                if (e9_debug() && cx->h_fed < 1000000 &&
                    toc->bif)
                    fprintf(stderr,
                            "ASPXCFG i=%zu q%d st%d/sp%d m%d nb%d eb%d fr%d\n",
                            i, e_ax_cfg.qmode, e_ax_cfg.start,
                            e_ax_cfg.stop, e_ax_cfg.mfscale,
                            e_ax_cfg.noisesbg, e_ax_cfg.envbits,
                            e_ax_cfg.freqres);
                if (e9_debug() && cx->h_fed < 1000000)
                    fprintf(stderr, "ASPX i=%zu ig=%d %s gap=%zu\n", i,
                            toc->bif, axst, ax_end - b.pos);
            }
        }
        if (e9_debug() && cx->h_fed < 1000000) {
            /* DNPEEK dialnorm 7b @ audio end (KEPT per A-test-hygiene-1). */
            size_t sv = b.pos;
            int sv_e = b.err;
            b.pos = (size_t)(asz + 2) * 8;
            b.err = 0;
            unsigned dn = ebr_u(&b, 7);
            int dn_e = b.err;
            b.pos = (size_t)(asz + 3) * 8;
            b.err = 0;
            unsigned dn3 = ebr_u(&b, 7);
            int dn3_e = b.err;
            b.pos = sv;
            b.err = sv_e;
            fprintf(stderr, "END i=%zu pos=%zu aszend=%u dn=%u%s dn3=%u%s\n",
                    i, sv, (asz + 2) * 8, dn, dn_e ? "(oob)" : "", dn3,
                    dn3_e ? "(oob)" : "");
        }
        if (b.err) {
            *note = "elem-trunc";
            return -1;
        }
        /* push windows with exact nn via one-window delay.
         * Per-channel loops (wave16 p2-unstub): LFE is long-only
         * while mains may switch short, so nwin may differ per
         * channel; stereo output is unchanged (pushes are
         * per-channel independent; only PCMDUMP/CODUMP record
         * order changes, debug-env only). */
        e3_spill_on = 0;
        if (nch == 8 && e3_imm_mode == 3) {
            int all1 = 1;
            for (int q = 4; q < 8 && all1; q++)
                if ((_ac4d_self->xm_ch)[q].nwin != 1 || !info[q].b_long)
                    all1 = 0;
            e3_spill_on = all1;
            if (!all1)
                e3_imm_elig = 0;
        }
        e5_spill_on = 0;
        if (nch == 6 && e5_last_acpl) {
            int all5 = 1;
            for (int q = 0; q < 2 && all5; q++)
                if ((_ac4d_self->xm_ch)[q].nwin != 1 || !info[q].b_long)
                    all5 = 0;
            e5_spill_on = all5;
            if (!all5)
                e5_acpl_elig = 0;
        }
        if (nch == 8 && e3_imm_mode == 0) {
            /* Stream-mix path: every mode-0 frame pushes all 12 tracks
             * natively to float streams; the Table-23 + fold mix runs
             * once over full streams post-drain (e3_mix12p). */
            /* H1: tf FIFO always present (fixed ring, was lazy
             * whole-stream malloc). */
            cx->tf_used = 1;
            for (int c = 0; c < 12; c++) {
                int advances = 0;
                for (int w = 0; w < (_ac4d_self->xm_ch)[c].nwin; w++) {
                    int nn = (_ac4d_self->xm_ch)[c].win_len[w];
                    if (e3_pendn[c] > 0) {
                        int k = 0;
                        const char *e = e3_push_win(c, e3_pendco[c],
                                                    e3_pendn[c], nn, fam,
                                                    e3_s16, &k);
                        if (e) {
                            *note = e;
                            return -1;
                        }
                        ac4d_rput_d(cx->tf_fifo[c], AC4D_SKN, cx->h_oc[c],
                                    e3_pcmbuf, k);
                        cx->h_oc[c] += k;
                    }
                    e3_pendn[c] = nn;
                    memcpy(e3_pendco[c], (_ac4d_self->xm_ch)[c].co[w],
                           (size_t)nn * sizeof(double));
                    advances += nn;
                }
                if (advances != spf) {
                    *note = "advance-mismatch";
                    return -1;
                }
            }
        } else {
            for (int c = 0; c < nch; c++) {
                int advances = 0;
                for (int w = 0; w < (_ac4d_self->xm_ch)[c].nwin; w++) {
                    int nn = (_ac4d_self->xm_ch)[c].win_len[w];
                    if (e3_pendn[c] > 0) {
                        int k = 0;
                        const char *e = e3_push_win(c, e3_pendco[c], e3_pendn[c],
                                                    nn, fam, e3_s16, &k);
                        if (e) {
                            *note = e;
                            return -1;
                        }
                        for (int j = 0; j < k; j++) {
                            double v =
                                (nch == 8 && e3_imm_mode >= 0 && c >= 6 && !getenv("AC4DEC_KEEP67"))
                                    ? 0.0
                                    : e3_pcmbuf[j];
                            cx->sk_fifo[c][(cx->h_oc[c] + j) % AC4D_SKN] = v;
                        }
                        cx->h_oc[c] += k;
                    }
                    e3_pendn[c] = nn;
                    memcpy(e3_pendco[c], (_ac4d_self->xm_ch)[c].co[w],
                           (size_t)nn * sizeof(double));
                    advances += nn;
                }
                if (advances != spf) {
                    *note = "advance-mismatch";
                    return -1;
                }
            }
        }
        /* (P2 DE activity record deleted: DE-on unsupported in v1;
         * the per-frame walk still runs above, PCM-neutral.) */
        /* NOTE: wave9 AC4DEC_STOP_AFTER truncation stripped per
         * A-test-hygiene-1 (behavior-altering probe residue). */
    if (ac4d_pump() != 0) {
        *note = "ring-overrun";
        return -1;
    }
    return 0;
}

/* Skeleton drain at flush (batch end-of-decode drain; pushes pendings
 * + OLA-ring flush into the FIFOs, same order/values). */
static int ac4d_drain(const char **note) {
    ac4d_ctx *cx = _ac4d_self;
    int nch = cx->h_nch;
    int spf = cx->h_spf;
    int fam = spf;
    (void)fam;
    (void)spf;
    /* end: push pendings with nn=n, flush, drain all */
    if (cx->tf_used) {
        /* Stream-mix drain: pending + flush to float streams. */
        for (int c = 0; c < 12; c++) {
            if (e3_pendn[c] > 0) {
                int k = 0;
                const char *e = e3_push_win(c, e3_pendco[c], e3_pendn[c],
                                            e3_pendn[c], fam, e3_s16, &k);
                if (e) {
                    *note = e;
                    return -1;
                }
                ac4d_rput_d(cx->tf_fifo[c], AC4D_SKN, cx->h_oc[c],
                                e3_pcmbuf, k);
                cx->h_oc[c] += k;
                e3_pendn[c] = 0;
            }
            int k = imdct_flush(&e3_chst[c], e3_pcmbuf);
            if (k > 0) {
                ac4d_rput_d(cx->tf_fifo[c], AC4D_SKN, cx->h_oc[c],
                                e3_pcmbuf, k);
                cx->h_oc[c] += k;
            }
        }
    } else {
        for (int c = 0; c < nch; c++) {
            if (e3_pendn[c] > 0) {
                int k = 0;
                const char *e = e3_push_win(c, e3_pendco[c], e3_pendn[c],
                                            e3_pendn[c], fam, e3_s16, &k);
                if (e) {
                    *note = e;
                    return -1;
                }
                for (int j = 0; j < k; j++) {
                    double v =
                        (nch == 8 && e3_imm_mode >= 0 && c >= 6 && !getenv("AC4DEC_KEEP67"))
                            ? 0.0
                            : e3_pcmbuf[j];
                    cx->sk_fifo[c][(cx->h_oc[c] + j) % AC4D_SKN] = v;
                }
                cx->h_oc[c] += k;
                e3_pendn[c] = 0;
            }
            int k = imdct_flush(&e3_chst[c], e3_pcmbuf);
            { /* PCMDUMP flush (KEPT per A-test-hygiene-1): one record
                 * per channel, header kk=-k distinguishes it from push
                 * records (k>=0); payload is the k flush doubles. */
                const char *pp = getenv("AC4DEC_PCMDUMP");
                if (pp) {
                    FILE *pf = fopen(pp, "ab");
                    if (pf) {
                        int kk = -k;
                        fwrite(&kk, sizeof kk, 1, pf);
                        fwrite(e3_pcmbuf, sizeof(double),
                               (size_t)(k > 0 ? k : 0), pf);
                        fclose(pf);
                    }
                }
            }
            if (e3_spill_on && c >= 4 && c < 8 && k > 0) {
                /* Drain-tail spill (post-pass mirror). */
                int q = c - 4;
                ac4d_rput_d(cx->sp3[q], AC4D_SPN, e3_spill_oc[q],
                                e3_pcmbuf, k);
                e3_spill_oc[q] += k;
            }
            if (e5_spill_on && c >= 0 && c < 2 && k > 0) {
                /* 5x drain-tail spill (mirror). */
                ac4d_rput_d(cx->sp5[c], AC4D_SPN, e5_spill_oc[c],
                                e3_pcmbuf, k);
                e5_spill_oc[c] += k;
            }
            for (int j = 0; j < k; j++) {
                double v =
                    (nch == 8 && e3_imm_mode >= 0 && c >= 6 && !getenv("AC4DEC_KEEP67"))
                        ? 0.0
                        : e3_pcmbuf[j];
                cx->sk_fifo[c][(cx->h_oc[c] + j) % AC4D_SKN] = v;
            }
            cx->h_oc[c] += k;
        }
    }
    if (cx->tf_used) {
        /* Stream-mix uniformity (mix itself runs per-frame at
         * consume; total-length check happens at flush end). */
        long outs = cx->h_oc[0];
        for (int c = 1; c < 12; c++) {
            if (cx->h_oc[c] != outs) {
                *note = "streamlen-mismatch";
                return -1;
            }
        }
        (void)outs;
    }
    return 0;
}

/* Length-contract check at flush end (batch: end-of-decode check).
 * Skipped under sticky silence (batch emitted no skeleton then). */
static int ac4d_length_check(const char **note) {
    ac4d_ctx *cx = _ac4d_self;
    int nch = cx->tf_used ? 12 : cx->h_nch;
    long outsamp, expect;
    int c;
    if (cx->h_gap_sticky)
        return 0;
    outsamp = cx->h_oc[0];
    for (c = 1; c < nch; c++) {
        if (cx->h_oc[c] != outsamp) {
            *note = "ch-length-mismatch";
            return -1;
        }
    }
    expect = cx->h_fed * (long)cx->h_spf;
    if (outsamp != expect) {
        *note = "length-mismatch";
        return -1;
    }
    return 0;
}

/* Immersive mode-3 post-pass for ONE emit frame e (batch: whole-stream
 * loop). dst = emit-stage s16 frame (skeleton in, surrounds overwritten).
 * Runs the identical QMF chain in the identical frame order; spill reads
 * use the current absolute oc as the zero-pad bound (batch outsamp).
 * Heights artifact (AC4DEC_HEIGHTS) dropped in streaming (side file). */
static int ac4d_pp_imm_emit(long e, double (*dst)[2048]) {
    ac4d_ctx *cx = _ac4d_self;
    static const double UNSC = 2.0 * 1.4142135623730951;
    int spf = cx->h_spf;
    int slot = (int)(e % AC4D_APFN);
    long base = e * (long)spf;
    double *blkD = cx->pp_blk[0], *blkE = cx->pp_blk[1];
    double *blkF = cx->pp_blk[2], *blkG = cx->pp_blk[3];
    double *blkL = cx->pp_blk[4], *blkR = cx->pp_blk[5];
    int j;
    if (!e3_imm_elig)
        return 0; /* skeleton stands (batch: global skip) */
    if (!cx->ap3[slot].have || cx->ap3[slot].fno != e)
        return 0; /* no params this frame: skeleton stands */
    for (int q = 0; q < 4; q++)
        if (e3_spill_oc[q] < (e + 1) * (long)spf)
            return 0; /* spill short: skeleton stands (batch-ward) */
    /* Spill overwrite tripwire (oldest actually-read sample). */
    for (int q = 0; q < 4; q++)
        if (base + 577 < e3_spill_oc[q] &&
            e3_spill_oc[q] - (base + 577) > AC4D_SPN)
            return -1;
    /* IN_OFF = +577 (BANK_DELAY, same as batch). */
    for (j = 0; j < spf; j++) {
        long s = base + j + 577;
        double d = 0, ee = 0, ff = 0, g = 0;
        if (s < e3_spill_oc[0])
            d = ac4d_rget_d(cx->sp3[0], AC4D_SPN, s) / UNSC;
        if (s < e3_spill_oc[1])
            ee = ac4d_rget_d(cx->sp3[1], AC4D_SPN, s) / UNSC;
        if (s < e3_spill_oc[2])
            ff = ac4d_rget_d(cx->sp3[2], AC4D_SPN, s);
        if (s < e3_spill_oc[3])
            g = ac4d_rget_d(cx->sp3[3], AC4D_SPN, s);
        blkD[j] = d;
        blkE[j] = ee;
        blkF[j] = ff;
        blkG[j] = g;
    }
    acpl_imm_frame(&cx->pp_imm, blkD, blkE, blkF, blkG, cx->ap3[slot].m,
                   spf, cx->ap3[slot].fold_div, blkL, blkR, NULL,
                   &cx->acpl);
    for (j = 0; j < spf; j++) {
        dst[4][j] = blkL[j]; /* pre-gain: gain at emit */
        dst[5][j] = blkR[j];
    }
    if (e9_debug())
        fprintf(stderr, "IMMPOST f=%ld applied oc=%ld/%ld\n", e,
                e3_spill_oc[0], e3_spill_oc[1]);
    return 0;
}

/* 5x mode-3 post-pass for ONE emit frame e. Skeleton L/R/Ls/Rs
 * overwritten on success. Sets *gap when batch would fail the whole
 * decode (elig killed with params present) so the caller latches
 * sticky silence (batch: rewind to full stub). */
static int ac4d_pp_5x_emit(long e, double (*dst)[2048], int *gap) {
    ac4d_ctx *cx = _ac4d_self;
    int spf = cx->h_spf;
    int slot = (int)(e % AC4D_APFN);
    long base = e * (long)spf;
    double *bL = cx->pp_blk[0], *bR = cx->pp_blk[1];
    double *bOL = cx->pp_blk[2], *bOR = cx->pp_blk[3];
    double *bOLs = cx->pp_blk[4], *bORs = cx->pp_blk[5];
    int j;
    *gap = 0;
    if (!cx->ap5[slot].have || cx->ap5[slot].fno != e)
        return 0; /* no params this frame: skeleton stands */
    if (!e5_acpl_elig) {
        *gap = 1; /* batch: -1 5x-acpl-unimpl -> full stub */
        return 0;
    }
    for (int q = 0; q < 2; q++)
        if (e5_spill_oc[q] < (e + 1) * (long)spf) {
            *gap = 1; /* batch: spill-short -> -1 -> full stub */
            return 0;
        }
    /* Spill overwrite tripwire (oldest actually-read sample). */
    for (int q = 0; q < 2; q++)
        if (base + 577 < e5_spill_oc[q] &&
            e5_spill_oc[q] - (base + 577) > AC4D_SPN)
            return -1;
    for (j = 0; j < spf; j++) {
        long s = base + j + 577;
        bL[j] = s < e5_spill_oc[0]
            ? ac4d_rget_d(cx->sp5[0], AC4D_SPN, s)
            : 0.0;
        bR[j] = s < e5_spill_oc[1]
            ? ac4d_rget_d(cx->sp5[1], AC4D_SPN, s)
            : 0.0;
    }
    acpl_5x_frame(&cx->pp_5x, bL, bR, cx->ap5[slot].m, spf, bOL, bOR,
                  bOLs, bORs, &cx->acpl);
    for (j = 0; j < spf; j++) {
        dst[0][j] = bOL[j]; /* pre-gain: gain at emit */
        dst[1][j] = bOR[j];
        dst[4][j] = bOLs[j];
        dst[5][j] = bORs[j];
    }
    if (e9_debug())
        fprintf(stderr, "5XPOST f=%ld applied\n", e);
    return 0;
}
/* Compand env snapshot (batch read once per decode; CMPLEAD
 * dropped: whole-stream pre-pass has no streaming form). */
static void ac4d_cmp_snapshot(void) {
    ac4d_ctx *cx = _ac4d_self;
        const char *ncp = getenv("AC4DEC_NOCMP");
        cx->ce_nocmp = ncp && ncp[0] == '1';
        const char *ucp = getenv("AC4DEC_CMPUNITY");
        cx->ce_unity = ucp && ucp[0] == '1';
        const char *icp = getenv("AC4DEC_CMPINV");
        cx->ce_inv = icp && icp[0] == '1';
        const char *ccp = getenv("AC4DEC_CMPCENSUS");
        cx->ce_census = ccp && ccp[0] == '1';
        const char *dcp = getenv("AC4DEC_CMPDUMP");
        cx->ce_dumpf = dcp ? strtol(dcp, NULL, 10) : -1;
        const char *fip = getenv("AC4DEC_CMPFULLINT");
        cx->ce_fullint = fip && fip[0] == '1';
        const char *aap = getenv("AC4DEC_CMPAVGALL");
        cx->ce_avgall = aap && aap[0] == '1';
        const char *ghp = getenv("AC4DEC_CMPGAINHOLD");
        cx->ce_gainhold = ghp && ghp[0] == '1';
        const char *gdp = getenv("AC4DEC_CMPGDEL");
        cx->ce_gdel = gdp ? strtol(gdp, NULL, 10) : 0;
        if (cx->ce_gdel < 0 || cx->ce_gdel > 16)
            cx->ce_gdel = 0;
        const char *usp = getenv("AC4DEC_CMPUNSHIFT");
        cx->ce_unshift = usp && usp[0] == '1';
        const char *pwp = getenv("AC4DEC_CMPPREWARM");
        cx->ce_prewarm = pwp ? strtol(pwp, NULL, 10) : 0;
        if (cx->ce_prewarm < 0 || cx->ce_prewarm > 4)
            cx->ce_prewarm = 0;
        const char *oap = getenv("AC4DEC_CMPONSETAVG");
        cx->ce_onsetavg = oap && oap[0] == '1';
        const char *skp = getenv("AC4DEC_CMPFLOORSKIP");
        cx->ce_floorskip = skp && skp[0] == '1';
        const char *nfp = getenv("AC4DEC_CMPFLAT");
        cx->ce_useflat = nfp && nfp[0] == '1';
        const char *fop = getenv("AC4DEC_CMPFRAMEOFF");
        cx->ce_frameoff = fop ? strtol(fop, NULL, 10) : -1;
        const char *fo2 = getenv("AC4DEC_CMPFRAMEOF2");
        cx->ce_frameof2 = fo2 ? strtol(fo2, NULL, 10) : -1;
        const char *flp = getenv("AC4DEC_CMPFOL");
        cx->ce_fol_a = 1.0;
        cx->ce_fol_r = 1.0;
        cx->ce_usofol = 0;
        if (flp && flp[0]) {
            double a = 1.0, r = 1.0;
            if (sscanf(flp, "%lf,%lf", &a, &r) == 2 && a > 0.0 &&
                a <= 1.0 && r > 0.0 && r <= 1.0) {
                cx->ce_fol_a = a;
                cx->ce_fol_r = r;
                cx->ce_usofol = (a < 1.0 || r < 1.0);
            }
        }
        const char *dmp = getenv("AC4DEC_CMPDAMP");
        cx->ce_dampf = dmp ? strtod(dmp, NULL) : -1.0;
        if (cx->ce_dampf < 0.0 || cx->ce_dampf > 1.0)
            cx->ce_dampf = -1.0;
        /* Hot-gap-fill (duet26 Track-G LANDING, P1g/A-46):
         * default ON (kill-switch AC4DEC_CMPHOTFILL=0).
         * Gap slots take computed-m/mavg iff >= 1.0. Heals
         * M2-notch + M2/M7-trail to DRP-exact (M2 15.5->2.4,
         * M7 11.7->2.5), keeps s2s-cliff (f48 cold-kept). */
        const char *hfp = getenv("AC4DEC_CMPHOTFILL");
        cx->ce_hotfill = !(hfp && hfp[0] == '0');
        const char *slp = getenv("AC4DEC_CMPSLEW");
        cx->ce_slewrate = slp ? strtod(slp, NULL) : -1.0;
        if (cx->ce_slewrate < 0.0)
            cx->ce_slewrate = -1.0;
        /* CMPUNITYF=f[,ts0,ts1] (duet26 atomic fork-(i), v2 range):
         * frame-f (slots [ts0,ts1), default whole frame) post-pass
         * OFF (Q passthrough, bank-transparent NOCMP). Default
         * unset = untouched. */
        const char *ufp = getenv("AC4DEC_CMPUNITYF");
        cx->ce_unityf = -1;
        cx->ce_unityt0 = 0;
        cx->ce_unityt1 = 32;
        if (ufp && ufp[0]) {
            long a = -1, b = 0, c = 32;
            int nn = sscanf(ufp, "%ld,%ld,%ld", &a, &b, &c);
            if (nn >= 1 && a >= 0) {
                cx->ce_unityf = a;
                if (nn == 3 && b >= 0 && c > b) {
                    cx->ce_unityt0 = b;
                    cx->ce_unityt1 = c;
                }
            }
        }
    /* FADE0 (batch: once per decode; applied per emit frame). */
    {
        const char *fp = getenv("AC4DEC_FADE0");
        cx->ce_fade0 = fp ? strtol(fp, NULL, 10) : 0;
    }
}

/* sb1 for one (frame, xslot) (batch: precomputed table). */
static int ac4d_cmp_sb1(long f, int xs) {
    int sb1 = -1;
    if (f < e3_cmp_n && e3_cmp_xov[xs][f] >= 0) {
        e_ax_cfg_t cf;
        int tab[23], num = 0, sbz = 0;
        memset(&cf, 0, sizeof cf);
        cf.mfscale = e3_cmp_mfs[f];
        cf.start = e3_cmp_sta[f];
        cf.stop = e3_cmp_sto[f];
        if (e_ax_master(&cf, tab, &num, &sbz) == 0 &&
            e3_cmp_xov[xs][f] <= num)
            sb1 = tab[e3_cmp_xov[xs][f]];
    }
    if (sb1 < 1 || sb1 > 64)
        sb1 = -1;
    return sb1;
}
/* Scoped short names for the compand bodies (batch locals kept
 * verbatim; continuous state + env snapshot live in the ctx). */
#define cmp_fp (cx->cmp_fp)
#define cmp_Q (cx->cmp_Q)
#define fp cmp_fp
#define Q cmp_Q
#define qrun (cx->cmp_qrun)
#define held (cx->cmp_held)
#define folsm (cx->cmp_folsm)
#define slewsm (cx->cmp_slewsm)
#define gfifo (cx->cmp_gfifo)
#define gatefifo (cx->cmp_gatefifo)
#define gpos (cx->cmp_gpos)
#define cmpG (cx->ce_G)
#define cmpex (cx->ce_ex)
#define G cmpG
#define ex cmpex
#define slots (cx->ce_slots)
#define unity (cx->ce_unity)
#define inv (cx->ce_inv)
#define census (cx->ce_census)
#define dumpf (cx->ce_dumpf)
#define fullint (cx->ce_fullint)
#define avgall (cx->ce_avgall)
#define gainhold (cx->ce_gainhold)
#define gdel (cx->ce_gdel)
#define unshift (cx->ce_unshift)
#define prewarm (cx->ce_prewarm)
#define onsetavg (cx->ce_onsetavg)
#define floorskip (cx->ce_floorskip)
#define useflat (cx->ce_useflat)
#define frameoff (cx->ce_frameoff)
#define frameof2 (cx->ce_frameof2)
#define fol_a (cx->ce_fol_a)
#define fol_r (cx->ce_fol_r)
#define usofol (cx->ce_usofol)
#define dampf (cx->ce_dampf)
#define hotfill (cx->ce_hotfill)
#define slewrate (cx->ce_slewrate)
#define unityf (cx->ce_unityf)
#define unityt0 (cx->ce_unityt0)
#define unityt1 (cx->ce_unityt1)

/* Compand analyze for ONE frame f (batch: per-f iteration of the
 * apply loop). Input = staged frame in fr_s16 (postpassed for 5x);
 * the QMF bank runs continuously from frame 0 exactly as in batch.
 * Always-run when eligible (ce_on): off-frames take no writeback so
 * bytes match the batch whole-stream OR gate. */
static void ac4d_cmp_analyze(long f) {
    ac4d_ctx *cx = _ac4d_self;
    int nch = cx->h_nch;
    int spf = cx->h_spf;
    double trim = e3_dngain * 32768.0;
    double pre = trim != 0.0 ? 1.0 / trim / 32768.0 : 1.0 / 32768.0;
    (void)trim;
    if (!cx->ce_on)
        return;
    if (census && f < e3_cmp_n)
        fprintf(stderr,
                "CMPC f=%ld on=%u avg=%u syn=%u xov=%d/%d/%d "
                "cfg=%d/%d/%d codec=%d m2=%d\n",
                f, e3_cmp_onm[f], e3_cmp_avg[f], e3_cmp_syn[f],
                e3_cmp_xov[0][f], e3_cmp_xov[1][f],
                e3_cmp_xov[2][f], e3_cmp_mfs[f], e3_cmp_sta[f],
                e3_cmp_sto[f], e3_cmp_codec[f], e3_cmp_m2[f]);
                        int syn = (f < e3_cmp_n) ? e3_cmp_syn[f] : 0;
                        int codec = (f < e3_cmp_n) ? e3_cmp_codec[f] : 0;
                        int m2v = (f < e3_cmp_n) ? e3_cmp_m2[f] : -1;
                        int ci_of[8], w_of[8], xs_of[8];
                        for (int c = 0; c < nch; c++)
                            ci_of[c] = e3_cmp_slotmap(nch, codec, m2v, c,
                                                     &w_of[c], &xs_of[c]);
                        /* CMPSLEW interval mirror (duet26): per-(_ac4d_self->xm_ch)
                         * [st0,st1) + chon, same derivation as the
                         * apply loop (fullint/frameoff honored). */
                        int st0[8], st1[8], chonv[8];
                        {
                            int sonm = (f < e3_cmp_n) ? e3_cmp_onm[f] : 0;
                            for (int c = 0; c < 8; c++) {
                                st0[c] = 0;
                                st1[c] = slots;
                                chonv[c] = 0;
                            }
                            for (int c = 0; c < nch; c++) {
                                int ci = ci_of[c];
                                if (ci >= 0)
                                    chonv[c] = (sonm >> ci) & 1;
                                int wire = w_of[c];
                                if (fullint) {
                                    st0[c] = 0;
                                    st1[c] = slots;
                                } else if (f < e3_cmp_n &&
                                           wire >= 0 && wire < 5) {
                                    int axt = e_ax_timeslots(spf);
                                    int nnta =
                                        axt > 0 ? slots / axt : 1;
                                    if (nnta < 1)
                                        nnta = 1;
                                    st0[c] =
                                        e3_cmp_i0[wire][f] * nnta;
                                    st1[c] =
                                        e3_cmp_i1[wire][f] * nnta;
                                    if (st0[c] < 0)
                                        st0[c] = 0;
                                    if (st1[c] > slots)
                                        st1[c] = slots;
                                    if (st1[c] <= st0[c]) {
                                        st0[c] = 0;
                                        st1[c] = slots;
                                    }
                                }
                            }
                        }
                        int slewskip =
                            (f == frameoff || f == frameof2);
                        double gg[8][32];
                        double LL[8][32];
                        for (int c = 0; c < nch; c++) {
                            if (ci_of[c] < 0)
                                continue;
                            qrun[c] = 1;
                            int sb1 = ac4d_cmp_sb1(f, xs_of[c]);
                            for (long j = 0; j < spf; j++)
                                fp[j] = (double)cx->fr_s16[c][j] * pre;
                            if (prewarm == 2 || (prewarm == 1 && f == 0))
                                qmf_analysis(fp, slots, &(cx->cmp_qs)[c],
                                             &Q[c * slots]);
                            if (prewarm == 3 && spf >= 640) {
                                double tail[640];
                                for (long j = 0; j < 640; j++)
                                    tail[j] = (double)cx->fr_s16[c][spf - 640 + j] *
                                              pre;
                                qmf_analysis(tail, 10, &(cx->cmp_qs)[c],
                                             &Q[c * slots]);
                            }
                            if (prewarm == 4 && spf >= 640 && pre > 0.0) {
                                double he = 0.0, se = 0.0;
                                for (long j = 0; j < 640; j++) {
                                    double h = (double)cx->fr_s16[c][j];
                                    he += h * h;
                                    se += (cx->cmp_qs)[c].afilt[j] * (cx->cmp_qs)[c].afilt[j];
                                }
                                he = sqrt(he / 640.0);
                                se = sqrt(se / 640.0) / pre;
                                if (he > 50.0 && he > 10.0 * se) {
                                    double mir[640];
                                    for (long j = 0; j < 640; j++)
                                        mir[j] =
                                            (double)cx->fr_s16[c][639 - j] *
                                            pre;
                                    qmf_analysis(mir, 10, &(cx->cmp_qs)[c],
                                                 &Q[c * slots]);
                                }
                            }
                            /* QMFRESET (duet34 ring-localizer, temporary): zero
                             * QMF state before listed frames
                             * (AC4DEC_QMFRESET="8" or "8,24"). Tests
                             * vendor-carry-model (ring-excess hypothesis). */
                            {
                                const char *rp = getenv("AC4DEC_QMFRESET");
                                if (rp && rp[0]) {
                                    char nb[32];
                                    snprintf(nb, sizeof nb, ",%ld,", f);
                                    char eb[256];
                                    snprintf(eb, sizeof eb, ",%s,", rp);
                                    if (strstr(eb, nb))
                                        qmf_init(&(cx->cmp_qs)[c]);
                                }
                            }
                            qmf_analysis(fp, slots, &(cx->cmp_qs)[c],
                                         &Q[c * slots]);
                            /* QMFDUMP-PRE (duet34 divergence-localizer,
                             * temporary): post-analysis Q (pre-m). */
                            if (getenv("AC4DEC_QMFDUMP")) {
                                for (int dts = 0; dts < slots; dts++) {
                                    fprintf(stderr, "QMFP f=%ld c=%d ts=%d Q=",
                                            f, c, dts);
                                    for (int dsb = 0; dsb < 64; dsb++)
                                        fprintf(stderr, "%.17g,%.17g%c",
                                                creal(Q[c * slots + dts][dsb]),
                                                cimag(Q[c * slots + dts][dsb]),
                                                dsb + 1 < 64 ? ',' : '\n');
                                }
                            }
                            for (int ts = 0; ts < slots; ts++) {
                                double L = 0.0;
                                double E[64];
                                int lsb = sb1;
                                { /* AC4DEC_L64=1 (duet26 M4 L-band
                                   * discriminant): measure-L sums ALL
                                   * 64 subbands (apply stays sb<sb1).
                                   * Default unset = sb<sb1. */
                                    const char *lp =
                                        getenv("AC4DEC_L64");
                                    if (lp && lp[0] == '1')
                                        lsb = 64;
                                }
                                if (sb1 > 0) {
                                    for (int sb = 0; sb < lsb; sb++) {
                                        double complex v =
                                            Q[c * slots + ts][sb];
                                        double re = creal(v), im = cimag(v);
                                        double ar = re < 0 ? -re : re;
                                        double ai = im < 0 ? -im : im;
                                        double mx = ar > ai ? ar : ai;
                                        double mn = ar > ai ? ai : ar;
                                        E[sb] = mx + 0.5 * mn;
                                        L += E[sb];
                                    }
                                    L = 0.9105 * L / (double)lsb;
                                }
                                if (usofol) {
                                    double k = L > folsm[c] ? fol_a : fol_r;
                                    folsm[c] += k * (L - folsm[c]);
                                    L = folsm[c];
                                }
                                /* CMPSLEW=r (duet26 Track-S): cap L-RISE
                                 * per slot at r (L-units), falls free.
                                 * Chon-interval slots only (avg/gaps/
                                 * frameoff untouched: mavg + hotfill
                                 * domains pristine). State resets at
                                 * each interval start (vendor-like
                                 * cold head, A-46). Post-follower.
                                 * Lead-combo: pre-pass ggall stays
                                 * unslewed (sweep slew-alone). */
                                if (slewrate >= 0.0 && !slewskip &&
                                    chonv[c] && ts >= st0[c] &&
                                    ts < st1[c]) {
                                    if (ts == st0[c])
                                        slewsm[c] = 0.0;
                                    if (L > slewsm[c]) {
                                        double cap =
                                            slewsm[c] + slewrate;
                                        L = L < cap ? L : cap;
                                    }
                                    slewsm[c] = L;
                                } else if (slewrate >= 0.0 &&
                                           !slewskip) {
                                    slewsm[c] = L;
                                }
                                gg[c][ts] = L > 1e-12 ? pow(L, ex) : 0.0;
                                LL[c][ts] = L;
                                if (f == dumpf || dumpf == -2) {
                                    int b0 = -1, b1 = -1;
                                    double e0 = -1.0, e1 = -1.0, er = 0.0;
                                    if (sb1 > 0) {
                                        for (int sb = 0; sb < sb1; sb++) {
                                            if (E[sb] > e0) {
                                                e1 = e0;
                                                b1 = b0;
                                                e0 = E[sb];
                                                b0 = sb;
                                            } else if (E[sb] > e1) {
                                                e1 = E[sb];
                                                b1 = sb;
                                            }
                                        }
                                        for (int sb = 0; sb < sb1; sb++)
                                            if (sb != b0 && sb != b1)
                                                er += E[sb];
                                    }
                                    fprintf(stderr,
                                            "CMPD f=%ld c=%d ts=%d L=%.6g "
                                            "g=%.6g m=%.6g top=%d:%.3g "
                                            "%d:%.3g rest=%.3g\n",
                                            f, c, ts, L, gg[c][ts],
                                            gg[c][ts] * G, b0, e0, b1, e1,
                                            er);
                                }
                            }
                            /* CMPQPH=f: complex-Q phase dump (P1e/f
                             * discriminant, duet26): per-slot magnitude
                             * + phase of the tone-carrying bands,
                             * pre-gain (analysis output). Read-only. */
                            {
                                const char *qp =
                                    getenv("AC4DEC_CMPQPH");
                                long qph = qp ? strtol(qp, NULL, 10) : -1;
                                if ((qph == f || qph == -2) && sb1 > 0) {
                                    for (int ts = 0; ts < slots; ts++) {
                                        fprintf(stderr,
                                                "CMPQ f=%ld c=%d ts=%d",
                                                f, c, ts);
                                        for (int sb = 0; sb < sb1 &&
                                                             sb < 8;
                                             sb++) {
                                            double complex v =
                                                Q[c * slots + ts][sb];
                                            double mg = cabs(v);
                                            double ph =
                                                atan2(cimag(v), creal(v));
                                            fprintf(stderr, " %d:%.3g@%.2f",
                                                    sb, mg, ph);
                                        }
                                        fprintf(stderr, "\n");
                                    }
                                }
                            }
                        }
                        int onm = (f < e3_cmp_n) ? e3_cmp_onm[f] : 0;
                        int avgf = (f < e3_cmp_n) ? e3_cmp_avg[f] : 0;
                        for (int c = 0; c < nch; c++) {
                            if (ci_of[c] < 0)
                                continue;
                            int sb1 = ac4d_cmp_sb1(f, xs_of[c]);
                            int wire = w_of[c];
                            int chon = syn ? (onm & 1)
                                           : ((onm >> ci_of[c]) & 1);
                            /* ASPX-interval slots (A-13/A-17): BOTH the
                             * per-slot path and the avg-held gain apply
                             * on interval slots only ("for each time
                             * slot in the A-SPX interval", 5.7.5.2).
                             * Interval recorded in ATS units per wire;
                             * slots = ATS x ntats. Avg-held gain (A-13):
                             * m_avg = G*(mean interval L)^ex; sync
                             * variant m = G*mean(gsynch) (A-13). */
                            int t0 = 0, t1 = slots;
                            double mavg = 1.0;
                            int isavg = !chon && !unity && avgf && sb1 > 0;
                            if (fullint) {
                                t0 = 0;
                                t1 = slots;
                            }
                            if (sb1 > 0 && f < e3_cmp_n && wire >= 0 &&
                                wire < 5 && !fullint) {
                                int axt = e_ax_timeslots(spf);
                                int nta =
                                    axt > 0 ? slots / axt : 1;
                                if (nta < 1)
                                    nta = 1;
                                t0 = e3_cmp_i0[wire][f] * nta;
                                t1 = e3_cmp_i1[wire][f] * nta;
                                if (t0 < 0)
                                    t0 = 0;
                                if (t1 > slots)
                                    t1 = slots;
                                if (t1 <= t0) {
                                    t0 = 0;
                                    t1 = slots;
                                }
                            }
                            double oavg = 1.0;
                            int isoatk = 0;
                            int isflat = 0;
                            double flat_minm = 1.0, flat_tailm = 1.0;
                            double flat_maxm = 1.0;
                            /* FLAT (landed): always computed on chon
                             * frames; onset probes inner-gated. */
                            if (chon && !unity && !syn && sb1 > 0 &&
                                t1 > t0) {
                                int hn = 0, tn = 0, qn = 0;
                                double hs = 0.0, tsec = 0.0, qs = 0.0;
                                double tms = 0.0;
                                int tmn = 0;
                                flat_minm = 1e30;
                                flat_maxm = -1e30;
                                for (int ts = t0; ts < t1; ts++) {
                                    double l = LL[c][ts];
                                    double mt = gg[c][ts] * G;
                                    double mm = (inv && mt > 1e-12)
                                        ? 1.0 / mt
                                        : mt;
                                    if (mm < flat_minm)
                                        flat_minm = mm;
                                    if (mm > flat_maxm)
                                        flat_maxm = mm;
                                    if (ts < t0 + 10) {
                                        hs += l;
                                        hn++;
                                    }
                                    if (ts >= t1 - 10) {
                                        tsec += l;
                                        tn++;
                                        tms += mm;
                                        tmn++;
                                    }
                                    if (l >= 0.05) {
                                        qs += l;
                                        qn++;
                                    }
                                }
                                if (tmn > 0)
                                    flat_tailm = tms / (double)tmn;
                                if (flat_minm > 1e29)
                                    flat_minm = 1.0;
                                if (flat_maxm < -1e29)
                                    flat_maxm = 1.0;
                                if ((onsetavg || floorskip) && hn > 0 &&
                                    tn > 0 && qn > 0) {
                                    double hm = hs / (double)hn;
                                    double tm = tsec / (double)tn;
                                    if (hm < 0.5 * tm && tm > 0.1) {
                                        double qa = qs / (double)qn;
                                        oavg = qa > 1e-12
                                            ? G * pow(qa, ex)
                                            : 0.0;
                                        isoatk = oavg > 1e-12 ? 1 : 0;
                                    }
                                }
                                /* Flat gate: gap-chon + unity-tail +
                                 * dip-head + flat-peak (s2s-f1 class;
                                 * maxm<1.2 spares IMS (1.29) + whip-tails
                                 * (M1/M7-f1 1.66, f36 1.64)). */
                                if (t0 > 0 &&
                                    flat_tailm > 0.8 && flat_tailm < 1.2 &&
                                    flat_minm < 0.8 && flat_maxm < 1.2)
                                    isflat = 1;
                                if (f == dumpf || dumpf == -2)
                                    fprintf(stderr,
                                            "CMPO f=%ld c=%d t0=%d t1=%d "
                                            "oavg=%.6g gate=%d chon=%d "
                                            "minm=%.6g tailm=%.6g "
                                            "maxm=%.6g isflat=%d\n",
                                            f, c, t0, t1, oavg, isoatk,
                                            chon ? 1 : 0, flat_minm,
                                            flat_tailm, flat_maxm, isflat);
                            }
                            if (isavg) {
                                double s = 0.0;
                                for (int ts = t0; ts < t1; ts++) {
                                    if (syn) {
                                        /* gsynch(ts): mean g over
                                         * companded slots (A-13). */
                                        double gs = 0.0;
                                        int nn = 0;
                                        for (int d = 0; d < nch; d++) {
                                            if (ci_of[d] < 0)
                                                continue;
                                            gs += gg[d][ts];
                                            nn++;
                                        }
                                        s += nn > 0 ? gs / (double)nn
                                                    : 0.0;
                                    } else {
                                        s += LL[c][ts];
                                    }
                                }
                                if (syn) {
                                    double ma = s / (double)(t1 - t0);
                                    mavg = ma > 1e-12 ? G * ma : 0.0;
                                } else {
                                    double la = s / (double)(t1 - t0);
                                    mavg = la > 1e-12 ? G * pow(la, ex)
                                                      : 0.0;
                                }
                                if (f == dumpf || dumpf == -2)
                                    fprintf(stderr,
                                            "CMPA f=%ld c=%d t0=%d t1=%d "
                                            "mavg=%.6g syn=%d\n",
                                            f, c, t0, t1, mavg, syn);
                            }
                            if (f == dumpf || dumpf == -2)
                                fprintf(stderr,
                                        "CMPG f=%ld c=%d t0=%d t1=%d sb1=%d "
                                        "isavg=%d\n",
                                        f, c, t0, t1, sb1, isavg);
                            if (gainhold && !unity && !chon && !avgf)
                                held[c] = 1.0;
                            for (int ts = 0; ts < slots; ts++) {
                                /* CMPUNITYF=f[,ts0,ts1] (duet26 atomic
                                 * fork-(i), v2 range): slots [ts0,ts1)
                                 * of frame f post-pass OFF (Q
                                 * passthrough, bank-transparent NOCMP).
                                 * gainhold-held/gdel-fifo go stale for
                                 * gated slots (both default-off; moot). */
                                if (f == unityf && ts >= unityt0 &&
                                    ts < unityt1)
                                    continue;
                                /* (CMPLEAD removed: no future
                                 * frames in streaming.) */
                                double g = gg[c][ts];
                                if (syn) {
                                    double s = 0.0;
                                    int nn = 0;
                                    for (int d = 0; d < nch; d++) {
                                        if (ci_of[d] < 0)
                                            continue;
                                        s += gg[d][ts];
                                        nn++;
                                    }
                                    g = nn > 0 ? s / (double)nn : 0.0;
                                }
                                double m = unity ? 1.0
                                                 : (inv && g * G > 1e-12
                                                        ? 1.0 / (g * G)
                                                        : g * G);
                                /* AC4DEC_CMPSET=f,ts0,ts1,mv[;...]
                                 * (duet26 f8-null tap; duet45 vector-m:
                                 * ';'-separated specs, later override
                                 * earlier): force per-slot m. */
                                const char *csp = getenv("AC4DEC_CMPSET");
                                if (csp && csp[0]) {
                                    const char *seg = csp;
                                    while (seg && *seg) {
                                        long sf = -1, st0 = 0, st1 = 0;
                                        double sv = 0.0;
                                        if (sscanf(seg, "%ld,%ld,%ld,%lg",
                                                   &sf, &st0, &st1,
                                                   &sv) == 4 &&
                                            f == sf && ts >= st0 &&
                                            ts < st1 && sv > 0.0)
                                            m = sv;
                                        seg = strchr(seg, ';');
                                        if (seg)
                                            seg++;
                                    }
                                }
                                if (onsetavg && isoatk && chon && !unity)
                                    m = oavg;
                                int flskip = floorskip && isoatk && chon &&
                                             !unity && m < 1.0;
                                /* Full-interval flat: vendor-effective
                                 * UNITY (f1-off-equivalent, gated). */
                                if (isflat && chon && !unity && useflat)
                                    m = 1.0;
                                /* CMPDAMP=d (duet26 Track-S
                                 * discriminant): compress per-slot m
                                 * toward its interval mean: m' = mean
                                 * + d*(m-mean). Chon path only (avg
                                 * mavg is already step-free). Mean is
                                 * over base m (g*G, lead-honoring);
                                 * inv/flat apply before damp. */
                                if (dampf >= 0.0 && chon && !unity &&
                                    t1 > t0) {
                                    double dm = 0.0;
                                    int dn = 0;
                                    for (int dts = t0; dts < t1; dts++) {
                                        double dg;
                                        dg = gg[c][dts];
                                        if (syn) {
                                            double ds = 0.0;
                                            int dnn = 0;
                                            for (int dd = 0; dd < nch;
                                                 dd++) {
                                                if (ci_of[dd] < 0)
                                                    continue;
                                                ds += gg[dd][dts];
                                                dnn++;
                                            }
                                            dg = dnn > 0
                                                ? ds / (double)dnn
                                                : 0.0;
                                        }
                                        dm += dg * G;
                                        dn++;
                                    }
                                    if (dn > 0) {
                                        dm /= (double)dn;
                                        m = dm + dampf * (m - dm);
                                        if (m < 0.0)
                                            m = 0.0;
                                    }
                                }
                                if (gdel > 0) {
                                    int gate_now = 0;
                                    double g_now = 1.0;
                                    if (sb1 > 0) {
                                        if (unity) {
                                            gate_now = 1;
                                            g_now = m;
                                        } else if (chon && ts >= t0 &&
                                                   ts < t1 && !flskip) {
                                            gate_now = 1;
                                            g_now = m;
                                        } else if (isavg &&
                                                   (avgall || (ts >= t0 &&
                                                                ts < t1))) {
                                            gate_now = 1;
                                            g_now = mavg;
                                        } else if (gainhold &&
                                                   (chon || isavg)) {
                                            gate_now = 1;
                                            g_now = held[c];
                                        }
                                    }
                                    int pp = gpos[c] % (int)gdel;
                                    int gate_old = gatefifo[c][pp];
                                    double g_old = gfifo[c][pp];
                                    gatefifo[c][pp] = gate_now;
                                    gfifo[c][pp] = g_now;
                                    gpos[c]++;
                                    if (gate_old && sb1 > 0 &&
                                        f != frameoff && f != frameof2) {
                                        for (int sb = 0; sb < sb1; sb++)
                                            Q[c * slots + ts][sb] *= g_old;
                                        held[c] = g_old;
                                    }
                                    continue;
                                }
                                if (sb1 <= 0)
                                    continue;
                                if (f == frameoff || f == frameof2)
                                    continue;
                                if (getenv("AC4DEC_APPM"))
                                    fprintf(stderr,
                                            "APPM f=%ld c=%d ts=%d br=%s m=%.6g mavg=%.6g held=%.6g chon=%d isavg=%d sb1=%d t0=%d t1=%d\n",
                                            f, c, ts,
                                            unity ? "unity" : (chon && ts >= t0 && ts < t1 && !flskip) ? "chon" : (isavg && (avgall || (ts >= t0 && ts < t1))) ? "avg" : (hotfill && (chon || isavg) && (ts < t0 || ts >= t1)) ? "hot" : (gainhold && (chon || isavg)) ? "hold"
                                                                                                                                                                                                                                                                                          : "NONE",
                                            m, mavg, held[c], chon, isavg, sb1,
                                            t0, t1);
                                if (unity) {
                                    for (int sb = 0; sb < sb1; sb++)
                                        Q[c * slots + ts][sb] *= m;
                                    held[c] = m;
                                } else if (chon && ts >= t0 && ts < t1 &&
                                           !flskip) {
                                    for (int sb = 0; sb < sb1; sb++)
                                        Q[c * slots + ts][sb] *= m;
                                    held[c] = m;
                                } else if (isavg &&
                                           (avgall || (ts >= t0 && ts < t1))) {
                                    for (int sb = 0; sb < sb1; sb++)
                                        Q[c * slots + ts][sb] *= mavg;
                                    held[c] = mavg;
                                } else if (hotfill && (chon || isavg) &&
                                           (ts < t0 || ts >= t1)) {
                                    /* CMPHOTFILL (duet26 Track-G):
                                     * gap slots take computed-m (chon)
                                     * / mavg (avg) iff >= 1.0. Hot-fill
                                     * heals gap-steps (f13 1.49 / f36
                                     * 1.5); cold-fill would crush, so
                                     * cold gaps stay passthrough (f48
                                     * 0.11 keep -> cliff safe). A-46. */
                                    double fmv = chon ? m : mavg;
                                    if (fmv >= 1.0) {
                                        for (int sb = 0; sb < sb1; sb++)
                                            Q[c * slots + ts][sb] *= fmv;
                                        held[c] = fmv;
                                    }
                                } else if (gainhold && (chon || isavg)) {
                                    for (int sb = 0; sb < sb1; sb++)
                                        Q[c * slots + ts][sb] *= held[c];
                                }
                                /* AC4DEC_IMSEQ=g00..g15:g10..g15 (duet26
                                 * IMS-f0 A-49): 16-band x 2ch EQ (1.5kHz
                                 * bands <=> 4 QMF SBs) at f==0, ts<8
                                 * (t=0-511). Chained AFTER m (all 64
                                 * SBs). Default unset/malformed = off. */
                                if (f == 0 && ts < 8 && c < 2) {
                                    const char *ip =
                                        getenv("AC4DEC_IMSEQ");
                                    if (ip && ip[0]) {
                                        double eq[2][16];
                                        int ok = 1;
                                        const char *p = ip;
                                        for (int cc = 0; cc < 2 && ok;
                                             cc++) {
                                            for (int bb = 0; bb < 16;
                                                 bb++) {
                                                char *e = NULL;
                                                double v =
                                                    strtod(p, &e);
                                                if (e == p)
                                                    ok = 0;
                                                else {
                                                    eq[cc][bb] = v;
                                                    p = (*e == ',' ||
                                                         *e == ':')
                                                        ? e + 1
                                                        : e;
                                                }
                                            }
                                        }
                                        if (ok)
                                            for (int sb = 0; sb < 64;
                                                 sb++)
                                                Q[c * slots + ts][sb] *=
                                                    eq[c][sb / 4];
                                    }
                                }
                            }
                            /* QMFDUMP-POST (duet34 divergence-localizer,
                             * temporary): pre-synthesis Q (post-m). */
                            if (getenv("AC4DEC_QMFDUMP")) {
                                for (int dts = 0; dts < slots; dts++) {
                                    fprintf(stderr, "QMFQ f=%ld c=%d ts=%d Q=",
                                            f, c, dts);
                                    for (int dsb = 0; dsb < 64; dsb++)
                                        fprintf(stderr, "%.17g,%.17g%c",
                                                creal(Q[c * slots + dts][dsb]),
                                                cimag(Q[c * slots + dts][dsb]),
                                                dsb + 1 < 64 ? ',' : '\n');
                                }
                            }
                            qmf_synthesis(&Q[c * slots], slots, &(cx->cmp_qs)[c],
                                          fp);
                            for (long j = 0; j < spf; j++)
                                cx->cmp_raw[c][(f * spf + j) % AC4D_RAWN] =
                                    fp[j];
                            cx->cmp_w[c] = (f + 1) * (long)spf;
                        }
}

/* Compand 640-zero flush (batch: post-loop bank flush). */
static void ac4d_cmp_flush(void) {
    ac4d_ctx *cx = _ac4d_self;
    int nch = cx->h_nch;
    int spf = cx->h_spf;
    if (!cx->ce_on)
        return;
                    /* Flush: 640 zero samples (10 slots, m = 1)
                     * advance the 577-delayed tail out of the bank. */
                    for (long j = 0; j < spf; j++)
                        fp[j] = 0.0;
                    for (int c = 0; c < nch; c++) {
                        if (!qrun[c])
                            continue;
                        double complex (*Qf)[64] = &Q[c * slots];
                        for (int rep = 0; rep < 640 / spf + 1; rep++) {
                            int fsl = rep == 640 / spf ? 640 % spf : spf;
                            if (fsl <= 0)
                                break;
                            int nsl = fsl / 64;
                            if (nsl <= 0 || nsl > slots)
                                break;
                            qmf_analysis(fp, nsl, &(cx->cmp_qs)[c], Qf);
                            qmf_synthesis(Qf, nsl, &(cx->cmp_qs)[c], fp);
                            for (int j = 0; j < fsl; j++)
                                cx->cmp_raw[c][(cx->cmp_w[c] + rep * spf + j) %
                                                  AC4D_RAWN] = fp[j];
                            for (long j = 0; j < spf; j++)
                                fp[j] = 0.0;
                        }
                    }
    for (int c = 0; c < 8; c++)
        if (cx->cmp_qrun[c])
            cx->cmp_w[c] += 640;
}

/* Compand writeback for ONE emit frame e (batch: per-f iteration
 * of the writeback loop; out[t] = raw[t+577]). dst starts as the
 * postpassed/skeleton base; take-channels are overwritten. */
static int ac4d_cmp_emit(long e, short (*dst)[2048]) {
    ac4d_ctx *cx = _ac4d_self;
    int nch = cx->h_nch;
    int spf = cx->h_spf;
    double trim = e3_dngain * 32768.0;
    if (!cx->ce_on)
        return 0;
                        int onm = (e < e3_cmp_n) ? e3_cmp_onm[e] : 0;
                        int syn = (e < e3_cmp_n) ? e3_cmp_syn[e] : 0;
                        int avgf = (e < e3_cmp_n) ? e3_cmp_avg[e] : 0;
                        int codec = (e < e3_cmp_n) ? e3_cmp_codec[e] : 0;
                        int m2v = (e < e3_cmp_n) ? e3_cmp_m2[e] : -1;
                        for (int c = 0; c < nch; c++) {
                            int wire = 0, xslot = 0;
                            int ci = e3_cmp_slotmap(nch, codec, m2v, c,
                                                    &wire, &xslot);
                            if (ci < 0)
                                continue;
                            int sb1 = ac4d_cmp_sb1(e, xslot);
                            int chon = syn ? (onm & 1) : ((onm >> ci) & 1);
                            int take = chon || unity || (!chon && avgf);
                            if (!take || sb1 <= 0)
                                continue;
                            for (long j = 0; j < spf; j++) {
                                long t = e * spf + j + (unshift ? 0 : 577);
                                if (t > cx->cmp_w[c] ||
                                    cx->cmp_w[c] - t > AC4D_RAWN)
                                    return -1; /* raw ring overrun */
                                double v =
                                    ac4d_rget_d(cx->cmp_raw[c], AC4D_RAWN,
                                                t) *
                                    trim;
                                dst[c][j] = imdct_d2s16(v);
                            }
                        }
                        if (census)
                            fprintf(stderr,
                                    "CMPW f=%ld sb1=%d/%d/%d on=%u avg=%u "
                                    "codec=%d\n",
                                    e, ac4d_cmp_sb1(e, 0), ac4d_cmp_sb1(e, 1), ac4d_cmp_sb1(e, 2),
                                    onm, avgf,
                                    e < e3_cmp_n ? e3_cmp_codec[e] : -9);
    return 0;
}

#undef fp
#undef Q
#undef qrun
#undef held
#undef folsm
#undef slewsm
#undef gfifo
#undef gatefifo
#undef gpos
#undef G
#undef ex
#undef slots
#undef unity
#undef inv
#undef census
#undef dumpf
#undef fullint
#undef avgall
#undef gainhold
#undef gdel
#undef unshift
#undef prewarm
#undef onsetavg
#undef floorskip
#undef useflat
#undef frameoff
#undef frameof2
#undef fol_a
#undef fol_r
#undef usofol
#undef dampf
#undef hotfill
#undef slewrate
#undef unityf
#undef unityt0
#undef unityt1
#undef cmp_fp
#undef cmp_Q
#undef cmpG
#undef cmpex
/* ---- emit pipeline ---- */

/* Stage skeleton frame f into dst as PRE-GAIN floats (plain FIFO
 * partition, or mode-0 track mix which batch ran once over full
 * streams -- the mix is per-sample stateless so per-frame is
 * identical). Gain applies at consume so a DN re-latch on the first
 * P-frame stays bit-exact. */
static int ac4d_gather_f(long f, double (*dst)[2048]) {
    ac4d_ctx *cx = _ac4d_self;
    int nch = cx->h_nch, spf = cx->h_spf;
    long base = f * (long)spf;
    int c, j;
    /* Returns 1 when the frame's samples are not pushed yet (normal
     * throttle: retry after more feeds), -1 on ring overwrite. */
    if (cx->tf_used) {
        const double *s[12];
        double *fl[8];
        long oc[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (c = 0; c < 12; c++) {
            if (base + spf > cx->h_oc[c])
                return 1;
            if (cx->h_oc[c] - base > AC4D_SKN)
                return -1;
            for (j = 0; j < spf; j++)
                cx->mx_tmp[c][j] =
                    ac4d_rget_d(cx->tf_fifo[c], AC4D_SKN, base + j);
            s[c] = cx->mx_tmp[c];
        }
        for (c = 0; c < 8; c++)
            fl[c] = dst[c];
        e3_mix12p_f(s, 0, spf, fl, oc);
        return 0;
    }
    for (c = 0; c < nch; c++) {
        if (base + spf > cx->h_oc[c])
            return 1;
        if (cx->h_oc[c] - base > AC4D_SKN)
            return -1;
        for (j = 0; j < spf; j++)
            dst[c][j] =
                ac4d_rget_d(cx->sk_fifo[c], AC4D_SKN, base + j);
    }
    return 0;
}

/* s16 twin of ac4d_gather_f (gain + quantize at consume). */
static int ac4d_gather(long f, short (*dst)[2048]) {
    ac4d_ctx *cx = _ac4d_self;
    int nch = cx->h_nch, spf = cx->h_spf;
    int c, j, grc;
    grc = ac4d_gather_f(f, cx->fr_flt);
    if (grc != 0)
        return grc;
    for (c = 0; c < nch; c++)
        for (j = 0; j < spf; j++)
            dst[c][j] =
                imdct_d2s16(cx->fr_flt[c][j] * e3_dngain);
    return 0;
}

/* Pump newly-ready stages (pp + compand analyze) after a feed or the
 * drain. Purely data-driven (priming pushes make count thresholds
 * wrong): each stage advances while its inputs are present. Emits
 * become ready 2 analyzed frames later (writeback looks 577 samples
 * past the emit frame). */
static int ac4d_pp_ready(long f) {
    /* 7.1.4 pp readiness: skeleton present, and the spill window when
     * the post-pass applies to this frame. */
    ac4d_ctx *cx = _ac4d_self;
    int spf = cx->h_spf;
    int slot = (int)(f % AC4D_APFN);
    int q;
    if (!cx->ap3[slot].have || cx->ap3[slot].fno != f || !e3_imm_elig)
        return 1; /* skeleton stands; nothing to wait for */
    for (q = 0; q < 4; q++)
        if (e3_spill_oc[q] < (f + 1) * (long)spf + 577)
            return cx->h_flushing ? 1 : 0; /* flush: zero-pad tail */
    return 1;
}

static int ac4d_pump(void) {
    ac4d_ctx *cx = _ac4d_self;
    long F = cx->h_fed;
    int c, j, grc;
    if (!cx->h_fmt || cx->h_stub || cx->h_gap_sticky)
        return 0;
    int frozen = cx->h_dn_tent && !cx->h_flushing;
    int press = 0;
    if (frozen) {
        /* Ring-pressure unfreeze: the oldest live data nears
         * overwrite, so emits/analyze must flow (marked dirty); a
         * later DN change then fails loud in feed (no silent
         * divergence). Fires at ~4 frames, ahead of the pp (5f) and
         * compand-raw horizons. */
        int nch = cx->tf_used ? 12 : cx->h_nch;
        long lim = (long)AC4D_SKN - 2L * (long)cx->h_spf;
        for (int c = 0; c < nch; c++)
            if (cx->h_oc[c] - cx->h_emit * (long)cx->h_spf > lim)
                press = 1;
    }
    /* DN tentative (leading I, no P yet): analyze stalls only when
     * compand is engaged (its QMF state + raw ring bake the gain);
     * disengaged analyze is a no-op and pp/skeleton stay pre-gain,
     * so eager analyze is exact. Ready freezes while tentative so
     * no frame emits under a superseded gain (covers post-splice
     * epochs where oc is already large). Flush with no P keeps the
     * tentative (= batch first-kept fallback). */
    if (frozen && !press && cx->ce_on)
        return 0;
    if (cx->h_nch == 6) {
        while (cx->h_an < F) {
            long f = cx->h_an;
            int gap = 0;
            int slot = (int)(f % AC4D_APFN);
            grc = ac4d_gather_f(f, cx->fr_flt);
            if (grc != 0)
                return grc < 0 ? -1 : 0;
            if (cx->ap5[slot].have && cx->ap5[slot].fno == f &&
                e5_acpl_elig && !cx->h_flushing) {
                int rd = 0;
                for (int q = 0; q < 2; q++)
                    if (e5_spill_oc[q] < (f + 1) * (long)cx->h_spf + 577)
                        rd = 1;
                if (rd)
                    break; /* spill window not pushed yet */
            }
            if (ac4d_pp_5x_emit(f, cx->fr_flt, &gap) != 0)
                return -1; /* spill ring overrun */
            if (gap) {
                /* Batch would fail the whole decode here and
                 * fall back to full stub: latch sticky silence. */
                cx->h_gap = "5x-acpl-unimpl";
                cx->h_gap_sticky = 1;
                cx->h_gap_start = f;
                cx->h_ready = cx->h_fed;
                return 0;
            }
            for (c = 0; c < 6; c++)
                for (j = 0; j < cx->h_spf; j++)
                    cx->pp_ring[c][(f * cx->h_spf + j) % AC4D_PPN] =
                        cx->fr_flt[c][j];
            if (cx->ce_on)
                for (c = 0; c < 6; c++)
                    for (j = 0; j < cx->h_spf; j++)
                        cx->fr_s16[c][j] = imdct_d2s16(
                            cx->fr_flt[c][j] * e3_dngain);
            ac4d_cmp_analyze(f);
            cx->h_an++;
            if (cx->h_dn_tent && cx->ce_on)
                cx->h_dn_dirty = 1;
            if (!cx->h_flushing && (!cx->h_dn_tent || press) &&
                cx->h_an > cx->h_need)
                cx->h_ready = cx->h_an - cx->h_need;
        }
        if (cx->h_ready > F)
            cx->h_ready = F;
        return 0;
    }
    if (cx->h_nch == 8) {
        while (cx->h_pp < F) {
            long f = cx->h_pp;
            if (!ac4d_pp_ready(f))
                break;
            grc = ac4d_gather_f(f, cx->fr_flt);
            if (grc != 0)
                return grc < 0 ? -1 : 0;
            if (ac4d_pp_imm_emit(f, cx->fr_flt) != 0)
                return -1; /* spill ring overrun */
            for (c = 0; c < 8; c++)
                for (j = 0; j < cx->h_spf; j++)
                    cx->pp_ring[c][(f * cx->h_spf + j) % AC4D_PPN] =
                        cx->fr_flt[c][j];
            cx->h_pp++;
            if (!cx->h_flushing && (!cx->h_dn_tent || press))
                cx->h_ready = cx->h_pp;
        }
        if (cx->h_ready > F)
            cx->h_ready = F;
        return 0;
    }
    while (cx->h_an < F) {
        long f = cx->h_an;
        grc = ac4d_gather_f(f, cx->fr_flt);
        if (grc != 0)
            return grc < 0 ? -1 : 0;
        if (cx->ce_on)
            for (c = 0; c < cx->h_nch; c++)
                for (j = 0; j < cx->h_spf; j++)
                    cx->fr_s16[c][j] = imdct_d2s16(
                        cx->fr_flt[c][j] * e3_dngain);
        ac4d_cmp_analyze(f);
        cx->h_an++;
        if (cx->h_dn_tent && cx->ce_on)
            cx->h_dn_dirty = 1;
        if (!cx->h_flushing && (!cx->h_dn_tent || press) &&
            cx->h_an > cx->h_need)
            cx->h_ready = cx->h_an - cx->h_need;
    }
    if (cx->h_ready > F)
        cx->h_ready = F;
    return 0;
}

/* Emit ordinal h_emit into em_s16; *is_gap set for silence frames. */
static int ac4d_emit_one(int *is_gap) {
    ac4d_ctx *cx = _ac4d_self;
    long e = cx->h_emit;
    int nch = cx->h_nch, spf = cx->h_spf;
    long base = e * (long)spf;
    int c, j;
    *is_gap = 0;
    if (cx->h_stub || (cx->h_gap_sticky && e >= cx->h_gap_start)) {
        for (c = 0; c < nch; c++)
            for (j = 0; j < spf; j++)
                cx->em_s16[c][j] = 0;
        *is_gap = 1;
        cx->h_emit++;
        return 0;
    }
    if (cx->h_dn_tent)
        cx->h_dn_dirty = 1; /* real emit under tentative gain */
    if (nch == 8 || nch == 6) {
        long done = nch == 8 ? cx->h_pp : cx->h_an;
        int nc = nch == 8 ? 8 : 6;
        if (e >= done || done - e > AC4D_PPN / 2048)
            return -1; /* pp ring overrun (impossible by h_ready) */
        for (c = 0; c < nc; c++)
            for (j = 0; j < spf; j++)
                cx->em_s16[c][j] = imdct_d2s16(
                    ac4d_rget_d(cx->pp_ring[c], AC4D_PPN, base + j) *
                    e3_dngain);
        if (nch == 6 && ac4d_cmp_emit(e, cx->em_s16) != 0)
            return -1;
    } else {
        if (ac4d_gather(e, cx->em_s16))
            return -1;
        if (ac4d_cmp_emit(e, cx->em_s16) != 0)
            return -1;
    }
    /* FADE0 (batch: post-compand scale of t < fn; assumes fn < total,
     * which holds for every real use of this discriminant probe). */
    if (cx->ce_fade0 > 0) {
        long fn = cx->ce_fade0;
        for (j = 0; j < spf; j++) {
            long t = base + j;
            double r;
            if (t >= fn)
                break;
            r = 0.5 *
                (1.0 - cos(3.141592653589793 * (double)t / (double)fn));
            for (c = 0; c < nch; c++)
                cx->em_s16[c][j] =
                    (short)lrint((double)cx->em_s16[c][j] * r);
        }
    }
    cx->h_emit++;
    return 0;
}

/* Convert + copy the next ready frame to the caller buffer. The frame
 * must be ready (caller checked delay/counts). Polls (pcm NULL) do
 * not consume. */
static int ac4d_emit_consume(void *pcm, size_t cap, ac4d_info *fi) {
    ac4d_ctx *cx = _ac4d_self;
    ac4d_info ii;
    size_t need, bps;
    int is_gap = 0;
    uint8_t *out;
    int c, j;
    if (ac4d_get_info(cx, &ii) != AC4D_OK)
        return AC4D_ERR;
    bps = ii.bits == 24 ? 3 : ii.bits == 32 ? 4 : 2;
    need = (size_t)ii.channels * (size_t)ii.spf * bps;
    if (fi)
        *fi = ii;
    if (pcm == NULL)
        return (cx->h_stub ||
                (cx->h_gap_sticky && cx->h_emit >= cx->h_gap_start))
            ? AC4D_GAP
            : AC4D_OK;
    if (cap < need)
        return AC4D_ERR;
    if (ac4d_emit_one(&is_gap) != 0) {
        cx->h_hard_err = AC4D_ERR;
        cx->h_hard_note = "ring-overrun";
        return AC4D_ERR;
    }
    out = pcm;
    for (j = 0; j < ii.spf; j++) {
        for (c = 0; c < ii.channels; c++) {
            short v = c < cx->h_nch ? cx->em_s16[c][j] : 0;
            uint8_t le[4];
            int nw = wav_s16_to_bytes(v, e_wav_bps, le);
            memcpy(out, le, (size_t)nw);
            out += nw;
        }
    }
    return is_gap ? AC4D_GAP : AC4D_OK;
}

int ac4d_get_frame(ac4d_ctx *c, void *pcm, size_t cap, ac4d_info *fi) {
    if (!c || !c->ao_open)
        return AC4D_ERR;
    _ac4d_self = c;
    if (c->h_hard_err)
        return c->h_hard_err;
    if (!c->h_fmt)
        return AC4D_NEED_MORE;
    if (c->h_emit >= c->h_ready)
        return AC4D_NEED_MORE;
    return ac4d_emit_consume(pcm, cap, fi);
}

int ac4d_flush(ac4d_ctx *c, void *pcm, size_t cap) {
    const char *note = "ok";
    if (!c || !c->ao_open)
        return AC4D_ERR;
    _ac4d_self = c;
    if (c->h_hard_err)
        return c->h_hard_err;
    if (!c->h_fmt || c->h_flushed)
        return AC4D_NEED_MORE;
    if (!c->h_flushing) {
        c->h_flushing = 1;
        c->h_dn_tent = 0; /* no-P fallback stands (= batch first-kept) */
        if (!c->h_stub && !c->h_gap_sticky) {
            if (ac4d_drain(&note) != 0 ||
                ac4d_length_check(&note) != 0) {
                c->h_hard_err = AC4D_BITSTREAM;
                c->h_hard_note = note;
                return AC4D_BITSTREAM;
            }
        }
        if (ac4d_pump() != 0) {
            c->h_hard_err = AC4D_ERR;
            c->h_hard_note = "ring-overrun";
            return AC4D_ERR;
        }
        ac4d_cmp_flush();
        c->h_ready = c->h_fed;
    }
    if (c->h_emit >= c->h_fed) {
        c->h_flushed = 1;
        return AC4D_NEED_MORE;
    }
    return ac4d_emit_consume(pcm, cap, NULL);
}


/* E8 --print-info TAG dump (A-78 Q3: order FMT VER DN DRC DRCSEL CH5X
 * CMP CRC LOUD; TAG k=v on stdout; HYBRID mirror -- DRP values quoted
 * verbatim where ours computes the same quantity, writer-defined keys
 * documented in tests/CLI.md). Values with spaces/commas are
 * double-quoted (grep-able units). */

/* Shim full-packet sidecar (combined alloc with raws; freed together). */
typedef struct {
    const uint8_t *p;
    size_t len;
} shim_pkt_t;
