#include "enc_aspx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pycompat.h"

#define ENC_ASPX_NBOOKS 18

static int is_bit(int v) {
    return v == 0 || v == 1;
}

int enc_aspx_write_2ch_head(bitwriter_t *w,
                            const enc_aspx_2ch_head_t *h) {
    int i;

    if (!is_bit(h->int_class0) || !is_bit(h->tmp_num_env0) ||
        !is_bit(h->balance) || !is_bit(h->fic) || !is_bit(h->tic) ||
        !is_bit(h->ah_left) || !is_bit(h->ah_right))
        return -1;
    if (h->balance == 0 &&
        (!is_bit(h->int_class1) || !is_bit(h->tmp_num_env1)))
        return -1;
    for (i = 0; i < 4; i++) {
        if (!is_bit(h->delta_dir[i]))
            return -1;
    }
    for (i = 0; i < 3; i++) {
        if (h->tna0[i] < 0 || h->tna0[i] > 3 ||
            h->tna1[i] < 0 || h->tna1[i] > 3)
            return -1;
    }
    if (h->ah_left && (h->add_harm0 < 0 || h->add_harm0 >= (1 << 12)))
        return -1;
    if (h->ah_right &&
        (h->add_harm1 < 0 || h->add_harm1 >= (1 << 12)))
        return -1;
    if (h->has_xover && (h->xover < 0 || h->xover > 7))
        return -1;
    if (h->has_xover)
        bw_put(w, (unsigned)h->xover, 3);
    bw_put(w, (unsigned)h->int_class0, 1);
    bw_put(w, (unsigned)h->tmp_num_env0, 1);
    bw_put(w, (unsigned)h->balance, 1);
    if (h->balance == 0) {
        bw_put(w, (unsigned)h->int_class1, 1);
        bw_put(w, (unsigned)h->tmp_num_env1, 1);
    }
    for (i = 0; i < 4; i++)
        bw_put(w, (unsigned)h->delta_dir[i], 1);
    for (i = 0; i < 3; i++) {
        bw_put(w, (unsigned)h->tna0[i], 2);
    }
    for (i = 0; i < 3; i++) {
        bw_put(w, (unsigned)h->tna1[i], 2);
    }
    bw_put(w, (unsigned)h->ah_left, 1);
    if (h->ah_left)
        bw_put(w, (unsigned)h->add_harm0, 12);
    bw_put(w, (unsigned)h->ah_right, 1);
    if (h->ah_right)
        bw_put(w, (unsigned)h->add_harm1, 12);
    bw_put(w, (unsigned)h->fic, 1);
    bw_put(w, (unsigned)h->tic, 1);
    return 0;
}

int enc_aspx_write_1ch_head(bitwriter_t *w,
                            const enc_aspx_1ch_head_t *h) {
    int i;

    if (!is_bit(h->int_class) || !is_bit(h->tmp_num_env) ||
        !is_bit(h->ah_present) || !is_bit(h->fic) ||
        !is_bit(h->tic))
        return -1;
    for (i = 0; i < 2; i++) {
        if (!is_bit(h->delta_dir[i]))
            return -1;
    }
    for (i = 0; i < 3; i++) {
        if (h->tna[i] < 0 || h->tna[i] > 3)
            return -1;
    }
    if (h->ah_present &&
        (h->add_harm < 0 || h->add_harm >= (1 << 12)))
        return -1;
    if (h->has_xover && (h->xover < 0 || h->xover > 7))
        return -1;
    if (h->has_xover)
        bw_put(w, (unsigned)h->xover, 3);
    bw_put(w, (unsigned)h->int_class, 1);
    bw_put(w, (unsigned)h->tmp_num_env, 1);
    for (i = 0; i < 2; i++)
        bw_put(w, (unsigned)h->delta_dir[i], 1);
    for (i = 0; i < 3; i++)
        bw_put(w, (unsigned)h->tna[i], 2);
    bw_put(w, (unsigned)h->ah_present, 1);
    if (h->ah_present)
        bw_put(w, (unsigned)h->add_harm, 12);
    bw_put(w, (unsigned)h->fic, 1);
    bw_put(w, (unsigned)h->tic, 1);
    return 0;
}

int enc_aspx_book_name(char *out, size_t cap, int is_noise,
                       int is_balance, int qm, const char *ht) {
    const char *fam;
    char fambuf[32];
    int n;

    if (qm != 0 && qm != 1)
        return -1;
    if (strcmp(ht, "F0") && strcmp(ht, "DF") && strcmp(ht, "DT"))
        return -1;
    if (is_noise) {
        fam = is_balance ? "NOISE_BALANCE" : "NOISE_LEVEL";
    } else {
        n = snprintf(fambuf, sizeof fambuf, "ENV_%s_%s",
                     is_balance ? "BALANCE" : "LEVEL",
                     qm == 0 ? "15" : "30");
        if (n < 0 || (size_t)n >= sizeof fambuf)
            return -1;
        fam = fambuf;
    }
    n = snprintf(out, cap, "ASPX_HCB_%s_%s", fam, ht);
    if (n < 0 || (size_t)n >= cap)
        return -1;
    return 0;
}

const aspx_book_t *enc_aspx_book_by_name(const char *name) {
    int i;

    for (i = 0; i < ENC_ASPX_NBOOKS; i++) {
        if (!strcmp(aspx_books[i].name, name))
            return &aspx_books[i];
    }
    return NULL;
}

int enc_aspx_harm_mask(const int *flags) {
    int m = 0;
    int b;

    for (b = 0; b < 12; b++) {
        if (!is_bit(flags[b]))
            return -1;
        if (flags[b])
            m |= 1 << (11 - b);
    }
    return m;
}

