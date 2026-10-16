/* enc_core_long51.c: steady-LONG 5.1 content packing. See
 * enc_core.h for the contract. */
#include "enc_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "asf.h"
#include "pycompat.h"
#include "sfb_offsets.h"

int
enc_core_band_peaks(const double *X, int n, const int *offs,
                    int msfb, double *peaks)
{
    const int *o = offs ? offs : sfb_offsets48(2048);
    if (!X || n < 0 || !o || msfb < 0 || !peaks)
        return -1;
    for (int b = 0; b < msfb; b++) {
        int lo = o[b], hi = o[b + 1];
        double pb = 0.0;
        for (int L = lo; L < hi; L++) {
            double a = L < n ? fabs(X[L]) : 0.0;
            if (a > pb)
                pb = a;
        }
        peaks[b] = pb;
    }
    return 0;
}

/* A-68 Table-197 crosswalk rows (param band -> 2048-sfb list). */
static const int PB_SFB[15][15] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 0, 0, 0, 0 },
    { 8, 9, 10, 11, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 13, 14, 15, 16, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 17, 18, 19, 20, 21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 21, 22, 23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 24, 25, 26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 26, 27, 28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 28, 29, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 29, 30, 31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 31, 32, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 33, 34, 35, 36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 36, 37, 38, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 39, 40, 41, 42, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 42, 43, 44, 45, 46, 47, 48, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
      61, 62 },
};

static const int PB_LEN[15] =
    { 8, 6, 5, 5, 3, 3, 3, 2, 3, 3, 4, 4, 4, 7, 15 };

static int
pan_idx(double ef, double es, double floor_e)
{
    double s = ef + es;
    double a;
    int idx;
    if (s < floor_e || s <= 0.0)
        return 16;
    a = (ef - es) / s;
    idx = pycompat_round_half_even(16.0 + 8.0 * a);
    if (idx < 0)
        idx = 0;
    if (idx > 32)
        idx = 32;
    return idx;
}

/* sum(p[b]^2 for the row) in row order. */
static double
row_energy(const double *p, int pb)
{
    double terms[15];
    for (int k = 0; k < PB_LEN[pb]; k++) {
        double v = p[PB_SFB[pb][k]];
        terms[k] = pow(v, 2.0);
    }
    return pycompat_sum(terms, (size_t)PB_LEN[pb]);
}

int
enc_core_pack_acpl_dual(const double *pL, const double *pLs,
                        const double *pR, const double *pRs,
                        int npeaks, double floor_e, int interp,
                        enc_core_acpl_dual_t *out)
{
    int a0[15], a1[11];
    if (!pL || !pLs || !pR || !pRs || npeaks < 63 || !out)
        return -1;
    for (int pb = 0; pb < 15; pb++)
        a0[pb] = pan_idx(row_energy(pL, pb),
                         row_energy(pLs, pb), floor_e);
    for (int pb = 0; pb < 11; pb++)
        a1[pb] = pan_idx(row_energy(pR, pb),
                         row_energy(pRs, pb), floor_e);
    out->a0.interp = interp;
    out->a0.alpha_f0 = a0[0];
    for (int k = 0; k < 14; k++)
        out->a0_adfs[k] = a0[k + 1] - a0[k];
    out->a0.alpha_dfs = out->a0_adfs;
    out->a0.nalpha = 14;
    out->a0.beta_f0 = 0;
    for (int k = 0; k < 14; k++)
        out->a0_bdfs[k] = 0;
    out->a0.beta_dfs = out->a0_bdfs;
    out->a0.nbeta = 14;
    out->a0.has_slot = 0;
    out->a0.slot = 0;
    out->a0.beta_first = 0;
    out->a1.interp = interp;
    out->a1.alpha_f0 = a1[0];
    for (int k = 0; k < 10; k++)
        out->a1_adfs[k] = a1[k + 1] - a1[k];
    out->a1.alpha_dfs = out->a1_adfs;
    out->a1.nalpha = 10;
    out->a1.beta_f0 = 0;
    for (int k = 0; k < 10; k++)
        out->a1_bdfs[k] = 0;
    out->a1.beta_dfs = out->a1_bdfs;
    out->a1.nbeta = 10;
    out->a1.has_slot = 0;
    out->a1.slot = 0;
    out->a1.beta_first = 0;
    return 0;
}

