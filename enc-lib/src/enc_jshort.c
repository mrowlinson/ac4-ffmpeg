#include "enc_jshort.h"

#include <stddef.h>

#include "asf.h"
#include "huffman_tables.h"
#include "sfb_offsets.h"

#define JSHORT_MAX_SFB 64 /* == ASF_MAX_SFB inside asf.c */


int enc_jshort_ngrp_bits(int tl0, int tl1) {
    if (tl0 == 3 && tl1 == 0)
        return 7;
    if (tl0 == 0 && tl1 == 3)
        return 7;
    if (tl0 == 2 && tl1 == 3)
        return 1;
    return -1;
}


int enc_jshort_msfb_widths(int tl0, int tl1, int *w0, int *w1) {
    if (w0 == NULL || w1 == NULL)
        return -1;
    if (tl0 == 3 && tl1 == 0) {
        *w0 = 6;
        *w1 = 4;
        return 0;
    }
    if (tl0 == 0 && tl1 == 3) {
        *w0 = 4;
        *w1 = 6;
        return 0;
    }
    if (tl0 == 2 && tl1 == 3) {
        *w0 = 6;
        *w1 = 6;
        return 0;
    }
    return -1;
}


int enc_jshort_narrow_w2g(int grp1, int w2g_out[3]) {
    if (w2g_out == NULL)
        return -1;
    if (grp1 == 0) {
        w2g_out[0] = 0;
        w2g_out[1] = 1;
        w2g_out[2] = 2;
        return 0;
    }
    if (grp1 == 1) {
        w2g_out[0] = 0;
        w2g_out[1] = 0;
        w2g_out[2] = 1;
        return 0;
    }
    return -1;
}


int enc_jshort_narrow_group_geo(int grp1, int m512, int m1024,
                               const int *offs_512,
                               const int *offs_1024,
                               enc_jshort_geo_t *out, int *ngeos_out) {
    if (out == NULL || ngeos_out == NULL)
        return -1;
    /* Same rows the generic group/merge loop yields; the two
     * cases only (kinds homogeneous by construction). */
    if (grp1 == 0) {
        out[0].offs = offs_512;
        out[0].max_sfb = m512;
        out[0].sect_bits = 3;
        out[0].nwins = 1;
        out[1].offs = offs_512;
        out[1].max_sfb = m512;
        out[1].sect_bits = 3;
        out[1].nwins = 1;
        out[2].offs = offs_1024;
        out[2].max_sfb = m1024;
        out[2].sect_bits = 5;
        out[2].nwins = 1;
        *ngeos_out = 3;
        return 0;
    }
    if (grp1 == 1) {
        out[0].offs = offs_512;
        out[0].max_sfb = m512;
        out[0].sect_bits = 3;
        out[0].nwins = 2;
        out[1].offs = offs_1024;
        out[1].max_sfb = m1024;
        out[1].sect_bits = 5;
        out[1].nwins = 1;
        *ngeos_out = 2;
        return 0;
    }
    return -1;
}


int enc_jshort_grouping_to_w2g(int grp7, int long_first,
                               int w2g_out[9]) {
    int sg[8];
    int g = 0, i;

    if (w2g_out == NULL)
        return -1;
    if (grp7 < 0 || grp7 >= 128)
        return -1;
    for (i = 0; i < 8; i++) {
        sg[i] = g;
        if (i < 7 && !(grp7 & (1 << (6 - i))))
            g++;
    }
    if (long_first) {
        w2g_out[0] = 0;
        for (i = 0; i < 8; i++)
            w2g_out[i + 1] = sg[i] + 1;
    } else {
        for (i = 0; i < 8; i++)
            w2g_out[i] = sg[i];
        w2g_out[8] = sg[7] + 1; /* sg[] is nondecreasing */
    }
    return 0;
}


