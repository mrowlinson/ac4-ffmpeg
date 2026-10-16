#ifndef AC4_ENC_ACPL_H
#define AC4_ENC_ACPL_H

/* Encoder-side A-CPL (AISP) param writer.
 * (Decoder-side parse/synth lives in acpl.[ch]; separate module.)
 * Framing per A-59 §4: interp 1b + nps 1b + slot 5b iff present,
 * then ALPHA/BETA runs in wire order (alpha first, or beta first
 * when beta_first). Each run: diffT 1b + F0 u(HC) + DFs s_var(HC).
 */
#include <stddef.h>

#include "bits.h"

typedef struct {
    const unsigned *cw;
    const unsigned char *ln;
    int entries;
    int off;
} enc_acpl_book_t;

typedef struct {
    enc_acpl_book_t alpha_f0;
    enc_acpl_book_t alpha_df;
    enc_acpl_book_t beta_f0;
    enc_acpl_book_t beta_df;
} enc_acpl_books_t;

extern const enc_acpl_books_t ENC_ACPL_FINE_BOOKS;

/* One ALPHA/BETA param run: diffT 1b + F0 + DFs. Returns 0 on
 * success, -1 on out-of-range symbol or bad diff_t. */
int enc_acpl_write_param(bitwriter_t *w, int f0, const int *dfs,
                         int ndfs, const enc_acpl_book_t *f0book,
                         const enc_acpl_book_t *dfbook, int diff_t);

/* One-channel block (steady single-set, nps must be 0). has_slot /
 * slot_val: optional 5b slot. beta_first: beta run before alpha.
 * Returns 0 on success, -1 on out-of-range symbol or bad shape. */
int enc_acpl_write_1ch(bitwriter_t *w, int interp, int alpha_f0,
                       const int *alpha_dfs, int nalpha, int beta_f0,
                       const int *beta_dfs, int nbeta,
                       const enc_acpl_books_t *books, int has_slot,
                       int slot_val, int nps, int beta_first);

#endif
