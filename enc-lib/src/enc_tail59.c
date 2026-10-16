#include "enc_tail59.h"

#include "enc_aspx.h"

static int run_ec(bitwriter_t *w, const int *idxs, int n, int dt,
                  int is_noise, int is_balance, int qm) {
    char name[27];
    const aspx_book_t *dtb;
    const aspx_book_t *f0b;
    const aspx_book_t *dfb;

    if (dt) {
        if (enc_aspx_book_name(name, sizeof name, is_noise,
                               is_balance, qm, "DT"))
            return -1;
        dtb = enc_aspx_book_by_name(name);
        if (dtb == NULL)
            return -1;
        return enc_aspx_write_ec_time(w, idxs, NULL, n, dtb);
    }
    if (enc_aspx_book_name(name, sizeof name, is_noise,
                           is_balance, qm, "F0"))
        return -1;
    f0b = enc_aspx_book_by_name(name);
    if (f0b == NULL)
        return -1;
    if (enc_aspx_book_name(name, sizeof name, is_noise,
                           is_balance, qm, "DF"))
        return -1;
    dfb = enc_aspx_book_by_name(name);
    if (dfb == NULL)
        return -1;
    return enc_aspx_write_ec_freq(w, idxs, n, f0b, dfb);
}

static int write_2ch_ec(bitwriter_t *w, const enc_tail_s2ch_t *s) {
    int ch1bal = s->balance == 1;

    if (s->qm0 != 0 && s->qm0 != 1)
        return -1;
    if (s->qm1 != 0 && s->qm1 != 1)
        return -1;
    if (run_ec(w, s->sig0, s->nsig0, s->delta_dir[0] == 1, 0, 0,
               s->qm0))
        return -1;
    if (run_ec(w, s->sig1, s->nsig1, s->delta_dir[2] == 1, 0,
               ch1bal, s->qm1))
        return -1;
    if (run_ec(w, s->noise0, s->nnoise0, s->delta_dir[1] == 1, 1,
               0, 0))
        return -1;
    if (run_ec(w, s->noise1, s->nnoise1, s->delta_dir[3] == 1, 1,
               ch1bal, 0))
        return -1;
    return 0;
}

static int write_1ch_ec(bitwriter_t *w, const enc_tail_s1ch_t *h) {
    if (h->qm != 0 && h->qm != 1)
        return -1;
    if (run_ec(w, h->sig, h->nsig, h->delta_dir[0] == 1, 0, 0,
               h->qm))
        return -1;
    if (run_ec(w, h->noise, h->nnoise, h->delta_dir[1] == 1, 1,
               0, 0))
        return -1;
    return 0;
}

static int write_acpl_side(bitwriter_t *w, const enc_tail_acpl_t *a,
                           const enc_acpl_books_t *abooks) {
    return enc_acpl_write_1ch(w, a->interp, a->alpha_f0,
                              a->alpha_dfs, a->nalpha, a->beta_f0,
                              a->beta_dfs, a->nbeta, abooks,
                              a->has_slot, a->slot, 0,
                              a->beta_first);
}

int enc_tail_write_steady(bitwriter_t *w,
                          const enc_tail_s2ch_t *s2ch,
                          const enc_tail_s1ch_t *s1ch,
                          const enc_tail_acpl_t *a0,
                          const enc_tail_acpl_t *a1,
                          const enc_acpl_books_t *abooks,
                          int has_xover, int xover) {
    enc_aspx_2ch_head_t h2;
    enc_aspx_1ch_head_t h1;
    int i;

    /* 2ch head: tail59 leaves int_class/tmp_num_env at 0. */
    h2.int_class0 = 0;
    h2.tmp_num_env0 = 0;
    h2.balance = s2ch->balance;
    h2.int_class1 = 0;
    h2.tmp_num_env1 = 0;
    for (i = 0; i < 4; i++)
        h2.delta_dir[i] = s2ch->delta_dir[i];
    for (i = 0; i < 3; i++) {
        h2.tna0[i] = s2ch->tna0[i];
        h2.tna1[i] = s2ch->tna1[i];
    }
    h2.ah_left = s2ch->ah_left;
    h2.ah_right = s2ch->ah_right;
    h2.add_harm0 = s2ch->add_harm0;
    h2.add_harm1 = s2ch->add_harm1;
    h2.fic = s2ch->fic;
    h2.tic = s2ch->tic;
    h2.has_xover = has_xover;
    h2.xover = xover;
    if (enc_aspx_write_2ch_head(w, &h2))
        return -1;
    if (write_2ch_ec(w, s2ch))
        return -1;
    /* 1ch head: int_class/tmp_num_env/fic/tic stay 0. */
    h1.int_class = 0;
    h1.tmp_num_env = 0;
    h1.delta_dir[0] = s1ch->delta_dir[0];
    h1.delta_dir[1] = s1ch->delta_dir[1];
    for (i = 0; i < 3; i++)
        h1.tna[i] = s1ch->tna[i];
    h1.ah_present = s1ch->ah_present;
    h1.add_harm = s1ch->add_harm;
    h1.fic = 0;
    h1.tic = 0;
    h1.has_xover = has_xover;
    h1.xover = xover;
    if (enc_aspx_write_1ch_head(w, &h1))
        return -1;
    if (write_1ch_ec(w, s1ch))
        return -1;
    if (write_acpl_side(w, a0, abooks))
        return -1;
    if (write_acpl_side(w, a1, abooks))
        return -1;
    return 0;
}