int enc_jshort_group_windows(const int *w2g, int nw, int long_first,
                             int *ngroups_out, int *starts, char *kinds,
                             int *wins) {
    int cursor[ENC_JSHORT_MAX_WINDOWS];
    int ng = 0, g, i, pos;

    if (w2g == NULL || ngroups_out == NULL || starts == NULL ||
        kinds == NULL || wins == NULL)
        return -1;
    if (nw < 1 || nw > ENC_JSHORT_MAX_WINDOWS)
        return -1;
    for (i = 0; i < nw; i++) {
        if (w2g[i] < 0 || w2g[i] >= ENC_JSHORT_MAX_WINDOWS)
            return -1;
        if (w2g[i] + 1 > ng)
            ng = w2g[i] + 1;
    }
    for (g = 0; g <= ng; g++)
        starts[g] = 0;
    for (i = 0; i < nw; i++)
        starts[w2g[i] + 1]++;
    for (g = 0; g < ng; g++)
        starts[g + 1] += starts[g];
    for (g = 0; g < ng; g++)
        cursor[g] = starts[g];
    for (i = 0; i < nw; i++) {
        char k;
        if (long_first)
            k = (i == 0) ? 'L' : 'S';
        else
            k = (i == nw - 1) ? 'L' : 'S';
        pos = cursor[w2g[i]]++;
        kinds[pos] = k;
        wins[pos] = i;
    }
    *ngroups_out = ng;
    return 0;
}


int enc_jshort_group_band_geo(const int *w2g, int nw, int msfb_long,
                              int msfb_short, const int *offs_long,
                              const int *offs_short, int long_first,
                              enc_jshort_geo_t *out, int *ngeos_out) {
    int starts[ENC_JSHORT_MAX_WINDOWS + 1];
    char kinds[ENC_JSHORT_MAX_WINDOWS];
    int wins[ENC_JSHORT_MAX_WINDOWS];
    int ng, g;

    if (out == NULL || ngeos_out == NULL)
        return -1;
    (void)wins;
    if (enc_jshort_group_windows(w2g, nw, long_first, &ng, starts,
                                 kinds, wins))
        return -1;
    for (g = 0; g < ng; g++) {
        int lo = starts[g], hi = starts[g + 1], i;
        int nl = 0, ns = 0;
        for (i = lo; i < hi; i++) {
            if (kinds[i] == 'L')
                nl++;
            else
                ns++;
        }
        if (nl > 0 && ns > 0)
            return -1;
        if (nl + ns == 0)
            return -1;
        if (nl > 0) {
            if (nl != 1)
                return -1;
            out[g].offs = offs_long;
            out[g].max_sfb = msfb_long;
            out[g].sect_bits = 5;
            out[g].nwins = 1;
        } else {
            out[g].offs = offs_short;
            out[g].max_sfb = msfb_short;
            out[g].sect_bits = 3;
            out[g].nwins = ns;
        }
    }
    *ngeos_out = ng;
    return 0;
}


static int gmax_at(const enc_jshort_chparam_t *cp, int g) {
    if (cp == NULL || cp->gmax == NULL)
        return 0;
    return cp->gmax[g];
}


static int chparam_check(int sap, int ngroups,
                         const enc_jshort_chparam_t *cp) {
    long need = 0;
    int g, i;

    if (sap < 0 || sap > 3)
        return -1;
    if (ngroups < 0)
        return -1;
    if (sap == 0 || sap == 2)
        return 0;
    if (sap == 1) {
        if (cp == NULL || cp->msbits == NULL)
            return -1;
        if (cp->nmsbits < 0)
            return -1;
        for (g = 0; g < ngroups; g++) {
            int t = gmax_at(cp, g);
            if (t > 0)
                need += t;
        }
        if (need != cp->nmsbits)
            return -1;
        for (i = 0; i < cp->nmsbits; i++) {
            if (cp->msbits[i] != 0 && cp->msbits[i] != 1)
                return -1;
        }
        return 0;
    }
    if (cp == NULL || !cp->has_allc)
        return -1;
    if (cp->allc != 0 && cp->allc != 1)
        return -1;
    if (cp->allc == 0) {
        if (cp->used == NULL)
            return -1;
        if (cp->nused < 0)
            return -1;
        for (g = 0; g < ngroups; g++) {
            int t = gmax_at(cp, g);
            if (t >= 0)
                need += (t + 1) / 2;
        }
        if (need != cp->nused)
            return -1;
        for (i = 0; i < cp->nused; i++) {
            if (cp->used[i] != 0 && cp->used[i] != 1)
                return -1;
        }
    }
    if (ngroups != 1) {
        if (!cp->has_dct)
            return -1;
        if (cp->dct != 0 && cp->dct != 1)
            return -1;
    }
    if (cp->ndeltas < 0)
        return -1;
    if (cp->ndeltas > 0 && cp->deltas == NULL)
        return -1;
    for (i = 0; i < cp->ndeltas; i++) {
        if (cp->deltas[i] < -60 || cp->deltas[i] > 60)
            return -1;
    }
    return 0;
}


