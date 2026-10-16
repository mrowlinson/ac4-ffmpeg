#include "enc_mc3.h"

#include <stddef.h>

#include "asf.h"
#include "enc_aspx.h"
#include "sfb_offsets.h"

#define MC3_MAX_SFB 64 /* == ASF group_plan bound */


/* BitWriter.put fit rule: 0 <= value < 2**nbits. */
static int fits(int value, int nbits) {
    if (value < 0 || nbits < 0)
        return 0;
    if (nbits == 0)
        return value == 0;
    if (nbits >= 31)
        return 1;
    return value < (1 << nbits);
}


void enc_mc3_default_iframe_cfg(enc_mc3_iframe_cfg_t *cfg) {
    cfg->qmode = 1;
    cfg->start = 5;
    cfg->stop = 0;
    cfg->mfscale = 1;
    cfg->interp = 1;
    cfg->preflat = 1;
    cfg->limiter = 1;
    cfg->noisesbg = 3;
    cfg->envbits = 0;
    cfg->freqres = 2;
    cfg->npb_id = 0;
    cfg->acpl_qm = 0;
}


int enc_mc3_write_iframe_cfg(bitwriter_t *w,
                             const enc_mc3_iframe_cfg_t *cfg) {
    if (w == NULL || cfg == NULL)
        return -1;
    if (!fits(cfg->qmode, 1) || !fits(cfg->start, 3) ||
        !fits(cfg->stop, 2) || !fits(cfg->mfscale, 1) ||
        !fits(cfg->interp, 1) || !fits(cfg->preflat, 1) ||
        !fits(cfg->limiter, 1) || !fits(cfg->noisesbg, 2) ||
        !fits(cfg->envbits, 1) || !fits(cfg->freqres, 2) ||
        !fits(cfg->npb_id, 2) || !fits(cfg->acpl_qm, 1))
        return -1;
    bw_put(w, (unsigned)cfg->qmode, 1);
    bw_put(w, (unsigned)cfg->start, 3);
    bw_put(w, (unsigned)cfg->stop, 2);
    bw_put(w, (unsigned)cfg->mfscale, 1);
    bw_put(w, (unsigned)cfg->interp, 1);
    bw_put(w, (unsigned)cfg->preflat, 1);
    bw_put(w, (unsigned)cfg->limiter, 1);
    bw_put(w, (unsigned)cfg->noisesbg, 2);
    bw_put(w, (unsigned)cfg->envbits, 1);
    bw_put(w, (unsigned)cfg->freqres, 2);
    bw_put(w, (unsigned)cfg->npb_id, 2);
    bw_put(w, (unsigned)cfg->acpl_qm, 1);
    return 0;
}


void enc_mc3_write_compand_off(bitwriter_t *w) {
    bw_put(w, 0, 1);
    bw_put(w, 0, 3);
    bw_put(w, 0, 1);
}


/* Validate one LFE/mono leg against the write_sf_data
 * asserts (msfb field width: 3 LFE / 6 mono). */
static int leg_check(const enc_mc3_leg_t *leg, int msfb_bits) {
    const int *offs;
    int seen[MC3_MAX_SFB];
    int b, i, first, pv;

    if (leg == NULL)
        return -1;
    if (!fits(leg->msfb, msfb_bits) || leg->msfb > 63)
        return -1;
    offs = sfb_offsets48(2048);
    if (offs == NULL)
        return -1;
    if (leg->ref < 0 || leg->ref > 255)
        return -1;
    if (leg->n < 0)
        return -1;
    if (leg->n > 0 && (leg->lines == NULL || leg->qs == NULL))
        return -1;
    for (b = 0; b < MC3_MAX_SFB; b++)
        seen[b] = 0;
    for (i = 0; i < leg->n; i++) {
        int ln = leg->lines[i], q = leg->qs[i];
        if (ln < 0 || ln >= offs[leg->msfb])
            return -1;
        if (q == 0 || q < -ASF_MAX_Q || q > ASF_MAX_Q)
            return -1;
        b = asf_band_of(ln, NULL, leg->msfb);
        if (b < 0)
            return -1;
        seen[b] = 1;
    }
    if (!leg->has_refs)
        return 0;
    if (leg->refs_by_band == NULL)
        return -1;
    first = 1;
    pv = 0;
    for (b = 0; b < leg->msfb; b++) {
        int v;
        if (!seen[b])
            continue;
        v = leg->refs_by_band[b];
        if (v < 0 || v > 255)
            return -1;
        if (first && v != leg->ref)
            return -1;
        if (!first && (v - pv < -60 || v - pv > 60))
            return -1;
        first = 0;
        pv = v;
    }
    if (first)
        return -1;
    return 0;
}


