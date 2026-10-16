/* ac4enc_lib.c: opaque-ctx streaming API (v1: buffered whole-stream).
 * See ac4enc_lib.h for the v1 contract notes. */
#include "ac4enc_lib.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "loudness.h"
#include "pycompat.h"

struct ac4enc_ctx {
    char *layout;
    int nch;
    int rate_kbps;
    char *interval;
    int *forced;
    int nforced;
    char *preset;
    double *window;
    int ns2_tone;
    int shorts;
    int joint_shorts;
    int priming;
    int iframe_base;
    int iframe_total;
    double target_db;
    int has_bake;
    double bake_db;
    enc_core_drc_t *drc;
    enc_core_drc_mode_t *modes;
    int *dmx;
    int has_measure;
    enc_core_measure_t ms;
    int domain; /* 0 none, 1 int, 2 float */
    int nsamples;
    int cap;
    int **pi;
    double **pf;
    int tailed;
    int encoded;
    int enc_err;
    uint8_t *blob;
    int blob_n;
    int *offs;
    int *lens;
    int *iframes;
    int nframes;
    int cursor;
};

static const int kRates[] = { 48, 64, 96, 128, 144, 192, 256,
                              288, 320, 384, 448, 512, 768 };

static char *
dup_str(const char *s)
{
    size_t n;
    char *d;
    if (!s)
        return NULL;
    n = strlen(s) + 1;
    d = malloc(n);
    if (d)
        memcpy(d, s, n);
    return d;
}

static void
free_pcm(ac4enc_ctx_t *c)
{
    if (c->pi) {
        for (int i = 0; i < c->nch; i++)
            free(c->pi[i]);
        free(c->pi);
        c->pi = NULL;
    }
    if (c->pf) {
        for (int i = 0; i < c->nch; i++)
            free(c->pf[i]);
        free(c->pf);
        c->pf = NULL;
    }
}

/* ---- pass-1 measure (same op order as the core prologues) ---- */

static int
measure_i(const int *const *chs, const int *lens, int nch,
          ac4enc_measure_t *out)
{
    double m = 0, f = 0, tf = 0;
    int peak = 0;
    int nb = 0, ng = 0;
    if (!chs || !lens || !out)
        return -EINVAL;
    if (nch != 2 && nch != 6)
        return -EINVAL;
    for (int c = 0; c < nch; c++)
        if (!chs[c] || lens[c] < 0)
            return -EINVAL;
    if (nch == 2) {
        double *d0 = NULL, *d1 = NULL;
        d0 = malloc((size_t)(lens[0] > 0 ? lens[0] : 1) *
                     sizeof *d0);
        d1 = malloc((size_t)(lens[1] > 0 ? lens[1] : 1) *
                     sizeof *d1);
        if (!d0 || !d1) {
            free(d0);
            free(d1);
            return -ENOMEM;
        }
        for (int k = 0; k < lens[0]; k++)
            d0[k] = (double)chs[0][k];
        for (int k = 0; k < lens[1]; k++)
            d1[k] = (double)chs[1][k];
        loud_integrated(d0, lens[0], d1, lens[1], 48000, &m,
                        &nb, &ng);
        loud_flatness(d0, lens[0], d1, lens[1], 2048, &f,
                      &tf);
        free(d0);
        free(d1);
        for (int k = 0; k < lens[0]; k++)
            if (abs(chs[0][k]) > peak)
                peak = abs(chs[0][k]);
        for (int k = 0; k < lens[1]; k++)
            if (abs(chs[1][k]) > peak)
                peak = abs(chs[1][k]);
    } else {
        double *dm[6];
        const double *dmp[6];
        double *mono = NULL;
        int nm = 0;
        for (int c = 0; c < 6; c++) {
            dm[c] = malloc(
                (size_t)(lens[c] > 0 ? lens[c] : 1) *
                sizeof *dm[c]);
            if (!dm[c]) {
                for (int k = 0; k < c; k++)
                    free(dm[k]);
                return -ENOMEM;
            }
            for (int k = 0; k < lens[c]; k++)
                dm[c][k] = (double)chs[c][k];
            dmp[c] = dm[c];
            if (lens[c] > nm)
                nm = lens[c];
        }
        loud_integrated_mc(dmp, lens, 48000, &m, &nb, &ng);
        mono = malloc((size_t)(nm > 0 ? nm : 1) *
                      sizeof *mono);
        if (!mono) {
            for (int c = 0; c < 6; c++)
                free(dm[c]);
            return -ENOMEM;
        }
        for (int k = 0; k < nm; k++) {
            long long s = 0;
            for (int c = 0; c < 6; c++)
                s += k < lens[c] ? chs[c][k] : 0;
            mono[k] = (double)s / 6.0;
        }
        loud_flatness(mono, nm, mono, nm, 2048, &f, &tf);
        free(mono);
        for (int c = 0; c < 6; c++) {
            for (int k = 0; k < lens[c]; k++)
                if (abs(chs[c][k]) > peak)
                    peak = abs(chs[c][k]);
            free(dm[c]);
        }
    }
    out->m_db = m;
    out->f_flat = f;
    out->tf_tone = tf;
    out->in_peak = peak;
    out->d_peak = 0.0;
    out->has_d_peak = 0;
    return 0;
}