static void chparam_emit(bitwriter_t *w, int sap, int ngroups,
                         const enc_jshort_chparam_t *cp) {
    int g, i, pos;

    if (sap == 0 || sap == 2)
        return;
    if (sap == 1) {
        pos = 0;
        for (g = 0; g < ngroups; g++) {
            int t = gmax_at(cp, g);
            int it = t < 0 ? 0 : t;
            for (i = 0; i < it; i++)
                bw_put(w, (unsigned)cp->msbits[pos++], 1);
        }
        return;
    }
    bw_put(w, (unsigned)cp->allc, 1);
    if (cp->allc == 0) {
        pos = 0;
        for (g = 0; g < ngroups; g++) {
            int t = gmax_at(cp, g);
            int it = t < 0 ? 0 : (t + 1) / 2;
            for (i = 0; i < it; i++)
                bw_put(w, (unsigned)cp->used[pos++], 1);
        }
    }
    if (ngroups != 1)
        bw_put(w, (unsigned)cp->dct, 1);
    for (i = 0; i < cp->ndeltas; i++) {
        int idx = cp->deltas[i] + 60;
        bw_put(w, ASF_HCB_SCALEFAC_CW[idx],
               ASF_HCB_SCALEFAC_LEN[idx]);
    }
}


int enc_jshort_write_chparam(bitwriter_t *w, int sap, int ngroups,
                             const enc_jshort_chparam_t *cp) {
    if (chparam_check(sap, ngroups, cp))
        return -1;
    chparam_emit(w, sap, ngroups, cp);
    return 0;
}


static const int *resolve_offs(const int *offs) {
    return offs != NULL ? offs : sfb_offsets48(2048);
}


/* BitWriter.put fit rule: 0 <= value < 2**nbits. */
static int fits(unsigned value, int nbits) {
    if (nbits < 0)
        return 0;
    if (nbits == 0)
        return value == 0;
    if (nbits >= 32)
        return 1;
    return value < (1u << nbits);
}


/*
 * Validate one joint-SHORT leg: geo rows, coeff ranges, ref
 * coverage of every qualifying band, first-qualifying == leg
 * ref, DPCM steps <= 60. Sets *ref_out to the leg ref
 * (first-nonempty-group min-band ref, else leg_refs[leg]).
 */
static int short_leg_check(const enc_jshort_group_data_t *leg,
                           const enc_jshort_geo_t *geo, int ngroups,
                           int leg_idx, const int *leg_refs,
                           int *ref_out) {
    int seen[ENC_JSHORT_MAX_GROUPS][JSHORT_MAX_SFB];
    int g, b, i, ref = -1;
    int started = 0, pv = 0;

    for (g = 0; g < ngroups; g++) {
        const int *offs;
        int max_sfb = geo[g].max_sfb;
        for (b = 0; b < JSHORT_MAX_SFB; b++)
            seen[g][b] = 0;
        if (max_sfb < 0 || max_sfb > JSHORT_MAX_SFB)
            return -1;
        if (geo[g].sect_bits < 1 || geo[g].sect_bits > 30)
            return -1;
        if (geo[g].nwins < 0)
            return -1;
        if (leg[g].n < 0)
            return -1;
        if (leg[g].n > 0) {
            if (geo[g].nwins < 1)
                return -1;
            if (leg[g].lines == NULL || leg[g].qs == NULL)
                return -1;
            if (leg[g].refs_by_band == NULL)
                return -1;
        }
        offs = resolve_offs(geo[g].offs);
        if (offs == NULL)
            return -1;
        for (i = 0; i < leg[g].n; i++) {
            int ln = leg[g].lines[i], q = leg[g].qs[i], wn;
            if (ln < 0 || ln >= offs[max_sfb])
                return -1;
            if (q == 0 || q < -ASF_MAX_Q || q > ASF_MAX_Q)
                return -1;
            wn = leg[g].wins != NULL ? leg[g].wins[i] : 0;
            if (wn < 0 || wn >= geo[g].nwins)
                return -1;
            b = asf_band_of(ln, offs, max_sfb);
            if (b < 0)
                return -1;
            seen[g][b] = 1;
        }
    }
    /* Ref-selection chain runs over ref entries (all bands),
     * like sorted(refs[g]) in the Python. */
    for (g = 0; g < ngroups && ref < 0; g++) {
        const int *rb = leg[g].refs_by_band;
        if (rb == NULL)
            continue;
        for (b = 0; b < geo[g].max_sfb; b++) {
            if (rb[b] != -1) {
                ref = rb[b];
                break;
            }
        }
    }
    if (ref < 0) {
        if (leg_refs == NULL)
            return -1;
        ref = leg_refs[leg_idx];
    }
    if (ref < 0 || ref > 255)
        return -1;
    for (g = 0; g < ngroups; g++) {
        const int *rb = leg[g].refs_by_band;
        for (b = 0; b < geo[g].max_sfb; b++) {
            int v;
            if (!seen[g][b])
                continue;
            if (rb == NULL || rb[b] < 0 || rb[b] > 255)
                return -1;
            v = rb[b];
            if (!started && v != ref)
                return -1;
            if (started && (v - pv < -60 || v - pv > 60))
                return -1;
            started = 1;
            pv = v;
        }
    }
    /* Silent leg (no qualifying band): ref + SNF off. */
    *ref_out = ref;
    return 0;
}