static void leg_emit(bitwriter_t *w, const enc_mc3_leg_t *leg) {
    asf_write_sf_data(w, leg->lines, leg->qs, leg->n, leg->ref,
                      leg->msfb,
                      leg->has_refs ? leg->refs_by_band : NULL,
                      leg->has_refs, NULL, 5);
}


int enc_mc3_write_audio_51_mode3(bitwriter_t *w, int iframe,
                                 const enc_mc3_leg_t *lfe,
                                 const enc_mc3_joint_t *joint,
                                 const enc_mc3_leg_t *mono,
                                 const enc_mc3_tail_t *tail) {
    static const enc_jshort_prefix_t short_prefix[2] = {
        { 1, 1 }, { 0, 1 }
    };
    enc_mc3_iframe_cfg_t cfg;

    if (w == NULL || lfe == NULL || joint == NULL ||
        mono == NULL || tail == NULL)
        return -1;
    if (tail->s2ch == NULL || tail->s1ch == NULL ||
        tail->a0 == NULL || tail->a1 == NULL ||
        tail->abooks == NULL)
        return -1;
    if (joint->kind != ENC_MC3_JOINT_LONG &&
        joint->kind != ENC_MC3_JOINT_SHORT)
        return -1;
    if (leg_check(lfe, 3))
        return -1;
    if (leg_check(mono, 6))
        return -1;
    bw_put(w, 3, 3);
    if (iframe) {
        enc_mc3_default_iframe_cfg(&cfg);
        enc_mc3_write_iframe_cfg(w, &cfg);
    }
    bw_put(w, (unsigned)lfe->msfb, 3);
    leg_emit(w, lfe);
    enc_mc3_write_compand_off(w);
    bw_put(w, 0, 1);
    if (joint->kind == ENC_MC3_JOINT_LONG) {
        const enc_mc3_joint_long_t *lon = &joint->u.lon;
        const int *lr = lon->has_leg_refs ? lon->leg_refs : NULL;
        bw_put(w, 1, 1);
        bw_put(w, 1, 1);
        if (enc_jshort_write_long_2ch(w, lon->msfb, lon->sap,
                                      &lon->legM, &lon->legS,
                                      lon->offs, lon->cp, lr))
            return -1;
    } else {
        const enc_mc3_joint_short_t *sh = &joint->u.sh;
        if (enc_jshort_write_short_2ch(
                w, short_prefix, 2, sh->tl0, sh->tl1, sh->m0,
                sh->m1, sh->grp7, sh->sap, sh->leg0, sh->leg1,
                sh->geo, sh->ngroups, sh->cp, sh->leg_refs))
            return -1;
    }
    bw_put(w, 0, 1);
    bw_put(w, 1, 1);
    bw_put(w, (unsigned)mono->msfb, 6);
    leg_emit(w, mono);
    if (enc_tail_write_steady(w, tail->s2ch, tail->s1ch, tail->a0,
                              tail->a1, tail->abooks,
                              iframe != 0, 0))
        return -1;
    bw_align(w, 0);
    return 0;
}