static int
measure_f(const double *const *chs, const int *lens, int nch,
          ac4enc_measure_t *out)
{
    double m = 0, f = 0, tf = 0, peak = 0.0;
    int nb = 0, ng = 0;
    if (!chs || !lens || !out)
        return -EINVAL;
    if (nch != 2 && nch != 6)
        return -EINVAL;
    for (int c = 0; c < nch; c++)
        if (!chs[c] || lens[c] < 0)
            return -EINVAL;
    if (nch == 2) {
        loud_integrated(chs[0], lens[0], chs[1], lens[1],
                        48000, &m, &nb, &ng);
        loud_flatness(chs[0], lens[0], chs[1], lens[1], 2048,
                      &f, &tf);
        for (int k = 0; k < lens[0]; k++)
            if (fabs(chs[0][k]) > peak)
                peak = fabs(chs[0][k]);
        for (int k = 0; k < lens[1]; k++)
            if (fabs(chs[1][k]) > peak)
                peak = fabs(chs[1][k]);
    } else {
        double *mono = NULL;
        int nm = 0;
        for (int c = 0; c < 6; c++)
            if (lens[c] > nm)
                nm = lens[c];
        loud_integrated_mc(chs, lens, 48000, &m, &nb, &ng);
        mono = malloc((size_t)(nm > 0 ? nm : 1) *
                      sizeof *mono);
        if (!mono)
            return -ENOMEM;
        for (int k = 0; k < nm; k++) {
            double v6[6];
            for (int c = 0; c < 6; c++)
                v6[c] = k < lens[c] ? chs[c][k] : 0.0;
            mono[k] = pycompat_sum(v6, 6) / 6.0;
        }
        loud_flatness(mono, nm, mono, nm, 2048, &f, &tf);
        free(mono);
        for (int c = 0; c < 6; c++)
            for (int k = 0; k < lens[c]; k++)
                if (fabs(chs[c][k]) > peak)
                    peak = fabs(chs[c][k]);
    }
    out->m_db = m;
    out->f_flat = f;
    out->tf_tone = tf;
    out->in_peak =
        peak >= 2147483647.0 ? INT_MAX : (int)peak;
    out->d_peak = peak;
    out->has_d_peak = 1;
    return 0;
}

