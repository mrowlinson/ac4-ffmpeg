#ifndef AC4_ENC_ASPX_H
#define AC4_ENC_ASPX_H

/* Encoder-side ASPX tail emitters.
 * (Decoder-side tables/noise live in aspx_tables.[ch] and
 * aspx_noise.[ch]; this module only writes bits + q laws.)
 * Steady shapes per viewer A-59 sections 2-3 + A-64 book map.
 */
#include <stddef.h>

#include "aspx_tables.h"
#include "bits.h"

/* aspx_data_2ch head params. 1b fields must be 0/1, tna 0..3,
 * add_harm 12b (checked iff its gate is set), xover 0..7
 * (iff has_xover). */
typedef struct {
    int int_class0;
    int tmp_num_env0;
    int balance;
    int int_class1;
    int tmp_num_env1;
    int delta_dir[4];
    int tna0[3];
    int tna1[3];
    int ah_left;
    int ah_right;
    int add_harm0;
    int add_harm1;
    int fic;
    int tic;
    int has_xover;
    int xover;
} enc_aspx_2ch_head_t;

/* aspx_data_1ch head params (same field contracts; 2-wide
 * delta_dir; single tna/ah/add_harm). */
typedef struct {
    int int_class;
    int tmp_num_env;
    int delta_dir[2];
    int tna[3];
    int ah_present;
    int add_harm;
    int fic;
    int tic;
    int has_xover;
    int xover;
} enc_aspx_1ch_head_t;

/* 2ch framing + hfgen (25b at steady defaults). 0 ok, -1 on
 * out-of-range field. */
int enc_aspx_write_2ch_head(bitwriter_t *w,
                            const enc_aspx_2ch_head_t *h);

/* 1ch framing + hfgen (25b at steady defaults). 0 ok, -1 on
 * out-of-range field. */
int enc_aspx_write_1ch_head(bitwriter_t *w,
                            const enc_aspx_1ch_head_t *h);

/* ASPX_HCB_* book name for one ec index run (A-64 map).
 * ht one of "F0"/"DF"/"DT", qm 0/1. Needs 27 bytes in out
 * (longest "ASPX_HCB_ENV_BALANCE_30_DT" + NUL). 0 ok, -1 on
 * bad args or short buffer. */
int enc_aspx_book_name(char *out, size_t cap, int is_noise,
                       int is_balance, int qm, const char *ht);

/* Committed book by full name (linear search over the 18).
 * NULL when unknown. */
const aspx_book_t *enc_aspx_book_by_name(const char *name);

/* 12 sbg sine flags (each 0/1) -> 12b mask, band0=MSB.
 * -1 on bad flag value. */
int enc_aspx_harm_mask(const int *flags);

/* FREQ ec run: F0 absolute + DF diffs (DPCM along freq).
 * 0 ok, -1 on empty run or out-of-book symbol. */
int enc_aspx_write_ec_freq(bitwriter_t *w, const int *idxs, int n,
                           const aspx_book_t *f0,
                           const aspx_book_t *df);

/* TIME ec run: DT book for EVERY index, no F0. prev NULL =
 * all-zero reference. 0 ok, -1 on empty run, NULL idxs,
 * or out-of-book symbol. */
int enc_aspx_write_ec_time(bitwriter_t *w, const int *idxs,
                           const int *prev_or_null, int n,
                           const aspx_book_t *dt);

/* A-72 signal law: q = round(a*log2(E/P)), a=2/1 for qm 0/1.
 * 0 ok (*out set), -1 unless E>0, P>0, finite, qm 0/1. */
int enc_aspx_sig_q(double e, double p, int qm, int *out);

/* A-72+A-74 noise law: q = round(6-log2(scf)).
 * 0 ok (*out set), -1 unless scf>0 and finite result. */
int enc_aspx_noise_q(double scf, int *out);

/* Decoded-q run -> wire idxs + decoded scf for FREQ dir.
 * F0 clamps to [0, entries); DF diffs saturate with
 * propagation; Ps82 sign-carry. scf_out may be NULL.
 * 0 ok, -1 unless n>0 and outs valid. */
int enc_aspx_freq_wire(const int *qs, int n,
                       const aspx_book_t *f0,
                       const aspx_book_t *df, int *wire_out,
                       int *scf_out);

/* Decoded-q run -> wire idxs + decoded scf for TIME dir.
 * Diffs against prev (NULL = zeros), saturated, no
 * sign-carry. scf_out may be NULL. 0 ok, -1 unless n>0. */
int enc_aspx_time_wire(const int *qs, const int *prev_or_null,
                       int n, const aspx_book_t *dt, int *wire_out,
                       int *scf_out);

/* Bit cost of a FREQ wire run (book lengths). -1 unless n>0
 * and every symbol in book (Python IndexError/wraparound
 * out of contract). */
int enc_aspx_run_cost_freq(const int *wire, int n,
                           const aspx_book_t *f0,
                           const aspx_book_t *df);

/* Bit cost of a TIME wire run against prev (NULL = zeros).
 * -1 on the same out-of-contract inputs. */
int enc_aspx_run_cost_time(const int *wire,
                           const int *prev_or_null, int n,
                           const aspx_book_t *dt);

/* Cheaper entropy path for one run: 0 FREQ / 1 TIME, ties
 * to FREQ. -1 unless n>0. */
int enc_aspx_pick_dir(const int *qs, const int *prev_or_null,
                      int n, const aspx_book_t *f0,
                      const aspx_book_t *df, const aspx_book_t *dt);

#endif
