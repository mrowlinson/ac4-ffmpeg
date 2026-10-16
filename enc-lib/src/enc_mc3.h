#ifndef AC4_ENC_MC3_H
#define AC4_ENC_MC3_H

/* 5.1 codec_mode-3 (ASPX_ACPL_2) frame assembler: C port of
 * encoder/mc3.py. Element ORDER + header bits only; core/tail
 * VALUES are caller-supplied. Pure int/bit assembly (no float
 * ops; submodules keep their own float discipline).
 *
 * Mirrors the Python entry points 1:1 (dicts/tuples become the
 * structs below; Python None becomes NULL or a has_* flag):
 *   write_iframe_cfg -> enc_mc3_write_iframe_cfg (+ defaults)
 *   write_compand_off -> enc_mc3_write_compand_off
 *   write_audio_51_mode3 -> enc_mc3_write_audio_51_mode3
 *   quiet_aspx -> enc_mc3_quiet_aspx
 *   measured_aspx -> enc_mc3_measured_aspx
 *   write_audio_51_mode3_silence -> enc_mc3_write_audio_51_mode3_silence
 * The nbooks dict param has no C counterpart (books resolve
 * from the committed tables via enc_aspx_book_by_name).
 *
 * Python assert/KeyError/IndexError/ValueError-class failures
 * return -1. mc3-owned ranges are pre-checked (writer
 * untouched); submodule -1s propagate with prefix bits kept,
 * exactly like a mid-write Python exception.
 */

#include "bits.h"
#include "enc_acpl.h"
#include "enc_jshort.h"
#include "enc_tail59.h"

/* write_iframe_cfg kwargs (widths: 1/3/2/1/1/1/1/2/1/2/2/1). */
typedef struct {
    int qmode;
    int start;
    int stop;
    int mfscale;
    int interp;
    int preflat;
    int limiter;
    int noisesbg;
    int envbits;
    int freqres;
    int npb_id;
    int acpl_qm;
} enc_mc3_iframe_cfg_t;

/* DEE defaults (fr0/fr1/fr24 identical). */
void enc_mc3_default_iframe_cfg(enc_mc3_iframe_cfg_t *cfg);

/* I-frame aspx_config 15b + acpl_config_1ch 3b. 0 ok, -1 on
 * NULL or any out-of-range field (BitWriter.put fit rule). */
int enc_mc3_write_iframe_cfg(bitwriter_t *w,
                             const enc_mc3_iframe_cfg_t *cfg);

/* Compand off-shape 5b. */
void enc_mc3_write_compand_off(bitwriter_t *w);

/* One LFE/mono sf leg: (coeffs, ref, refs|None, msfb).
 * refs_by_band covers [0, msfb) with -1 = absent; NULL iff
 * has_refs == 0 (refs None). */
typedef struct {
    const int *lines;
    const int *qs;
    int n;
    int ref;
    const int *refs_by_band;
    int has_refs;
    int msfb;
} enc_mc3_leg_t;

/* Joint LONG: ("long", msfb, sap, chparam, legM, legS, offs,
 * leg_refs?). offs NULL = 2048 ladder; cp NULL = chparam
 * None; leg_refs used iff has_leg_refs (empty legs). */
typedef struct {
    int msfb;
    int sap;
    enc_jshort_long_leg_t legM;
    enc_jshort_long_leg_t legS;
    const int *offs;
    const enc_jshort_chparam_t *cp;
    int leg_refs[2];
    int has_leg_refs;
} enc_mc3_joint_long_t;

/* Joint SHORT: ("short", tl0, tl1, m0, m1, grp7, sap, legs,
 * refs, geo, chparam, leg_refs). leg0/leg1/geo hold ngroups
 * entries; cp NULL = None; leg_refs NULL unless a leg has
 * empty refs. */
typedef struct {
    int tl0;
    int tl1;
    int m0;
    int m1;
    int grp7;
    int sap;
    const enc_jshort_group_data_t *leg0;
    const enc_jshort_group_data_t *leg1;
    const enc_jshort_geo_t *geo;
    int ngroups;
    const enc_jshort_chparam_t *cp;
    const int *leg_refs;
} enc_mc3_joint_short_t;