int
ac4enc_measure(const int *const *chs, const int *lens, int nch,
               double *m_db, double *f_flat, double *tf_tone,
               int *in_peak)
{
    ac4enc_measure_t full;
    int rc;
    if (!m_db || !f_flat || !tf_tone || !in_peak)
        return -EINVAL;
    rc = measure_i(chs, lens, nch, &full);
    if (rc)
        return rc;
    *m_db = full.m_db;
    *f_flat = full.f_flat;
    *tf_tone = full.tf_tone;
    *in_peak = full.in_peak;
    return 0;
}

int
ac4enc_measure_f(const double *const *chs, const int *lens,
                 int nch, double *m_db, double *f_flat,
                 double *tf_tone, int *in_peak)
{
    ac4enc_measure_t full;
    int rc;
    if (!m_db || !f_flat || !tf_tone || !in_peak)
        return -EINVAL;
    rc = measure_f(chs, lens, nch, &full);
    if (rc)
        return rc;
    *m_db = full.m_db;
    *f_flat = full.f_flat;
    *tf_tone = full.tf_tone;
    *in_peak = full.in_peak;
    return 0;
}

int
ac4enc_measure2(const int *const *chs, const int *lens, int nch,
                ac4enc_measure_t *out)
{
    return measure_i(chs, lens, nch, out);
}

int
ac4enc_measure2_f(const double *const *chs, const int *lens,
                  int nch, ac4enc_measure_t *out)
{
    return measure_f(chs, lens, nch, out);
}

/* ---- open ---- */

static int
rate_ok(int rate, int floor)
{
    int hit = 0;
    if (rate < floor)
        return 0;
    for (size_t i = 0; i < sizeof kRates / sizeof kRates[0];
         i++)
        if (kRates[i] == rate)
            hit = 1;
    return hit;
}

static int
is01(int v)
{
    return v == 0 || v == 1;
}

int
ac4enc_open(ac4enc_ctx_t **ctx, const ac4enc_opts_t *opts)
{
    ac4enc_ctx_t *c;
    int nch, floor;
    if (!ctx || !opts || !opts->layout)
        return -EINVAL;
    if (!strcmp(opts->layout, "stereo")) {
        nch = 2;
        floor = 48;
    } else if (!strcmp(opts->layout, "5.1")) {
        nch = 6;
        floor = 96;
    } else {
        return -EINVAL;
    }
    if (!rate_ok(opts->rate_kbps, floor))
        return -EINVAL;
    if (!(opts->target_db == 0.0 ||
          (-31.0 <= opts->target_db &&
           opts->target_db <= -10.0)))
        return -EINVAL;
    if (!is01(opts->ns2_tone) || !is01(opts->shorts) ||
        !is01(opts->joint_shorts) || !is01(opts->priming))
        return -EINVAL;
    if (opts->nforced < 0 ||
        (opts->nforced > 0 && !opts->forced))
        return -EINVAL;
    if (opts->preset &&
        enc_core_preset_id(opts->preset) < 0)
        return -EINVAL;
    if (opts->drc && (opts->drc->nmodes < 0 ||
                      (opts->drc->nmodes > 0 &&
                       !opts->drc->modes)))
        return -EINVAL;
    if (opts->has_measure) {
        if (isnan(opts->m_db) || opts->in_peak < 0)
            return -EINVAL;
        if (opts->has_d_peak && isnan(opts->d_peak))
            return -EINVAL;
    }
    c = calloc(1, sizeof *c);
    if (!c)
        return -ENOMEM;
    c->layout = dup_str(opts->layout);
    c->interval = dup_str(opts->interval);
    c->preset = dup_str(opts->preset);
    if (!c->layout ||
        (opts->interval && !c->interval) ||
        (opts->preset && !c->preset))
        goto oom;
    c->nch = nch;
    c->rate_kbps = opts->rate_kbps;
    c->nforced = opts->nforced;
    if (opts->nforced > 0) {
        c->forced = malloc((size_t)opts->nforced *
                           sizeof *c->forced);
        if (!c->forced)
            goto oom;
        memcpy(c->forced, opts->forced,
               (size_t)opts->nforced * sizeof *c->forced);
    }
    if (opts->window) {
        c->window = malloc(4096 * sizeof *c->window);
        if (!c->window)
            goto oom;
        memcpy(c->window, opts->window,
               4096 * sizeof *c->window);
    }
    c->ns2_tone = opts->ns2_tone;
    c->shorts = opts->shorts;
    c->joint_shorts = opts->joint_shorts;
    c->priming = opts->priming;
    c->iframe_base = opts->iframe_base;
    c->iframe_total = opts->iframe_total;
    c->target_db = opts->target_db;
    c->has_bake = opts->has_bake;
    c->bake_db = opts->bake_db;
    if (opts->drc) {
        c->drc = malloc(sizeof *c->drc);
        if (!c->drc)
            goto oom;
        c->drc->eac3 = opts->drc->eac3;
        c->drc->nmodes = opts->drc->nmodes;
        c->drc->modes = NULL;
        if (opts->drc->nmodes > 0) {
            c->modes = malloc((size_t)opts->drc->nmodes *
                              sizeof *c->modes);
            if (!c->modes)
                goto oom;
            memcpy(c->modes, opts->drc->modes,
                   (size_t)opts->drc->nmodes *
                       sizeof *c->modes);
            c->drc->modes = c->modes;
        }
    }
    if (opts->dmx) {
        c->dmx = malloc(5 * sizeof *c->dmx);
        if (!c->dmx)
            goto oom;
        memcpy(c->dmx, opts->dmx, 5 * sizeof *c->dmx);
    }
    c->has_measure = opts->has_measure;
    if (opts->has_measure) {
        c->ms.m_db = opts->m_db;
        c->ms.f_flat = opts->f_flat;
        c->ms.tf_tone = opts->tf_tone;
        c->ms.in_peak = opts->in_peak;
        c->ms.d_peak = opts->d_peak;
        c->ms.has_d_peak = opts->has_d_peak;
    }
    *ctx = c;
    return 0;
oom:
    ac4enc_close(c);
    return -ENOMEM;
}

