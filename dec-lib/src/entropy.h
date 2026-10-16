/* entropy.h — ASF spectral-frontend decode: sf_info + sections +
 * spectral + scalefactors + noise-fill + ungrouping -> per-window coeffs.
 *
 * Lane e3-pcm, wave6. Behavior pins (own words): ETSI P1 Tables
 * 34/37/38/39/40/41/42 + Pseudocode 2/3/4/5/21/22/25 + Annex B;
 * ANSWER-e1-1 (widths/ladders), ANSWER-c3-1 N1/N4 (walk-in),
 * survey-asf (books/dim/sign). Zero ETSI prose copied.
 *
 * Scope: mono/stereo SIMPLE ASF, 48k family, long + short blocks.
 * Stereo M/S + SAP application rides a followup (parse positions
 * exact here; application math = survey-coupling/clause-5.3 lane).
 * SNF: Table 42 + clause 5.1.4.2 implemented (snf_tables.h RNG
 * table); AC4E_SNF retained for API compat, no longer returned.
 */
#ifndef AC4_ENTROPY_H
#define AC4_ENTROPY_H

#include <stddef.h>
#include <stdint.h>

#define AC4E_OK 0
#define AC4E_OVERRUN (-1)  /* bit overrun / truncated element */
#define AC4E_SYNTAX (-2)   /* illegal value (book>11, tiling, range) */
#define AC4E_HUFF (-3)     /* no codeword match */
#define AC4E_SNF (-4)      /* noise fill active: table not staged */
#define AC4E_SSAP (-5)     /* stereo SAP/ms application not wired */
#define AC4E_SSF (-6)      /* SSF frontend: out of scope */

#define AC4E_MAXWIN 16
#define AC4E_MAXGRP 16
#define AC4E_MAXBANDS 64 /* 48k max_sfb ceiling (63+END) */
#define AC4E_MAXLINES 8192

/* MSB-first bit reader over a byte buffer. err sticks on overrun. */
typedef struct {
    const uint8_t *buf;
    size_t nbits;
    size_t pos;
    int err;
} ac4_ebr_t;

void ebr_init(ac4_ebr_t *b, const uint8_t *buf, size_t len);
unsigned ebr_u(ac4_ebr_t *b, int n); /* n<=32; err on overrun */
void ebr_align(ac4_ebr_t *b);        /* skip to next byte boundary */

/* Decoded sf_info: transform + grouping geometry (Pseudo 2/3/4/5). */
typedef struct {
    int b_long;
    int tl_idx[2];      /* wire transform-length indices */
    int max_sfb[2];     /* wire band counts */
    int grouping[16];   /* raw grouping bits */
    int ngrpbits;
    int num_windows;
    int num_groups;
    int win_to_group[AC4E_MAXWIN];
    int num_win_in_group[AC4E_MAXGRP];
    int win_len[AC4E_MAXWIN];  /* lines per window */
    int grp_tlidx[AC4E_MAXGRP]; /* transform index per group */
} asf_info_t;

/* Table 37+38 walk. frame_len_base in lines (= samples/tile).
 * Returns AC4E_*; info valid on OK. */
int asf_info_parse(ac4_ebr_t *b, int frame_len_base, asf_info_t *info);

/* One channel's decoded spectra, ungrouped: coeffs[w] has
 * info.win_len[w] scaled lines ready for the IMDCT. nch mono=1.
 * qw[w] = matching quantized ints (M3 textbook-ref N1; filled by the
 * same ungroup walk, zeros where co is zero). */
typedef struct {
    int nwin;
    int win_len[AC4E_MAXWIN];
    double co[AC4E_MAXWIN][AC4E_MAXLINES];
    int qw[AC4E_MAXWIN][AC4E_MAXLINES];
} asf_chan_t;

/* Tables 39->40->41->42 + dequant/scale/ungroup for one channel.
 * seq = TOC sequence_counter (SNF RNG reseed, clause 5.1.4.2).
 * info from asf_info_parse on the same channel. */
int asf_chan_decode(ac4_ebr_t *b, const asf_info_t *info, int frame_len_base,
                    unsigned seq, asf_chan_t *out);

/* SAP (MDCT stereo processing): chparam_info parse + 2x2 apply.
 * Behavior pins (own words): ETSI P1 Tables 23/47/48 + Pseudocode
 * 59 + clause 5.3.3.2. mat[g][s][4] = row-major a,b,c,d per
 * (window-group, scalefactor-band). Parse needs the COMMON sf_info
 * (shared geometry); apply needs both channels decoded under it. */
typedef struct {
    int sap_mode;
    double mat[AC4E_MAXGRP][AC4E_MAXBANDS][4];
} ac4e_sap_t;

int ac4e_chparam_parse(ac4_ebr_t *b, const asf_info_t *info,
                       ac4e_sap_t *sap);
void ac4e_sap_apply(const asf_info_t *info, const ac4e_sap_t *sap,
                    asf_chan_t *ch0, asf_chan_t *ch1);

#endif
