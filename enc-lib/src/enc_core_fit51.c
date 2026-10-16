/* enc_core_fit51.c: mode-3 5.1 CBR rate loop. See enc_core.h
 * for the contract. */
#include "enc_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "asf.h"
#include "pycompat.h"
#include "sfb_offsets.h"

/* Empty-coeff mono leg -> proven silence-leg shape. */
static void
quiet_leg(enc_core_leg_t *leg)
{
    if (leg->n == 0) {
        for (int b = 0; b < 63; b++)
            leg->refs[b] = -1;
        leg->ref = 100;
        leg->msfb = 1;
    }
}

static void
leg_to_mc3(const enc_core_leg_t *leg, enc_mc3_leg_t *out)
{
    out->lines = leg->lines;
    out->qs = leg->qs;
    out->n = leg->n;
    out->ref = leg->ref;
    out->refs_by_band = leg->n > 0 ? leg->refs : NULL;
    out->has_refs = leg->n > 0 ? 1 : 0;
    out->msfb = leg->msfb;
}

int
enc_core_fit_frame_51(int i, const double *const *Xs,
                      const int *lens, int raw_size, int iframe,
                      const enc_acpl_books_t *abooks,
                      const enc_core_stamp_t *stamp51,
                      enc_core_fit_info_t *info, uint8_t *out,
                      size_t outcap)
{
    static const int kcaps[6] = { 63, 31, 15, 7, 3, 1 };
    const int *offs = sfb_offsets48(2048);
    double peak = 0.0;
    double *XA = NULL, *XB = NULL;
    double peaks[6][63];
    enc_core_acpl_dual_t dual;
    enc_mc3_quiet_t quiet;
    enc_mc3_tail_t tail;
    bitwriter_t w;
    uint8_t *sil_audio = NULL;
    int n_sil = 0;
    uint8_t *sil_body = NULL;
    int n_sil_body = -1;
    int n = -1;
    if (!Xs || !lens || !offs || !abooks || !info || !out)
        return -1;
    for (int c = 0; c < 6; c++) {
        if (!Xs[c] || lens[c] < 0)
            return -1;
        for (int k = 0; k < lens[c]; k++) {
            double a = fabs(Xs[c][k]);
            if (a > peak)
                peak = a;
        }
    }
    bw_init(&w);
    if (enc_mc3_write_audio_51_mode3_silence(&w, !!iframe,
                                             abooks)) {
        bw_free(&w);
        return -1;
    }
    {
        size_t nb = bw_nbits(&w) / 8;
        if (bw_nbits(&w) % 8 != 0) {
            bw_free(&w);
            return -1;
        }
        sil_audio = malloc(nb ? nb : 1);
        if (!sil_audio ||
            bw_bytes(&w, sil_audio, nb) != nb) {
            free(sil_audio);
            bw_free(&w);
            return -1;
        }
        n_sil = (int)nb;
    }
    bw_free(&w);
    sil_body = malloc(65536);
    if (!sil_body) {
        free(sil_audio);
        return -1;
    }
    n_sil_body = enc_core_assemble_51(i, sil_audio, n_sil,
                                      raw_size, !!iframe,
                                      stamp51, 0, NULL,
                                      NULL, sil_body,
                                      65536);
    if (peak <= 0.0) {
        /* Exact-digital-zero short-circuit (A-155). */
        if (n_sil_body < 0)
            goto done;
        if ((size_t)n_sil_body > outcap)
            goto done;
        memcpy(out, sil_body, (size_t)n_sil_body);
        info->kcap = 0;
        info->shift = 0;
        info->msfb[0] = info->msfb[1] = info->msfb[2] = 1;
        info->nbets = 0;
        info->audio_len = n_sil;
        n = n_sil_body;
        goto done;
    }
    {
        int nXA = lens[0] < lens[4] ? lens[0] : lens[4];
        int nXB = lens[1] < lens[5] ? lens[1] : lens[5];
        double floor_ = peak * 1e-3;
        XA = malloc((size_t)(nXA > 0 ? nXA : 1) *
                    sizeof *XA);
        XB = malloc((size_t)(nXB > 0 ? nXB : 1) *
                    sizeof *XB);
        if (!XA || !XB)
            goto done;
        for (int k = 0; k < nXA; k++)
            XA[k] = Xs[0][k] + Xs[4][k];
        for (int k = 0; k < nXB; k++)
            XB[k] = Xs[1][k] + Xs[5][k];
        for (int c = 0; c < 6; c++)
            if (enc_core_band_peaks(Xs[c], lens[c], offs,
                                    63, peaks[c]))
                goto done;
        if (enc_core_pack_acpl_dual(
                peaks[0], peaks[4], peaks[1], peaks[5],
                63, floor_ * floor_, 0, &dual))
            goto done;
        if (enc_mc3_quiet_aspx(0, &quiet))
            goto done;
        tail.s2ch = &quiet.s2ch;
        tail.s1ch = &quiet.s1ch;
        tail.a0 = &dual.a0;
        tail.a1 = &dual.a1;
        tail.abooks = abooks;
        for (int k = 0; k < 6; k++) {
            int shift = 0;
            while (shift <= ENC_CORE_SHIFT_MAX) {
                enc_core_joint_pack_t *pack =
                    malloc(sizeof *pack);
                enc_core_leg_t mono, lfe;
                int deg;
                if (!pack)
                    goto done;
                if (enc_core_pack_joint_long(
                        XA, nXA, XB, nXB, 0, 0.0, 48,
                        floor_, shift, kcaps[k], pack)) {
                    free(pack);
                    goto done;
                }
                if (enc_core_pack_mono(Xs[2], lens[2], 0,
                                       0.0, 48, shift,
                                       kcaps[k], &mono) ||
                    enc_core_pack_mono(Xs[3], lens[3], 0,
                                       0.0, 7, shift, 63,
                                       &lfe)) {
                    free(pack);
                    goto done;
                }
                quiet_leg(&mono);
                quiet_leg(&lfe);
                deg = pack->n0 == 0 && pack->n1 == 0 &&
                    mono.n == 0 && lfe.n == 0;
                if (deg) {
                    if (n_sil_body < 0) {
                        free(pack);
                        break;
                    }
                    if ((size_t)n_sil_body > outcap) {
                        free(pack);
                        goto done;
                    }
                    memcpy(out, sil_body,
                           (size_t)n_sil_body);
                    info->kcap = 0;
                    info->shift = shift;
                    info->msfb[0] = info->msfb[1] =
                        info->msfb[2] = 1;
                    info->nbets = 0;
                    info->audio_len = n_sil;
                    n = n_sil_body;
                    free(pack);
                    goto done;
                }
                {
                    enc_mc3_leg_t lfe_leg, mono_leg;
                    uint8_t *audio = NULL;
                    uint8_t *body = NULL;
                    int n_audio, n_body;
                    leg_to_mc3(&lfe, &lfe_leg);
                    leg_to_mc3(&mono, &mono_leg);
                    bw_init(&w);
                    if (enc_mc3_write_audio_51_mode3(
                            &w, !!iframe, &lfe_leg,
                            &pack->joint, &mono_leg,
                            &tail)) {
                        bw_free(&w);
                        free(pack);
                        goto done;
                    }
                    {
                        size_t nb = bw_nbits(&w) / 8;
                        if (bw_nbits(&w) % 8 != 0) {
                            bw_free(&w);
                            free(pack);
                            goto done;
                        }
                        audio = malloc(nb ? nb : 1);
                        if (!audio ||
                            bw_bytes(&w, audio, nb) !=
                                nb) {
                            free(audio);
                            bw_free(&w);
                            free(pack);
                            goto done;
                        }
                        n_audio = (int)nb;
                    }
                    bw_free(&w);
                    body = malloc(65536);
                    if (!body) {
                        free(audio);
                        free(pack);
                        goto done;
                    }
                    n_body = enc_core_assemble_51(
                        i, audio, n_audio, raw_size,
                        !!iframe, stamp51, 0, NULL, NULL,
                        body, 65536);
                    if (n_body >= 0) {
                        if ((size_t)n_body > outcap) {
                            free(audio);
                            free(body);
                            free(pack);
                            goto done;
                        }
                        memcpy(out, body, (size_t)n_body);
                        info->kcap = kcaps[k];
                        info->shift = shift;
                        info->msfb[0] = pack->msfb;
                        info->msfb[1] = mono.msfb;
                        info->msfb[2] = lfe.msfb;
                        for (int b = 0;
                             b < pack->nbets; b++)
                            info->bets[b] =
                                pack->bets[b];
                        info->nbets = pack->nbets;
                        info->audio_len = n_audio;
                        n = n_body;
                        free(audio);
                        free(body);
                        free(pack);
                        goto done;
                    }
                    free(audio);
                    free(body);
                }
                free(pack);
                shift += ENC_CORE_SHIFT_STEP;
            }
        }
    }
done:
    free(XA);
    free(XB);
    free(sil_audio);
    free(sil_body);
    return n;
}
