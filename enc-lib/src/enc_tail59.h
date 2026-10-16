#ifndef AC4_ENC_TAIL59_H
#define AC4_ENC_TAIL59_H

/* Frame-level ASPX/ACPL tail assembler: C port of
 * encoder/tail59.py (public entry write_tail_steady only;
 * _run/_write_*_ec stay static like the Python privates).
 * Tail = aspx_data_2ch + aspx_data_1ch + acpl_data_1ch#0 +
 * acpl_data_1ch#1 (A-59 block order; 220b at fr2 steady).
 * ASPX books resolve from the committed table by A-64 name;
 * ACPL books come from the caller (ENC_ACPL_FINE_BOOKS).
 */
#include "bits.h"
#include "enc_acpl.h"

/* 2ch side params. sig/noise runs are wire idx lists;
 * delta_dir entries pick FREQ(0)/TIME(1) per run. */
typedef struct {
    int balance;
    int delta_dir[4];
    int tna0[3];
    int tna1[3];
    int ah_left;
    int ah_right;
    int add_harm0;
    int add_harm1;
    int fic;
    int tic;
    const int *sig0;
    int nsig0;
    const int *sig1;
    int nsig1;
    const int *noise0;
    int nnoise0;
    const int *noise1;
    int nnoise1;
    int qm0;
    int qm1;
} enc_tail_s2ch_t;

/* 1ch side params (same run conventions). */
typedef struct {
    int delta_dir[2];
    int tna[3];
    int ah_present;
    int add_harm;
    const int *sig;
    int nsig;
    const int *noise;
    int nnoise;
    int qm;
} enc_tail_s1ch_t;

/* One acpl_data_1ch side: (f0, dfs) per param, optional
 * slot, beta_first wire order. */
typedef struct {
    int interp;
    int alpha_f0;
    const int *alpha_dfs;
    int nalpha;
    int beta_f0;
    const int *beta_dfs;
    int nbeta;
    int has_slot;
    int slot;
    int beta_first;
} enc_tail_acpl_t;

/* One steady joint-LONG tail. has_xover/xover: None (P) or
 * 0..7 (I: 3b at EACH aspx head). 0 ok, -1 on any
 * out-of-range field, bad qm, or out-of-book symbol. */
int enc_tail_write_steady(bitwriter_t *w,
                          const enc_tail_s2ch_t *s2ch,
                          const enc_tail_s1ch_t *s1ch,
                          const enc_tail_acpl_t *a0,
                          const enc_tail_acpl_t *a1,
                          const enc_acpl_books_t *abooks,
                          int has_xover, int xover);

#endif