int
enc_core_pack_mono(const double *XC, int nC, int has_gain,
                   double gain, int msfb_cap, int ref_shift,
                   int kcap, enc_core_leg_t *out)
{
    const int *offs = sfb_offsets48(2048);
    int coded = 0, top = 0;
    if (!XC || nC < 0 || !offs || msfb_cap < 1 ||
        msfb_cap > 63 || !out)
        return -1;
    if (!has_gain)
        gain = ENC_CORE_DIG_GAIN;
    if (enc_core_quantize_bands(XC, nC, gain, ref_shift,
                                kcap, offs, 63, msfb_cap,
                                0, out))
        return -1;
    for (int k = 0; k < out->n; k++) {
        int b = asf_band_of(out->lines[k], offs, 63);
        if (b < 0)
            return -1;
        coded = 1;
        if (b + 1 > top)
            top = b + 1;
    }
    if (!coded)
        top = 1;
    out->msfb = top < msfb_cap ? top : msfb_cap;
    return 0;
}

int
enc_core_pack_joint_long(const double *XA, int nA,
                         const double *XB, int nB,
                         int has_gain, double gain,
                         int msfb_cap, double floor_,
                         int ref_shift, int kcap,
                         enc_core_joint_pack_t *out)
{
    const int *offs = sfb_offsets48(2048);
    double pA[63], pB[63];
    double *X0 = NULL, *X1 = NULL;
    enc_core_leg_t leg0, leg1;
    int visbands[63];
    int abands[63], bbands[63];
    int alpha_keys[32], alpha_val[32];
    int nalpha = 0, msfb, npairs, allc, n, prev;
    if (!XA || nA < 0 || !XB || nB < 0 || !offs ||
        msfb_cap < 1 || msfb_cap > 63 || !out)
        return -1;
    if (!has_gain)
        gain = ENC_CORE_DIG_GAIN;
    if (enc_core_band_peaks(XA, nA, offs, msfb_cap, pA) ||
        enc_core_band_peaks(XB, nB, offs, msfb_cap, pB))
        return -1;
    out->nside = msfb_cap;
    for (int b = 0; b < msfb_cap; b++) {
        double a = pA[b], q = pB[b];
        if (a >= floor_ && a >= q && a > 0)
            out->side[b] = 1;
        else if (q >= floor_ && q > a && q > 0)
            out->side[b] = -1;
        else
            out->side[b] = 0;
    }
    n = nA > nB ? nA : nB;
    X0 = calloc((size_t)(n > 0 ? n : 1), sizeof *X0);
    X1 = calloc((size_t)(n > 0 ? n : 1), sizeof *X1);
    if (!X0 || !X1) {
        free(X0);
        free(X1);
        return -1;
    }
    for (int b = 0; b < msfb_cap; b++) {
        int lo = offs[b], hi = offs[b + 1];
        if (out->side[b] == 1) {
            int top = hi < nA ? hi : nA;
            for (int L = lo; L < top; L++)
                X0[L] = XA[L] * 0.5;
        } else if (out->side[b] == -1) {
            int top = hi < nB ? hi : nB;
            for (int L = lo; L < top; L++)
                X1[L] = XB[L];
        }
    }
    if (enc_core_quantize_bands(X0, n, gain, ref_shift,
                                kcap, offs, 63, msfb_cap,
                                0, &leg0) ||
        enc_core_quantize_bands(X1, n, gain, ref_shift,
                                kcap, offs, 63, msfb_cap,
                                0, &leg1)) {
        free(X0);
        free(X1);
        return -1;
    }
    free(X0);
    free(X1);
    {
        int coded = 0, top = 0;
        for (int k = 0; k < leg0.n; k++) {
            int b = asf_band_of(leg0.lines[k], offs, 63);
            if (b < 0)
                return -1;
            coded = 1;
            if (b + 1 > top)
                top = b + 1;
        }
        for (int k = 0; k < leg1.n; k++) {
            int b = asf_band_of(leg1.lines[k], offs, 63);
            if (b < 0)
                return -1;
            coded = 1;
            if (b + 1 > top)
                top = b + 1;
        }
        if (!coded)
            top = 1;
        msfb = top < msfb_cap ? top : msfb_cap;
    }
    out->msfb = msfb;
    npairs = (msfb + 1) / 2;
    out->npairs = npairs;
    for (int b = 0; b < 63; b++) {
        visbands[b] = 0;
        abands[b] = 0;
        bbands[b] = 0;
    }
    for (int k = 0; k < leg0.n; k++)
        abands[asf_band_of(leg0.lines[k], offs, 63)] = 1;
    for (int k = 0; k < leg1.n; k++)
        bbands[asf_band_of(leg1.lines[k], offs, 63)] = 1;
    for (int b = 0; b < 63; b++)
        visbands[b] = abands[b] || bbands[b];
    for (int p = 0; p < npairs; p++) {
        out->used[p] = 0;
        if (abands[2 * p] || (2 * p + 1 < 63 &&
                              abands[2 * p + 1])) {
            double te[2];
            int ne = 0;
            double ea, eb;
            for (int j = 0; j < 2; j++) {
                int b = 2 * p + j;
                if (b < msfb_cap)
                    te[ne++] = pow(pA[b], 2.0);
            }
            ea = pycompat_sum(te, (size_t)ne);
            ne = 0;
            for (int j = 0; j < 2; j++) {
                int b = 2 * p + j;
                if (b < msfb_cap)
                    te[ne++] = pow(pB[b], 2.0);
            }
            eb = pycompat_sum(te, (size_t)ne);
            if (ea >= 0.1 * eb)
                out->used[p] = 1;
        }
    }
    for (int p = 0; p < npairs; p++) {
        if (out->used[p]) {
            alpha_keys[nalpha] = p;
            alpha_val[nalpha] = 10;
            nalpha++;
        }
    }
    {
        double mA = 0.0, mB = 0.0, frame_e;
        int runs[32][2], nruns = 0, s = -1;
        for (int b = 0; b < msfb_cap; b++) {
            if (pA[b] > mA)
                mA = pA[b];
            if (pB[b] > mB)
                mB = pB[b];
        }
        frame_e = pow(mA, 2.0) > pow(mB, 2.0)
            ? pow(mA, 2.0)
            : pow(mB, 2.0);
        for (int i = 0; i <= npairs; i++) {
            int u = i < npairs ? out->used[i] : 0;
            if (u && s < 0)
                s = i;
            if (!u && s >= 0) {
                runs[nruns][0] = s;
                runs[nruns][1] = i - 1;
                nruns++;
                s = -1;
            }
        }
        out->nbets = 0;
        for (int r = nruns - 1; r >= 0; r--) {
            int e = runs[r][1], follows = 0;
            for (int j = e + 1; j < npairs && !follows;
                 j++) {
                if (out->used[j] &&
                    (visbands[2 * j] ||
                     (2 * j + 1 < 63 &&
                      visbands[2 * j + 1])))
                    follows = 1;
            }
            if (!follows)
                continue;
            {
                int q = e + 1;
                if (q < npairs &&
                    !visbands[2 * q] &&
                    (2 * q + 1 >= 63 ||
                     !visbands[2 * q + 1])) {
                    out->used[q] = 1;
                    alpha_keys[nalpha] = q;
                    alpha_val[nalpha] = 0;
                    nalpha++;
                    continue;
                }
            }
            {
                double te[2];
                int ne = 0;
                double ea;
                for (int j = 0; j < 2; j++) {
                    int b = 2 * e + j;
                    if (b < msfb_cap)
                        te[ne++] = pow(pA[b], 2.0);
                }
                ea = pycompat_sum(te, (size_t)ne);
                if (ea < 0.01 * frame_e) {
                    for (int k = 0; k < nalpha; k++)
                        if (alpha_keys[k] == e)
                            alpha_val[k] = 0;
                } else {
                    out->bets[out->nbets++] = e;
                }
            }
        }
        /* bets ascending (insertion). */
        for (int k = 1; k < out->nbets; k++) {
            int t = out->bets[k], j = k - 1;
            while (j >= 0 && out->bets[j] > t) {
                out->bets[j + 1] = out->bets[j];
                j--;
            }
            out->bets[j + 1] = t;
        }
    }
    allc = 0;
    if (npairs > 0) {
        allc = 1;
        for (int p = 0; p < npairs; p++)
            if (!out->used[p])
                allc = 0;
    }
    /* alphas in ascending pair order (insertion on keys). */
    for (int k = 1; k < nalpha; k++) {
        int tk = alpha_keys[k], tv = alpha_val[k];
        int j = k - 1;
        while (j >= 0 && alpha_keys[j] > tk) {
            alpha_keys[j + 1] = alpha_keys[j];
            alpha_val[j + 1] = alpha_val[j];
            j--;
        }
        alpha_keys[j + 1] = tk;
        alpha_val[j + 1] = tv;
    }
    out->nalphas = nalpha;
    prev = 0;
    for (int k = 0; k < nalpha; k++) {
        out->alphas[k] = alpha_val[k];
        out->deltas[k] = alpha_val[k] - prev;
        prev = alpha_val[k];
    }
    out->ndeltas = nalpha;
    /* Copy legs into owned storage. */
    out->n0 = leg0.n;
    memcpy(out->lines0, leg0.lines,
           (size_t)leg0.n * sizeof *out->lines0);
    memcpy(out->qs0, leg0.qs,
           (size_t)leg0.n * sizeof *out->qs0);
    memcpy(out->refs0, leg0.refs, sizeof out->refs0);
    out->n1 = leg1.n;
    memcpy(out->lines1, leg1.lines,
           (size_t)leg1.n * sizeof *out->lines1);
    memcpy(out->qs1, leg1.qs,
           (size_t)leg1.n * sizeof *out->qs1);
    memcpy(out->refs1, leg1.refs, sizeof out->refs1);
    out->gmax[0] = msfb;
    for (int p = 0; p < npairs; p++)
        out->used_arr[p] = out->used[p];
    for (int k = 0; k < nalpha; k++)
        out->deltas_arr[k] = out->deltas[k];
    out->leg_refs[0] = leg0.n > 0 ? leg0.ref : 100;
    out->leg_refs[1] = leg1.n > 0 ? leg1.ref : 100;
    out->cp.gmax = out->gmax;
    out->cp.msbits = NULL;
    out->cp.nmsbits = 0;
    out->cp.has_allc = 1;
    out->cp.allc = allc;
    out->cp.used = allc ? NULL : out->used_arr;
    out->cp.nused = allc ? 0 : npairs;
    out->cp.has_dct = 0;
    out->cp.dct = 0;
    out->cp.deltas = nalpha > 0 ? out->deltas_arr : NULL;
    out->cp.ndeltas = nalpha;
    out->lm.lines = out->lines0;
    out->lm.qs = out->qs0;
    out->lm.n = out->n0;
    out->lm.refs_by_band = out->refs0;
    out->ls.lines = out->lines1;
    out->ls.qs = out->qs1;
    out->ls.n = out->n1;
    out->ls.refs_by_band = out->refs1;
    out->joint.kind = ENC_MC3_JOINT_LONG;
    out->joint.u.lon.msfb = msfb;
    out->joint.u.lon.sap = 3;
    out->joint.u.lon.legM = out->lm;
    out->joint.u.lon.legS = out->ls;
    out->joint.u.lon.offs = NULL;
    out->joint.u.lon.cp = &out->cp;
    out->joint.u.lon.leg_refs[0] = out->leg_refs[0];
    out->joint.u.lon.leg_refs[1] = out->leg_refs[1];
    out->joint.u.lon.has_leg_refs = 1;
    return 0;
}