int enc_mc3_quiet_aspx(int freq, enc_mc3_quiet_t *out) {
    int d, i;

    if (out == NULL)
        return -1;
    d = freq ? 0 : 1;
    for (i = 0; i < 12; i++) {
        out->r2.sig0[i] = 0;
        out->r2.sig1[i] = 0;
        out->r1.sig[i] = 0;
    }
    for (i = 0; i < 3; i++) {
        out->r2.noise0[i] = 0;
        out->r2.noise1[i] = 0;
        out->r1.noise[i] = 0;
    }
    out->s2ch.balance = 0;
    for (i = 0; i < 4; i++)
        out->s2ch.delta_dir[i] = d;
    for (i = 0; i < 3; i++) {
        out->s2ch.tna0[i] = 0;
        out->s2ch.tna1[i] = 0;
    }
    out->s2ch.ah_left = 0;
    out->s2ch.ah_right = 0;
    out->s2ch.add_harm0 = 0;
    out->s2ch.add_harm1 = 0;
    out->s2ch.fic = 0;
    out->s2ch.tic = 0;
    out->s2ch.sig0 = out->r2.sig0;
    out->s2ch.nsig0 = 12;
    out->s2ch.sig1 = out->r2.sig1;
    out->s2ch.nsig1 = 12;
    out->s2ch.noise0 = out->r2.noise0;
    out->s2ch.nnoise0 = 3;
    out->s2ch.noise1 = out->r2.noise1;
    out->s2ch.nnoise1 = 3;
    out->s2ch.qm0 = 0;
    out->s2ch.qm1 = 0;
    out->s1ch.delta_dir[0] = d;
    out->s1ch.delta_dir[1] = d;
    for (i = 0; i < 3; i++)
        out->s1ch.tna[i] = 0;
    out->s1ch.ah_present = 0;
    out->s1ch.add_harm = 0;
    out->s1ch.sig = out->r1.sig;
    out->s1ch.nsig = 12;
    out->s1ch.noise = out->r1.noise;
    out->s1ch.nnoise = 3;
    out->s1ch.qm = 0;
    return 0;
}


/* One measured_aspx run: decoded qs -> wire + scf. */
static int meas_run(const int *qs, const int *prev_or_null, int n,
                    int is_noise, int *wire, int *scf, int *dir) {
    char name[27];
    const aspx_book_t *f0b, *dfb, *dtb;

    if (enc_aspx_book_name(name, sizeof name, is_noise, 0, 0,
                           "F0"))
        return -1;
    f0b = enc_aspx_book_by_name(name);
    if (f0b == NULL)
        return -1;
    if (enc_aspx_book_name(name, sizeof name, is_noise, 0, 0,
                           "DF"))
        return -1;
    dfb = enc_aspx_book_by_name(name);
    if (dfb == NULL)
        return -1;
    if (enc_aspx_book_name(name, sizeof name, is_noise, 0, 0,
                           "DT"))
        return -1;
    dtb = enc_aspx_book_by_name(name);
    if (dtb == NULL)
        return -1;
    if (prev_or_null == NULL) {
        *dir = 0;
    } else {
        *dir = enc_aspx_pick_dir(qs, prev_or_null, n, f0b, dfb,
                                 dtb);
        if (*dir < 0)
            return -1;
    }
    if (*dir == 0)
        return enc_aspx_freq_wire(qs, n, f0b, dfb, wire, scf);
    return enc_aspx_time_wire(qs, prev_or_null, n, dtb, wire,
                              scf);
}