#define ENC_MC3_JOINT_LONG 0
#define ENC_MC3_JOINT_SHORT 1

typedef struct {
    int kind;
    union {
        enc_mc3_joint_long_t lon;
        enc_mc3_joint_short_t sh;
    } u;
} enc_mc3_joint_t;

/* Tail: (s2ch, s1ch, a0, a1, abooks). */
typedef struct {
    const enc_tail_s2ch_t *s2ch;
    const enc_tail_s1ch_t *s1ch;
    const enc_tail_acpl_t *a0;
    const enc_tail_acpl_t *a1;
    const enc_acpl_books_t *abooks;
} enc_mc3_tail_t;

/* One mode-3 5.1 audio_data. iframe != 0 writes the I-frame
 * cfg + xover 0. 0 ok, -1 on NULL, bad joint kind, or any
 * submodule assert-class failure. */
int enc_mc3_write_audio_51_mode3(bitwriter_t *w, int iframe,
                                 const enc_mc3_leg_t *lfe,
                                 const enc_mc3_joint_t *joint,
                                 const enc_mc3_leg_t *mono,
                                 const enc_mc3_tail_t *tail);

/* ASPX wire/scf run bundles (steady 12/12/3/3 + 12/3). */
typedef struct {
    int sig0[12];
    int sig1[12];
    int noise0[3];
    int noise1[3];
} enc_mc3_aspx2_runs_t;

typedef struct {
    int sig[12];
    int noise[3];
} enc_mc3_aspx1_runs_t;

/* quiet_aspx(freq) outputs. s2ch/s1ch wire pointers alias
 * r2/r1 (self-contained: valid while out lives). */
typedef struct {
    enc_tail_s2ch_t s2ch;
    enc_tail_s1ch_t s1ch;
    enc_mc3_aspx2_runs_t r2;
    enc_mc3_aspx1_runs_t r1;
} enc_mc3_quiet_t;

/* Quiet ASPX pair (zero indices; TIME-flat dirs unless freq).
 * 0 ok, -1 on NULL out. */
int enc_mc3_quiet_aspx(int freq, enc_mc3_quiet_t *out);

/* measured_aspx inputs: decoded-q runs; has_prev gates per
 * run (0 = None -> FREQ forced); harm 12b masks (0 = none). */
typedef struct {
    int qsig0[12];
    int qsig1[12];
    int qnoise0[3];
    int qnoise1[3];
    int has_prev[4];
    int psig0[12];
    int psig1[12];
    int pnoise0[3];
    int pnoise1[3];
    int harm0;
    int harm1;
} enc_mc3_aspx2_in_t;

typedef struct {
    int qsig[12];
    int qnoise[3];
    int has_prev[2];
    int psig[12];
    int pnoise[3];
    int harm;
} enc_mc3_aspx1_in_t;

/* measured_aspx outputs: (s2ch, s1ch) + (scf2, scf1). Wire
 * pointers alias wire2/wire1 (valid while out lives). */
typedef struct {
    enc_tail_s2ch_t s2ch;
    enc_tail_s1ch_t s1ch;
    enc_mc3_aspx2_runs_t wire2;
    enc_mc3_aspx2_runs_t scf2;
    enc_mc3_aspx1_runs_t wire1;
    enc_mc3_aspx1_runs_t scf1;
} enc_mc3_measured_t;

/* Measured-envelope ASPX pair. 0 ok, -1 on NULL, bad
 * has_prev flag, or out-of-range harm mask. */
int enc_mc3_measured_aspx(const enc_mc3_aspx2_in_t *in2,
                          const enc_mc3_aspx1_in_t *in1,
                          enc_mc3_measured_t *out);

/* One mode-3 5.1 silence audio_data (container proof). 0 ok,
 * -1 on NULL. */
int enc_mc3_write_audio_51_mode3_silence(
    bitwriter_t *w, int iframe, const enc_acpl_books_t *abooks);

#endif