/* ---- feed ---- */

static int
grow_for(ac4enc_ctx_t *c, int need)
{
    int want;
    if (need <= c->cap)
        return 0;
    want = c->cap > 0 ? c->cap : AC4ENC_FRAME_SAMPLES;
    while (want < need)
        want *= 2;
    if (c->domain == 1) {
        for (int i = 0; i < c->nch; i++) {
            int *row =
                realloc(c->pi[i], (size_t)want * sizeof *row);
            if (!row)
                return -ENOMEM;
            c->pi[i] = row;
        }
    } else {
        for (int i = 0; i < c->nch; i++) {
            double *row = realloc(c->pf[i], (size_t)want *
                                                sizeof *row);
            if (!row)
                return -ENOMEM;
            c->pf[i] = row;
        }
    }
    c->cap = want;
    return 0;
}

static int
feed_rows(ac4enc_ctx_t *c, int domain, int n)
{
    int rc;
    if (c->domain == 0) {
        c->domain = domain;
        if (domain == 1) {
            c->pi = calloc((size_t)c->nch, sizeof *c->pi);
            if (!c->pi)
                return -ENOMEM;
            for (int i = 0; i < c->nch; i++) {
                c->pi[i] = NULL;
            }
        } else {
            c->pf = calloc((size_t)c->nch, sizeof *c->pf);
            if (!c->pf)
                return -ENOMEM;
            for (int i = 0; i < c->nch; i++) {
                c->pf[i] = NULL;
            }
        }
    }
    if (c->domain != domain)
        return -EINVAL;
    rc = grow_for(c, c->nsamples + n);
    if (rc)
        return rc;
    return 0;
}