static void short_leg_groups(const enc_jshort_group_data_t *leg,
                             const enc_jshort_geo_t *geo, int ngroups,
                             asf_group_t *out) {
    int g;
    for (g = 0; g < ngroups; g++) {
        out[g].lines = leg[g].lines;
        out[g].wins = leg[g].wins;
        out[g].qs = leg[g].qs;
        out[g].n = leg[g].n;
        out[g].offs = geo[g].offs;
        out[g].max_sfb = geo[g].max_sfb;
        out[g].sect_bits = geo[g].sect_bits;
        out[g].nwins = geo[g].nwins;
        out[g].refs_by_band = leg[g].refs_by_band;
    }
}


int enc_jshort_write_short_2ch(bitwriter_t *w,
                               const enc_jshort_prefix_t *prefix,
                               int nprefix, int tl0, int tl1, int m0,
                               int m1, int grp7, int sap,
                               const enc_jshort_group_data_t *leg0,
                               const enc_jshort_group_data_t *leg1,
                               const enc_jshort_geo_t *geo, int ngroups,
                               const enc_jshort_chparam_t *cp,
                               const int *leg_refs) {
    asf_group_t groups[ENC_JSHORT_MAX_GROUPS];
    int ngrp = enc_jshort_ngrp_bits(tl0, tl1);
    int w0 = 0, w1 = 0, ref0, ref1, i;

    if (ngrp < 0)
        return -1;
    if (grp7 < 0 || grp7 >= (1 << ngrp))
        return -1;
    if (enc_jshort_msfb_widths(tl0, tl1, &w0, &w1))
        return -1;
    if (ngroups < 1 || ngroups > ENC_JSHORT_MAX_GROUPS)
        return -1;
    if (leg0 == NULL || leg1 == NULL || geo == NULL)
        return -1;
    if (nprefix < 0)
        return -1;
    if (nprefix > 0 && prefix == NULL)
        return -1;
    for (i = 0; i < nprefix; i++) {
        if (!fits(prefix[i].value, prefix[i].nbits))
            return -1;
    }
    if (m0 < 0 || !fits((unsigned)m0, w0))
        return -1;
    if (m1 < 0 || !fits((unsigned)m1, w1))
        return -1;
    if (chparam_check(sap, ngroups, cp))
        return -1;
    if (short_leg_check(leg0, geo, ngroups, 0, leg_refs, &ref0))
        return -1;
    if (short_leg_check(leg1, geo, ngroups, 1, leg_refs, &ref1))
        return -1;
    for (i = 0; i < nprefix; i++)
        bw_put(w, prefix[i].value, prefix[i].nbits);
    bw_put(w, (unsigned)tl0, 2);
    bw_put(w, (unsigned)tl1, 2);
    bw_put(w, (unsigned)m0, w0);
    bw_put(w, (unsigned)m1, w1);
    bw_put(w, (unsigned)grp7, ngrp);
    chparam_emit(w, sap, ngroups, cp);
    short_leg_groups(leg0, geo, ngroups, groups);
    asf_write_mixed_leg(w, groups, ngroups, ref0);
    short_leg_groups(leg1, geo, ngroups, groups);
    asf_write_mixed_leg(w, groups, ngroups, ref1);
    return 0;
}


