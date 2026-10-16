#include "enc_acpl.h"

#include "huffman_tables.h"

const enc_acpl_books_t ENC_ACPL_FINE_BOOKS = {
    {ACPL_HCB_ALPHA_FINE_F0_CW, ACPL_HCB_ALPHA_FINE_F0_LEN, 33, 0},
    {ACPL_HCB_ALPHA_FINE_DF_CW, ACPL_HCB_ALPHA_FINE_DF_LEN, 65, 32},
    {ACPL_HCB_BETA_FINE_F0_CW, ACPL_HCB_BETA_FINE_F0_LEN, 9, 0},
    {ACPL_HCB_BETA_FINE_DF_CW, ACPL_HCB_BETA_FINE_DF_LEN, 17, 8},
};

int enc_acpl_write_param(bitwriter_t *w, int f0, const int *dfs,
                         int ndfs, const enc_acpl_book_t *f0book,
                         const enc_acpl_book_t *dfbook, int diff_t) {
    if (diff_t < 0 || diff_t > 1)
        return -1;
    if (f0 < 0 || f0 >= f0book->entries)
        return -1;
    bw_put(w, (unsigned)diff_t, 1);
    bw_put(w, f0book->cw[f0], f0book->ln[f0]);
    for (int i = 0; i < ndfs; i++) {
        int sym = dfs[i] + dfbook->off;
        if (sym < 0 || sym >= dfbook->entries)
            return -1;
        bw_put(w, dfbook->cw[sym], dfbook->ln[sym]);
    }
    return 0;
}

int enc_acpl_write_1ch(bitwriter_t *w, int interp, int alpha_f0,
                       const int *alpha_dfs, int nalpha, int beta_f0,
                       const int *beta_dfs, int nbeta,
                       const enc_acpl_books_t *books, int has_slot,
                       int slot_val, int nps, int beta_first) {
    const int *f0s[2];
    const int *dfss[2];
    int ndfs[2];
    const enc_acpl_book_t *f0b[2], *dfb[2];
    int ia = beta_first ? 1 : 0;

    if (interp < 0 || interp > 1)
        return -1;
    if (nps != 0)
        return -1;
    if (has_slot && (slot_val < 0 || slot_val > 31))
        return -1;
    bw_put(w, (unsigned)interp, 1);
    bw_put(w, 0u, 1);
    if (has_slot)
        bw_put(w, (unsigned)slot_val, 5);
    f0s[ia] = &alpha_f0;
    dfss[ia] = alpha_dfs;
    ndfs[ia] = nalpha;
    f0b[ia] = &books->alpha_f0;
    dfb[ia] = &books->alpha_df;
    f0s[1 - ia] = &beta_f0;
    dfss[1 - ia] = beta_dfs;
    ndfs[1 - ia] = nbeta;
    f0b[1 - ia] = &books->beta_f0;
    dfb[1 - ia] = &books->beta_df;
    for (int i = 0; i < 2; i++) {
        int rc = enc_acpl_write_param(w, *f0s[i], dfss[i], ndfs[i],
                                      f0b[i], dfb[i], 0);
        if (rc)
            return rc;
    }
    return 0;
}