int
ac4enc_encode_frame(ac4enc_ctx_t *c, const int *const *pcm,
                    uint8_t *out, size_t outcap, int *is_iframe)
{
    int rc;
    (void)out;
    (void)outcap;
    (void)is_iframe;
    if (!c || !pcm)
        return -EINVAL;
    if (c->encoded || c->tailed)
        return -EINVAL;
    for (int i = 0; i < c->nch; i++)
        if (!pcm[i])
            return -EINVAL;
    rc = feed_rows(c, 1, AC4ENC_FRAME_SAMPLES);
    if (rc)
        return rc;
    for (int i = 0; i < c->nch; i++)
        memcpy(c->pi[i] + c->nsamples, pcm[i],
               AC4ENC_FRAME_SAMPLES * sizeof *pcm[i]);
    c->nsamples += AC4ENC_FRAME_SAMPLES;
    return 0;
}

int
ac4enc_encode_frame_f(ac4enc_ctx_t *c,
                      const double *const *pcm, uint8_t *out,
                      size_t outcap, int *is_iframe)
{
    int rc;
    (void)out;
    (void)outcap;
    (void)is_iframe;
    if (!c || !pcm)
        return -EINVAL;
    if (c->encoded || c->tailed)
        return -EINVAL;
    for (int i = 0; i < c->nch; i++)
        if (!pcm[i])
            return -EINVAL;
    rc = feed_rows(c, 2, AC4ENC_FRAME_SAMPLES);
    if (rc)
        return rc;
    for (int i = 0; i < c->nch; i++)
        memcpy(c->pf[i] + c->nsamples, pcm[i],
               AC4ENC_FRAME_SAMPLES * sizeof *pcm[i]);
    c->nsamples += AC4ENC_FRAME_SAMPLES;
    return 0;
}

int
ac4enc_encode_tail(ac4enc_ctx_t *c, const int *const *pcm,
                   int n_valid, uint8_t *out, size_t outcap,
                   int *is_iframe)
{
    int rc;
    (void)out;
    (void)outcap;
    (void)is_iframe;
    if (!c || !pcm)
        return -EINVAL;
    if (c->encoded || c->tailed)
        return -EINVAL;
    if (n_valid <= 0 || n_valid > AC4ENC_FRAME_SAMPLES)
        return -EINVAL;
    for (int i = 0; i < c->nch; i++)
        if (!pcm[i])
            return -EINVAL;
    rc = feed_rows(c, 1, n_valid);
    if (rc)
        return rc;
    for (int i = 0; i < c->nch; i++)
        memcpy(c->pi[i] + c->nsamples, pcm[i],
               (size_t)n_valid * sizeof *pcm[i]);
    c->nsamples += n_valid;
    c->tailed = 1;
    return 0;
}

int
ac4enc_encode_tail_f(ac4enc_ctx_t *c, const double *const *pcm,
                     int n_valid, uint8_t *out, size_t outcap,
                     int *is_iframe)
{
    int rc;
    (void)out;
    (void)outcap;
    (void)is_iframe;
    if (!c || !pcm)
        return -EINVAL;
    if (c->encoded || c->tailed)
        return -EINVAL;
    if (n_valid <= 0 || n_valid > AC4ENC_FRAME_SAMPLES)
        return -EINVAL;
    for (int i = 0; i < c->nch; i++)
        if (!pcm[i])
            return -EINVAL;
    rc = feed_rows(c, 2, n_valid);
    if (rc)
        return rc;
    for (int i = 0; i < c->nch; i++)
        memcpy(c->pf[i] + c->nsamples, pcm[i],
               (size_t)n_valid * sizeof *pcm[i]);
    c->nsamples += n_valid;
    c->tailed = 1;
    return 0;
}

/* ---- flush: run core once, split sync frames, serve bare ---- */

static int
expected_nframes(const ac4enc_ctx_t *c)
{
    if (c->nch == 6 || c->priming)
        return enc_core_nframes_for_samples(c->nsamples);
    return (c->nsamples + AC4ENC_FRAME_SAMPLES - 1) /
        AC4ENC_FRAME_SAMPLES;
}

