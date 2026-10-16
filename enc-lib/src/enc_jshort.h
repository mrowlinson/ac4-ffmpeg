#ifndef AC4_ENC_JSHORT_H
#define AC4_ENC_JSHORT_H

/* Joint SHORT/LONG 2ch block writers: C port of encoder/jshort.py.
 * Shared window-group geometry + chparam + 2-leg assembly for
 * joint blocks. Spectral legs reuse src/asf.h (asf_write_sf_data,
 * asf_write_mixed_leg); scalefactor DPCM uses the committed
 * ASF_HCB_SCALEFAC codewords. Offs ladders are opaque
 * const int* (NULL = 2048 ladder, the asf convention).
 *
 * Refs convention: refs_by_band arrays carry -1 for "no ref"
 * (absent dict key) and 0..255 for a ref value; a NULL
 * refs_by_band is an empty dict. Python assert-class failures
 * (bad shape, out-of-range field, KeyError/IndexError-class
 * gaps) return -1; the writer is untouched on -1.
 */

#include "bits.h"

#define ENC_JSHORT_MAX_WINDOWS 9
#define ENC_JSHORT_MAX_GROUPS 16

/* One window-group geometry row: (offs, max_sfb, sect_bits, nwins).
 * offs is carried opaque (NULL = 2048 ladder downstream). */
typedef struct {
    const int *offs;
    int max_sfb;
    int sect_bits;
    int nwins;
} enc_jshort_geo_t;

/* (tl0, tl1) -> grouping-field bit count, or -1 when the pair is
 * not a supported joint shape (NGRP_BITS). */
int enc_jshort_ngrp_bits(int tl0, int tl1);

/* (tl0, tl1) -> (m0 width, m1 width) (MSFB_WIDTHS).
 * 0 ok, -1 on unsupported pair. */
int enc_jshort_msfb_widths(int tl0, int tl1, int *w0, int *w1);

/* (2,3) 1-bit grouping -> 3-entry window->group map.
 * 0 ok, -1 unless grp1 is 0/1. */
int enc_jshort_narrow_w2g(int grp1, int w2g_out[3]);

/* (2,3) groups -> geo rows (2 or 3 of them; out must hold 3).
 * 0 ok, -1 unless grp1 is 0/1. */
int enc_jshort_narrow_group_geo(int grp1, int m512, int m1024,
                               const int *offs_512,
                               const int *offs_1024,
                               enc_jshort_geo_t *out, int *ngeos_out);

/* 7-bit grouping -> 9-entry window->group map. long_first != 0
 * puts the long window first. 0 ok, -1 unless 0 <= grp7 < 128. */
int enc_jshort_grouping_to_w2g(int grp7, int long_first,
                               int w2g_out[9]);

/* w2g map -> per-group members. Group g owns wins (window
 * indices, ascending) and kinds ('L'/'S') over
 * starts[g]..starts[g+1]-1 (starts has *ngroups_out + 1 <= 10
 * entries; kinds/wins hold nw). 1 <= nw <= 9, entries < 9.
 * 0 ok, -1 on bad shape. */
int enc_jshort_group_windows(const int *w2g, int nw, int long_first,
                             int *ngroups_out, int *starts, char *kinds,
                             int *wins);

/* w2g map -> per-group geo rows (out must hold nw). Groups must
 * be window-type homogeneous and long groups singleton.
 * 0 ok, -1 on bad shape. */
int enc_jshort_group_band_geo(const int *w2g, int nw, int msfb_long,
                              int msfb_short, const int *offs_long,
                              const int *offs_short, int long_first,
                              enc_jshort_geo_t *out, int *ngeos_out);

/* Chparam side inputs. gmax NULL = all-zero (the joint default);
 * msbits/used NULL = absent (Python None); deltas NULL/0 = none.
 * has_allc/has_dct mark present scalar fields. */
typedef struct {
    const int *gmax; /* [ngroups] */
    const int *msbits;
    int nmsbits;
    int has_allc;
    int allc;
    const int *used;
    int nused;
    int has_dct;
    int dct;
    const int *deltas;
    int ndeltas;
} enc_jshort_chparam_t;

/* Joint chparam after shared geometry. cp NULL = all defaults.
 * 0 ok, -1 on any assert-class failure (bad sap, missing or
 * miscounted fields, out-of-range bit or delta). Negative gmax
 * floors to 0 iterations like Python's range(). */
int enc_jshort_write_chparam(bitwriter_t *w, int sap, int ngroups,
                             const enc_jshort_chparam_t *cp);

/* One joint-SHORT leg's per-group coeffs. lines/wins/qs hold n
 * entries (wins NULL = every coeff is window 0); refs_by_band
 * has max_sfb entries (-1 = absent) or is NULL when empty. */
typedef struct {
    const int *lines;
    const int *wins;
    const int *qs;
    int n;
    const int *refs_by_band;
} enc_jshort_group_data_t;

/* One caller-supplied prefix field: value in nbits bits. */
typedef struct {
    unsigned value;
    int nbits;
} enc_jshort_prefix_t;

/* One joint-SHORT 2ch block: shared geom + chparam + 2 legs.
 * leg0/leg1/geo each hold ngroups entries (1..16). cp NULL =
 * defaults (sap 2 needs none). leg_refs holds 2 fallback refs
 * and may be NULL unless a leg has empty refs. Per-leg ref =
 * first-nonempty-group min-band ref, else leg_refs[leg].
 * 0 ok, -1 on any assert-class failure. */
int enc_jshort_write_short_2ch(bitwriter_t *w,
                               const enc_jshort_prefix_t *prefix,
                               int nprefix, int tl0, int tl1, int m0,
                               int m1, int grp7, int sap,
                               const enc_jshort_group_data_t *leg0,
                               const enc_jshort_group_data_t *leg1,
                               const enc_jshort_geo_t *geo, int ngroups,
                               const enc_jshort_chparam_t *cp,
                               const int *leg_refs);

/* One joint-LONG leg: n singleton-line coeffs; refs_by_band has
 * msfb entries (-1 = absent) or is NULL for refs None. */
typedef struct {
    const int *lines;
    const int *qs;
    int n;
    const int *refs_by_band;
} enc_jshort_long_leg_t;

/* One joint-LONG 2ch block (caller writes the [st_proc, b_long]
 * prefix): msfb 6b + sap 2b + chparam + legM + legS LONG legs
 * (sect_bits 5). A leg with empty refs uses leg_refs[leg] and,
 * like the Python (which passes {} there), drops its coeffs.
 * 0 ok, -1 on any assert-class failure. */
int enc_jshort_write_long_2ch(bitwriter_t *w, int msfb, int sap,
                              const enc_jshort_long_leg_t *legM,
                              const enc_jshort_long_leg_t *legS,
                              const int *offs,
                              const enc_jshort_chparam_t *cp,
                              const int *leg_refs);

#endif