int enc_mc3_measured_aspx(const enc_mc3_aspx2_in_t *in2,
                          const enc_mc3_aspx1_in_t *in1,
                          enc_mc3_measured_t *out) {
    int i;

    if (in2 == NULL || in1 == NULL || out == NULL)
        return -1;
    for (i = 0; i < 4; i++) {
        if (in2->has_prev[i] != 0 && in2->has_prev[i] != 1)
            return -1;
    }
    for (i = 0; i < 2; i++) {
        if (in1->has_prev[i] != 0 && in1->has_prev[i] != 1)
            return -1;
    }
    if (in2->harm0 < 0 || in2->harm0 >= (1 << 12) ||
        in2->harm1 < 0 || in2->harm1 >= (1 << 12) ||
        in1->harm < 0 || in1->harm >= (1 << 12))
        return -1;
    if (meas_run(in2->qsig0, in2->has_prev[0] ? in2->psig0 : NULL,
                 12, 0, out->wire2.sig0, out->scf2.sig0,
                 &out->s2ch.delta_dir[0]))
        return -1;
    if (meas_run(in2->qsig1, in2->has_prev[1] ? in2->psig1 : NULL,
                 12, 0, out->wire2.sig1, out->scf2.sig1,
                 &out->s2ch.delta_dir[1]))
        return -1;
    if (meas_run(in2->qnoise0,
                 in2->has_prev[2] ? in2->pnoise0 : NULL, 3, 1,
                 out->wire2.noise0, out->scf2.noise0,
                 &out->s2ch.delta_dir[2]))
        return -1;
    if (meas_run(in2->qnoise1,
                 in2->has_prev[3] ? in2->pnoise1 : NULL, 3, 1,
                 out->wire2.noise1, out->scf2.noise1,
                 &out->s2ch.delta_dir[3]))
        return -1;
    if (meas_run(in1->qsig, in1->has_prev[0] ? in1->psig : NULL,
                 12, 0, out->wire1.sig, out->scf1.sig,
                 &out->s1ch.delta_dir[0]))
        return -1;
    if (meas_run(in1->qnoise,
                 in1->has_prev[1] ? in1->pnoise : NULL, 3, 1,
                 out->wire1.noise, out->scf1.noise,
                 &out->s1ch.delta_dir[1]))
        return -1;
    out->s2ch.balance = 0;
    for (i = 0; i < 3; i++) {
        out->s2ch.tna0[i] = 0;
        out->s2ch.tna1[i] = 0;
    }
    out->s2ch.ah_left = in2->harm0 ? 1 : 0;
    out->s2ch.ah_right = in2->harm1 ? 1 : 0;
    out->s2ch.add_harm0 = in2->harm0;
    out->s2ch.add_harm1 = in2->harm1;
    out->s2ch.fic = 0;
    out->s2ch.tic = 0;
    out->s2ch.sig0 = out->wire2.sig0;
    out->s2ch.nsig0 = 12;
    out->s2ch.sig1 = out->wire2.sig1;
    out->s2ch.nsig1 = 12;
    out->s2ch.noise0 = out->wire2.noise0;
    out->s2ch.nnoise0 = 3;
    out->s2ch.noise1 = out->wire2.noise1;
    out->s2ch.nnoise1 = 3;
    out->s2ch.qm0 = 0;
    out->s2ch.qm1 = 0;
    for (i = 0; i < 3; i++)
        out->s1ch.tna[i] = 0;
    out->s1ch.ah_present = in1->harm ? 1 : 0;
    out->s1ch.add_harm = in1->harm;
    out->s1ch.sig = out->wire1.sig;
    out->s1ch.nsig = 12;
    out->s1ch.noise = out->wire1.noise;
    out->s1ch.nnoise = 3;
    out->s1ch.qm = 0;
    return 0;
}


int enc_mc3_write_audio_51_mode3_silence(
    bitwriter_t *w, int iframe, const enc_acpl_books_t *abooks) {
    int z14[14] = { 0 };
    int z10[10] = { 0 };
    enc_mc3_quiet_t q;
    enc_mc3_leg_t lfe;
    enc_mc3_leg_t mono;
    enc_mc3_joint_t joint;
    enc_mc3_joint_long_t *lon = &joint.u.lon;
    enc_mc3_tail_t tail;
    enc_tail_acpl_t a0;
    enc_tail_acpl_t a1;

    if (w == NULL || abooks == NULL)
        return -1;
    if (enc_mc3_quiet_aspx(0, &q))
        return -1;
    lfe.lines = NULL;
    lfe.qs = NULL;
    lfe.n = 0;
    lfe.ref = 100;
    lfe.refs_by_band = NULL;
    lfe.has_refs = 0;
    lfe.msfb = 1;
    mono = lfe;
    joint.kind = ENC_MC3_JOINT_LONG;
    lon->msfb = 1;
    lon->sap = 2;
    lon->legM.lines = NULL;
    lon->legM.qs = NULL;
    lon->legM.n = 0;
    lon->legM.refs_by_band = NULL;
    lon->legS = lon->legM;
    lon->offs = NULL;
    lon->cp = NULL;
    lon->leg_refs[0] = 100;
    lon->leg_refs[1] = 100;
    lon->has_leg_refs = 1;
    a0.interp = 0;
    a0.alpha_f0 = 16;
    a0.alpha_dfs = z14;
    a0.nalpha = 14;
    a0.beta_f0 = 0;
    a0.beta_dfs = z14;
    a0.nbeta = 14;
    a0.has_slot = 0;
    a0.slot = 0;
    a0.beta_first = 0;
    a1.interp = 0;
    a1.alpha_f0 = 16;
    a1.alpha_dfs = z10;
    a1.nalpha = 10;
    a1.beta_f0 = 0;
    a1.beta_dfs = z10;
    a1.nbeta = 10;
    a1.has_slot = 0;
    a1.slot = 0;
    a1.beta_first = 0;
    tail.s2ch = &q.s2ch;
    tail.s1ch = &q.s1ch;
    tail.a0 = &a0;
    tail.a1 = &a1;
    tail.abooks = abooks;
    return enc_mc3_write_audio_51_mode3(w, iframe, &lfe, &joint,
                                       &mono, &tail);
}