static int
run_core(ac4enc_ctx_t *c)
{
    int lens[6];
    int est, attempt;
    long got = -1;
    size_t cap = 0;
    const enc_core_measure_t *ms =
        c->has_measure ? &c->ms : NULL;
    int want_domain = c->domain == 0 ? 1 : c->domain;
    uint8_t *blob = NULL;
    est = enc_core_nframes_for_samples(c->nsamples);
    if (est < 0)
        return -EIO;
    cap = (size_t)est *
            (size_t)(c->rate_kbps * 16 / 3 + 4096) +
        65536;
    for (int i = 0; i < c->nch; i++)
        lens[i] = c->nsamples;
    if (c->domain == 0) {
        /* Empty stream: dummy zero-length rows for the core. */
        if (want_domain == 1) {
            c->pi =
                calloc((size_t)c->nch, sizeof *c->pi);
            if (!c->pi)
                return -ENOMEM;
            for (int i = 0; i < c->nch; i++) {
                c->pi[i] = malloc(sizeof *c->pi[i]);
                if (!c->pi[i])
                    return -ENOMEM;
            }
        } else {
            c->pf =
                calloc((size_t)c->nch, sizeof *c->pf);
            if (!c->pf)
                return -ENOMEM;
            for (int i = 0; i < c->nch; i++) {
                c->pf[i] = malloc(sizeof *c->pf[i]);
                if (!c->pf[i])
                    return -ENOMEM;
            }
        }
        c->domain = want_domain;
    }
    for (attempt = 0; attempt < 2 && got < 0; attempt++) {
        free(blob);
        blob = malloc(cap > 0 ? cap : 1);
        if (!blob)
            return -ENOMEM;
        if (c->nch == 2 && want_domain == 1)
            got = enc_core_encode_pcm_m(
                c->pi[0], lens[0], c->pi[1], lens[1],
                c->rate_kbps, c->layout, c->interval, "zero",
                c->window, c->ns2_tone, c->shorts,
                c->target_db, c->has_bake, c->bake_db,
                c->joint_shorts, c->forced, c->nforced,
                c->preset, c->drc, c->dmx, c->priming,
                c->iframe_base, c->iframe_total, ms, blob,
                cap);
        else if (c->nch == 2)
            got = enc_core_encode_pcm_f_m(
                c->pf[0], lens[0], c->pf[1], lens[1],
                c->rate_kbps, c->layout, c->interval, "zero",
                c->window, c->ns2_tone, c->shorts,
                c->target_db, c->has_bake, c->bake_db,
                c->joint_shorts, c->forced, c->nforced,
                c->preset, c->drc, c->dmx, c->priming,
                c->iframe_base, c->iframe_total, ms, blob,
                cap);
        else if (want_domain == 1)
            got = enc_core_encode_pcm_mc_m(
                (const int *const *)c->pi, lens,
                c->rate_kbps, c->interval, "zero", c->window,
                c->target_db, c->has_bake, c->bake_db,
                c->shorts, c->forced, c->nforced, c->preset,
                c->drc, c->dmx, ms, blob, cap);
        else
            got = enc_core_encode_pcm_mc_f_m(
                (const double *const *)c->pf, lens,
                c->rate_kbps, c->interval, "zero", c->window,
                c->target_db, c->has_bake, c->bake_db,
                c->shorts, c->forced, c->nforced, c->preset,
                c->drc, c->dmx, ms, blob, cap);
        cap *= 4;
    }
    if (got < 0 || got > INT_MAX) {
        free(blob);
        return -EIO;
    }
    c->blob = blob;
    c->blob_n = (int)got;
    return 0;
}