/*
 * Validate one joint-LONG leg. Sets *ref_out and *has_out (refs
 * nonempty). Empty refs take leg_refs[leg] and, like the
 * Python (which passes {} there), drop the leg's coeffs.
 */
static int long_leg_check(const enc_jshort_long_leg_t *leg, int msfb,
                          const int *offs, int leg_idx,
                          const int *leg_refs, int *ref_out,
                          int *has_out) {
    int seen[JSHORT_MAX_SFB];
    const int *rb;
    int b, i, ref = -1, nqb = 0, pv = 0, first = 1;

    if (leg == NULL)
        return -1;
    offs = resolve_offs(offs);
    if (offs == NULL)
        return -1;
    rb = leg->refs_by_band;
    if (rb != NULL) {
        for (b = 0; b < msfb; b++) {
            if (rb[b] != -1) {
                ref = rb[b];
                break;
            }
        }
    }
    if (ref < 0) {
        if (leg_refs == NULL)
            return -1;
        ref = leg_refs[leg_idx];
        if (ref < 0 || ref > 255)
            return -1;
        *ref_out = ref;
        *has_out = 0;
        return 0;
    }
    if (ref < 0 || ref > 255)
        return -1;
    if (leg->n < 0)
        return -1;
    if (leg->n > 0 && (leg->lines == NULL || leg->qs == NULL))
        return -1;
    for (b = 0; b < JSHORT_MAX_SFB; b++)
        seen[b] = 0;
    for (i = 0; i < leg->n; i++) {
        int ln = leg->lines[i], q = leg->qs[i];
        if (ln < 0 || ln >= offs[msfb])
            return -1;
        if (q == 0 || q < -ASF_MAX_Q || q > ASF_MAX_Q)
            return -1;
        b = asf_band_of(ln, offs, msfb);
        if (b < 0)
            return -1;
        seen[b] = 1;
    }
    for (b = 0; b < msfb; b++) {
        int v;
        if (!seen[b])
            continue;
        nqb++;
        v = rb[b];
        if (v < 0 || v > 255)
            return -1;
        if (first && v != ref)
            return -1;
        if (!first && (v - pv < -60 || v - pv > 60))
            return -1;
        first = 0;
        pv = v;
    }
    if (nqb == 0)
        return -1;
    *ref_out = ref;
    *has_out = 1;
    return 0;
}


int enc_jshort_write_long_2ch(bitwriter_t *w, int msfb, int sap,
                              const enc_jshort_long_leg_t *legM,
                              const enc_jshort_long_leg_t *legS,
                              const int *offs,
                              const enc_jshort_chparam_t *cp,
                              const int *leg_refs) {
    int refM, refS, hasM, hasS;

    if (msfb < 0 || msfb >= 64)
        return -1;
    if (legM == NULL || legS == NULL)
        return -1;
    if (chparam_check(sap, 1, cp))
        return -1;
    if (long_leg_check(legM, msfb, offs, 0, leg_refs, &refM,
                       &hasM))
        return -1;
    if (long_leg_check(legS, msfb, offs, 1, leg_refs, &refS,
                       &hasS))
        return -1;
    bw_put(w, (unsigned)msfb, 6);
    bw_put(w, (unsigned)sap, 2);
    chparam_emit(w, sap, 1, cp);
    asf_write_sf_data(w, hasM ? legM->lines : NULL,
                      hasM ? legM->qs : NULL, hasM ? legM->n : 0,
                      refM, msfb,
                      hasM ? legM->refs_by_band : NULL, hasM, offs,
                      5);
    asf_write_sf_data(w, hasS ? legS->lines : NULL,
                      hasS ? legS->qs : NULL, hasS ? legS->n : 0,
                      refS, msfb,
                      hasS ? legS->refs_by_band : NULL, hasS, offs,
                      5);
    return 0;
}