static int emit_idx(bitwriter_t *w, const aspx_book_t *b, int idx) {
    if (idx < 0 || idx >= b->entries)
        return -1;
    bw_put(w, b->cw[idx], b->len[idx]);
    return 0;
}

int enc_aspx_write_ec_freq(bitwriter_t *w, const int *idxs, int n,
                           const aspx_book_t *f0,
                           const aspx_book_t *df) {
    int prev;
    int i;

    if (idxs == NULL || n <= 0)
        return -1;
    if (emit_idx(w, f0, idxs[0]))
        return -1;
    prev = idxs[0];
    for (i = 1; i < n; i++) {
        if (emit_idx(w, df, idxs[i] - prev + df->off))
            return -1;
        prev = idxs[i];
    }
    return 0;
}

int enc_aspx_write_ec_time(bitwriter_t *w, const int *idxs,
                           const int *prev_or_null, int n,
                           const aspx_book_t *dt) {
    int i;

    if (idxs == NULL || n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        int p = prev_or_null == NULL ? 0 : prev_or_null[i];
        if (emit_idx(w, dt, idxs[i] - p + dt->off))
            return -1;
    }
    return 0;
}

int enc_aspx_sig_q(double e, double p, int qm, int *out) {
    double r;
    double a;

    if (qm != 0 && qm != 1)
        return -1;
    if (!(e > 0.0) || !(p > 0.0))
        return -1;
    a = qm == 0 ? 2.0 : 1.0;
    r = a * log2(e / p);
    if (!isfinite(r))
        return -1;
    *out = pycompat_round_half_even(r);
    return 0;
}

int enc_aspx_noise_q(double scf, int *out) {
    double r;

    if (!(scf > 0.0))
        return -1;
    r = 6.0 - log2(scf);
    if (!isfinite(r))
        return -1;
    *out = pycompat_round_half_even(r);
    return 0;
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

int enc_aspx_freq_wire(const int *qs, int n,
                       const aspx_book_t *f0,
                       const aspx_book_t *df, int *wire_out,
                       int *scf_out) {
    int f0max;
    int dmin;
    int dmax;
    int i;

    if (qs == NULL || n <= 0 || wire_out == NULL)
        return -1;
    f0max = f0->entries - 1;
    dmin = -df->off;
    dmax = df->entries - 1 - df->off;
    wire_out[0] = clamp_int(qs[0], 0, f0max);
    for (i = 1; i < n; i++) {
        int d = clamp_int(qs[i] - wire_out[i - 1], dmin, dmax);
        wire_out[i] = wire_out[i - 1] + d;
    }
    if (scf_out != NULL) {
        for (i = 0; i < n; i++)
            scf_out[i] = wire_out[i];
        if (n > 1 && wire_out[0] == 0 && scf_out[1] < 0)
            scf_out[0] = scf_out[1];
    }
    return 0;
}

int enc_aspx_time_wire(const int *qs, const int *prev_or_null,
                       int n, const aspx_book_t *dt, int *wire_out,
                       int *scf_out) {
    int dmin;
    int dmax;
    int i;

    if (qs == NULL || n <= 0 || wire_out == NULL)
        return -1;
    dmin = -dt->off;
    dmax = dt->entries - 1 - dt->off;
    for (i = 0; i < n; i++) {
        int p = prev_or_null == NULL ? 0 : prev_or_null[i];
        wire_out[i] = p + clamp_int(qs[i] - p, dmin, dmax);
        if (scf_out != NULL)
            scf_out[i] = wire_out[i];
    }
    return 0;
}

int enc_aspx_run_cost_freq(const int *wire, int n,
                           const aspx_book_t *f0,
                           const aspx_book_t *df) {
    int cost;
    int i;

    if (wire == NULL || n <= 0)
        return -1;
    if (wire[0] < 0 || wire[0] >= f0->entries)
        return -1;
    cost = f0->len[wire[0]];
    for (i = 1; i < n; i++) {
        int s = wire[i] - wire[i - 1] + df->off;
        if (s < 0 || s >= df->entries)
            return -1;
        cost += df->len[s];
    }
    return cost;
}

int enc_aspx_run_cost_time(const int *wire,
                           const int *prev_or_null, int n,
                           const aspx_book_t *dt) {
    int cost = 0;
    int i;

    if (wire == NULL || n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        int p = prev_or_null == NULL ? 0 : prev_or_null[i];
        int s = wire[i] - p + dt->off;
        if (s < 0 || s >= dt->entries)
            return -1;
        cost += dt->len[s];
    }
    return cost;
}

int enc_aspx_pick_dir(const int *qs, const int *prev_or_null,
                      int n, const aspx_book_t *f0,
                      const aspx_book_t *df, const aspx_book_t *dt) {
    int *wf;
    int *wt;
    int cf;
    int ct;
    int dir;

    if (qs == NULL || n <= 0)
        return -1;
    wf = malloc(sizeof *wf * (size_t)n);
    wt = malloc(sizeof *wt * (size_t)n);
    if (wf == NULL || wt == NULL) {
        free(wf);
        free(wt);
        return -1;
    }
    enc_aspx_freq_wire(qs, n, f0, df, wf, NULL);
    enc_aspx_time_wire(qs, prev_or_null, n, dt, wt, NULL);
    cf = enc_aspx_run_cost_freq(wf, n, f0, df);
    ct = enc_aspx_run_cost_time(wt, prev_or_null, n, dt);
    dir = (ct >= 0 && cf >= 0 && ct < cf) ? 1 : 0;
    free(wf);
    free(wt);
    return dir;
}