static int
split_frames(ac4enc_ctx_t *c)
{
    int pos = 0, count = 0, want;
    int *offs = NULL, *lens = NULL;
    /* Count first. */
    while (pos < c->blob_n) {
        int len;
        if (pos + 6 > c->blob_n)
            return -EIO;
        if (c->blob[pos] != 0xAC || c->blob[pos + 1] != 0x41)
            return -EIO;
        len = (c->blob[pos + 2] << 8) | c->blob[pos + 3];
        if (pos + 4 + len + 2 > c->blob_n)
            return -EIO;
        count++;
        pos += 4 + len + 2;
    }
    want = expected_nframes(c);
    if (count != want)
        return -EIO;
    offs = malloc((size_t)(count > 0 ? count : 1) *
                  sizeof *offs);
    lens = malloc((size_t)(count > 0 ? count : 1) *
                  sizeof *lens);
    if (!offs || !lens) {
        free(offs);
        free(lens);
        return -ENOMEM;
    }
    pos = 0;
    for (int i = 0; i < count; i++) {
        int len =
            (c->blob[pos + 2] << 8) | c->blob[pos + 3];
        offs[i] = pos + 4;
        lens[i] = len;
        pos += 4 + len + 2;
    }
    c->offs = offs;
    c->lens = lens;
    c->nframes = count;
    return 0;
}

static int
compute_iframes(ac4enc_ctx_t *c)
{
    int *set = NULL, nset = 0;
    set = malloc((size_t)(c->nframes > 0 ? c->nframes : 1) *
                 sizeof *set);
    c->iframes = calloc(
        (size_t)(c->nframes > 0 ? c->nframes : 1),
        sizeof *c->iframes);
    if (!set || !c->iframes) {
        free(set);
        return -ENOMEM;
    }
    if (c->nch == 6 || c->priming)
        nset = enc_core_iframe_set(c->nframes, c->interval,
                                   c->forced, c->nforced,
                                   set);
    else
        nset = enc_core_joint_iframes(
            c->nframes, c->iframe_base,
            c->iframe_total < 0 ? c->nframes
                                : c->iframe_total,
            c->interval, c->forced, c->nforced, set);
    if (nset < 0) {
        free(set);
        return -EIO;
    }
    for (int i = 0; i < nset; i++)
        if (set[i] >= 0 && set[i] < c->nframes)
            c->iframes[set[i]] = 1;
    free(set);
    return 0;
}

int
ac4enc_flush2(ac4enc_ctx_t *c, uint8_t *out, size_t outcap,
              size_t *out_len, int *is_iframe)
{
    int rc, len;
    if (!c || !out || !out_len)
        return -EINVAL;
    if (c->enc_err)
        return c->enc_err;
    if (!c->encoded) {
        rc = run_core(c);
        if (rc) {
            c->enc_err = rc;
            return rc;
        }
        rc = split_frames(c);
        if (rc) {
            c->enc_err = rc;
            return rc;
        }
        rc = compute_iframes(c);
        if (rc) {
            c->enc_err = rc;
            return rc;
        }
        c->encoded = 1;
    }
    if (c->cursor >= c->nframes)
        return 0;
    len = c->lens[c->cursor];
    if ((size_t)len > outcap)
        return -EINVAL;
    memcpy(out, c->blob + c->offs[c->cursor], (size_t)len);
    *out_len = (size_t)len;
    if (is_iframe)
        *is_iframe = c->iframes[c->cursor];
    c->cursor++;
    return 1;
}

int
ac4enc_flush(ac4enc_ctx_t *c, uint8_t *out, size_t outcap,
             int *is_iframe)
{
    size_t len = 0;
    return ac4enc_flush2(c, out, outcap, &len, is_iframe);
}

void
ac4enc_close(ac4enc_ctx_t *c)
{
    if (!c)
        return;
    free(c->layout);
    free(c->interval);
    free(c->preset);
    free(c->window);
    free(c->forced);
    free(c->modes);
    free(c->drc);
    free(c->dmx);
    free_pcm(c);
    free(c->blob);
    free(c->offs);
    free(c->lens);
    free(c->iframes);
    free(c);
}
