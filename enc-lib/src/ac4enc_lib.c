/* ac4enc_lib.c: opaque-ctx incremental API (r2).
 * See ac4enc_lib.h for the contract notes. */
#include "ac4enc_lib.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enc_core.h"
#include "enc_step.h"
#include "loudness.h"
#include "mdct.h"
#include "pycompat.h"

/* M4: the public DRC mirror must track the core layout. */
_Static_assert(sizeof(ac4enc_drc_curve_t) ==
                   sizeof(enc_core_drc_curve_t),
               "drc curve mirror");
_Static_assert(sizeof(ac4enc_drc_mode_t) ==
                   sizeof(enc_core_drc_mode_t),
               "drc mode mirror");
_Static_assert(offsetof(ac4enc_drc_mode_t, mid) ==
                   offsetof(enc_core_drc_mode_t, mid),
               "drc mid");
_Static_assert(offsetof(ac4enc_drc_mode_t, kind) ==
                   offsetof(enc_core_drc_mode_t, kind),
               "drc kind");
_Static_assert(offsetof(ac4enc_drc_mode_t, gain) ==
                   offsetof(enc_core_drc_mode_t, gain),
               "drc gain");
_Static_assert(offsetof(ac4enc_drc_mode_t, curve) ==
                   offsetof(enc_core_drc_mode_t, curve),
               "drc curve");
_Static_assert(AC4ENC_DRC_DEFAULT == ENC_CORE_DRC_DEFAULT &&
                   AC4ENC_DRC_GAINS == ENC_CORE_DRC_GAINS &&
                   AC4ENC_DRC_CURVE == ENC_CORE_DRC_CURVE,
               "drc kinds");

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
    enc_core_drc_t drcbuf;
    enc_core_drc_mode_t *modes;
    int has_drc;
    int *dmx;
    int sample_rate;
    int has_measure;
    enc_core_measure_t ms;
    int has_spec_lines;
    int n_spec_lines;
    int spec_lines[7];
    /* Prologue products (mirror the _m entries exactly). */
    double b0[6];
    double b1[6];
    int bake_two;
    int do_bake;
    int do_peak;
    int have_stamp;
    enc_core_stamp_t stamp_i;
    enc_core_stamp_t stamp_f;
    uint8_t si_p6[6], si_i12[15], si_s4[4], si_fr0[15];
    uint8_t sf_p6[6], sf_i12[15], sf_s4[4], sf_fr0[15];
    int flat_file;
    int stamp_set;
    /* Stream state. */
    enc_step_ctx_t *eng;
    int domain; /* 0 none, 1 int, 2 float */
    int64_t fed_total; /* m2: 64-bit sample total (per ch) */
    int input_ended;
    int enc_err;
    ac4enc_subcode_t err_sub;
    const char *err_stage;
    int err_frame;
    /* R3 loud refusal: gate + offending values for the bake
     * subcodes, else "". Follows err_sub (record clears). */
    char err_detail[256];
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

/* ---- diagnostics (M1) ---- */

const char *
ac4enc_strerror(int rc)
{
    switch (rc) {
    case 0:
        return "ok";
    case 1:
        return "frame ready";
    case -EINVAL:
        return "invalid argument or option";
    case -ENOMEM:
        return "out of memory";
    case -EIO:
        return "encode failure";
    default:
        return "unknown";
    }
}

const char *
ac4enc_subcode_str(ac4enc_subcode_t sub)
{
    switch (sub) {
    case AC4ENC_EOK:
        return "ok";
    case AC4ENC_EBADCALL:
        return "bad call";
    case AC4ENC_EBADOPT:
        return "bad option";
    case AC4ENC_ENOMEM:
        return "out of memory";
    case AC4ENC_EBAKE_OVERFLOW:
        return "bake overflow";
    case AC4ENC_EBAKE_PEAK:
        return "baked peak overflow";
    case AC4ENC_EENCODE:
        return "encode refusal";
    case AC4ENC_ESHORTBUF:
        return "short buffer";
    case AC4ENC_EFPENV:
        return "unsafe FP environment";
    default:
        return "unknown";
    }
}

ac4enc_subcode_t
ac4enc_err_sub(const ac4enc_ctx_t *c)
{
    return c ? c->err_sub : AC4ENC_EOK;
}

const char *
ac4enc_err_stage(const ac4enc_ctx_t *c)
{
    return c ? c->err_stage : "-";
}

int
ac4enc_err_frame(const ac4enc_ctx_t *c)
{
    return c ? c->err_frame : -1;
}

const char *
ac4enc_err_detail(const ac4enc_ctx_t *c)
{
    return c ? c->err_detail : "";
}

int64_t
ac4enc_fed_samples(const ac4enc_ctx_t *c)
{
    return c ? c->fed_total : -1;
}

int64_t
ac4enc_emitted_frames(const ac4enc_ctx_t *c)
{
    if (!c)
        return -1;
    return (int64_t)enc_step_emitted(c->eng);
}

static int
record(ac4enc_ctx_t *c, ac4enc_subcode_t sub,
       const char *stage, int frame, int rc)
{
    c->err_sub = sub;
    c->err_stage = stage;
    c->err_frame = frame;
    c->err_detail[0] = '\0';
    /* Protocol + short-buffer errors are retryable (the ctx
     * survives); encode/memory failures are sticky. */
    if (sub != AC4ENC_ESHORTBUF && sub != AC4ENC_EBADCALL &&
        !c->enc_err)
        c->enc_err = rc;
    return rc;
}

/* Net per-ch bake factor (stereo: b0; mc: (b0*b1)). */
static double
net_bake(const ac4enc_ctx_t *c, int ch)
{
    double b = c->b0[ch];
    if (c->bake_two)
        b *= c->b1[ch];
    return b;
}

/* Map an engine return onto the lib error model (sticky). */
static int
from_engine(ac4enc_ctx_t *c, const char *stage, int rc)
{
    int detail = enc_step_detail(c->eng);
    int frame = enc_step_emitted(c->eng);
    if (rc == 0 || rc == 1)
        return rc;
    if (rc == -ENOMEM)
        return record(c, AC4ENC_ENOMEM, stage, frame, rc);
    if (detail == ENC_STEP_EBAKE) {
        int ech = 0, eidx = 0;
        double x = 0.0, nb;
        long long fr = c->fed_total / AC4ENC_FRAME_SAMPLES;
        record(c, AC4ENC_EBAKE_OVERFLOW, stage,
               fr > INT_MAX ? INT_MAX : (int)fr, rc);
        if (enc_step_err_sample(c->eng, &ech, &eidx, &x))
            ech = eidx = 0;
        if (ech < 0 || ech >= c->nch)
            ech = 0;
        nb = net_bake(c, ech);
        snprintf(c->err_detail, sizeof c->err_detail,
                 "bake_round gate: int32 overflow at ch=%d "
                 "idx=%d (x=%.17g, bake x%.17g)",
                 ech, eidx, x, nb);
        return rc;
    }
    if (detail == ENC_STEP_EPEAK) {
        int pk = enc_step_err_peak(c->eng);
        double nb = 0.0;
        record(c, AC4ENC_EBAKE_PEAK, stage, -1, rc);
        for (int ch = 0; ch < c->nch; ch++) {
            double b = net_bake(c, ch);
            if (fabs(b) > fabs(nb))
                nb = b;
        }
        snprintf(c->err_detail, sizeof c->err_detail,
                 "baked peak gate: baked_peak=%d >= 32767 "
                 "(max-|bake| x%.17g)", pk, nb);
        return rc;
    }
    if (detail == ENC_STEP_ESHORT)
        return record(c, AC4ENC_ESHORTBUF, stage, frame,
                      rc);
    if (detail == ENC_STEP_EBCFG)
        return record(c, AC4ENC_EBADOPT, stage, frame, rc);
    if (detail == ENC_STEP_EPROTO)
        return record(c, AC4ENC_EBADCALL, stage, frame, rc);
    return record(c, AC4ENC_EENCODE, stage, frame, rc);
}

/* ---- FP environment self-check (M5) ----
 * Probe: a*b+c with a=b=1+2^-52, c=-(1+2^-51). Unfused:
 * fl(a*b)=1+2^-51 (the 2^-104 term rounds away), minus c is
 * exactly 0. Fused: fl(2^-104)=2^-104. Nonzero observes
 * contraction (verified: 0 under -ffp-contract=off at -O0/-O2,
 * 2^-104 under =fast). Volatile defeats const-folding; the
 * expression fuses iff the TU allows contraction. */
static int
fp_contract_active(void)
{
    volatile double a = 0x1.0000000000001p0;
    volatile double b = 0x1.0000000000001p0;
    volatile double c = -0x1.0000000000002p0;
    return (a * b + c) != 0.0;
}

int
ac4enc_fp_ok(void)
{
    return fp_contract_active() ? 0 : 1;
}

int
ac4enc_max_packet(int rate_kbps)
{
    int m = enc_step_max_cbr(rate_kbps);
    return m < 0 ? -EINVAL : m;
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
            if (enc_core_abs_sat(chs[0][k]) > peak)
                peak = enc_core_abs_sat(chs[0][k]);
        for (int k = 0; k < lens[1]; k++)
            if (enc_core_abs_sat(chs[1][k]) > peak)
                peak = enc_core_abs_sat(chs[1][k]);
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
                if (enc_core_abs_sat(chs[c][k]) > peak)
                    peak = enc_core_abs_sat(chs[c][k]);
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

/* ---- bake gain + stamp (mirror the _m prologues exactly) ---- */

static double
stereo_gain(double m, double f, double tf, double target_db,
            int has_bake, double bake_db)
{
    double gain;
    if (has_bake)
        gain = bake_db;
    else if (isinf(m) && m < 0)
        gain = 0.0;
    else
        gain = loud_bake_gain(m, f, tf, target_db);
    return gain;
}

static double
mc_gain(double m, double f, double tf, double target_db,
        int has_bake, double bake_db, int *hits51)
{
    double gain;
    int hits = tf < 0.6 && f > 0.5;
    if (hits51)
        *hits51 = hits;
    if (has_bake)
        gain = bake_db;
    else if (isinf(m) && m < 0)
        gain = 0.0;
    else if (hits)
        gain = ENC_CORE_T_HIT_51_DB -
            (m > -23.0 ? m : -23.0) + (target_db + 24.0);
    else
        gain = target_db - m;
    return gain;
}

/* bake_round twin (core uses a file-static; same op order). */
static int
lib_bake_round(double v, int *out)
{
    long long ll;
    if (pycompat_round_half_even_ll(v, &ll))
        return -1;
    if (ll < -2147483647L - 1 || ll > 2147483647L)
        return -1;
    *out = (int)ll;
    return 0;
}

/* Compose one stereo stamp (peak selects int/float twin). */
static int
stereo_stamp(double m, int in_peak, double d_peak,
             int has_d_peak, int is_f, double f, double tf,
             double target_db, const char *preset,
             uint8_t *p6, uint8_t *i12, uint8_t *s4,
             uint8_t *fr0, enc_core_stamp_t *stamp)
{
    double fpeak =
        has_d_peak ? d_peak : (double)in_peak;
    int sparse, c16, t16, pid, np6, ni12, nfr0;
    int sby = pycompat_round_half_even(-4.0 * target_db);
    sparse = tf < 0.6 && f > 0.5;
    c16 = is_f ? enc_core_content16_f(m, fpeak, f, target_db)
               : enc_core_content16(m, in_peak, f,
                                    target_db);
    t16 = enc_core_tail16(sparse, tf);
    pid = enc_core_preset_id(preset ? preset : "manual");
    if (pid < 0)
        return -1;
    np6 = enc_core_compose_sub0(0, sparse, sby, 0, NULL,
                               pid, p6);
    ni12 = enc_core_compose_sub0(
        1, 0, sby, t16, ENC_CORE_I12_MID_TAIL, pid, i12);
    nfr0 = enc_core_compose_sub0(1, sparse, sby, c16, NULL,
                                pid, fr0);
    /* S1: the sparse-iframe fr0 stamp is legitimately 11
     * bytes (enc.py sparse branch); accept the actual length
     * and store it (downstream splice is length-agnostic). */
    if (np6 != 6 || ni12 != 12 ||
        nfr0 != (sparse ? 11 : 12))
        return -1;
    memcpy(s4, ENC_CORE_SUB2_DEE, 4);
    stamp->p6 = p6;
    stamp->n_p6 = 6;
    stamp->i12 = i12;
    stamp->n_i12 = 12;
    stamp->s4 = s4;
    stamp->n_s4 = 4;
    stamp->fr0 = fr0;
    stamp->n_fr0 = nfr0;
    return 0;
}

/* Compose one 5.1 stamp (peak selects int/float twin). */
static int
mc_stamp(double m, int in_peak, double d_peak,
         int has_d_peak, int is_f, double f,
         double target_db, const char *preset,
         const int *dmx, uint8_t *p6, uint8_t *i15,
         uint8_t *s4, uint8_t *fr0,
         enc_core_stamp_t *stamp)
{
    double fpeak =
        has_d_peak ? d_peak : (double)in_peak;
    int peak = in_peak;
    int sby, c16, pid, nfr0, ni15, np6;
    sby = pycompat_round_half_even(-4.0 * target_db);
    /* NOTE: the int _mc_m entry passes ms->in_peak to
     * content16; the float twin passes the selected peak to
     * content16_f. Mirrored per is_f. */
    c16 = is_f ? enc_core_content16_f(m, fpeak, f, target_db)
               : enc_core_content16(m, peak, f, target_db);
    pid = enc_core_preset_id(preset ? preset : "manual");
    if (pid < 0)
        return -1;
    nfr0 = enc_core_compose_sub0_51(sby, c16, NULL, pid,
                                    dmx, fr0);
    ni15 = enc_core_compose_sub0_51(
        sby, ENC_CORE_TAIL16_DENSE, ENC_CORE_I12_MID_TAIL,
        pid, dmx, i15);
    np6 = enc_core_compose_sub0(0, 0, sby, 0, NULL, pid,
                               p6);
    if (nfr0 != 15 || ni15 != 15 || np6 != 6)
        return -1;
    memcpy(s4, ENC_CORE_SUB2_DEE, 4);
    stamp->p6 = p6;
    stamp->n_p6 = 6;
    stamp->i12 = i15;
    stamp->n_i12 = 15;
    stamp->s4 = s4;
    stamp->n_s4 = 4;
    stamp->fr0 = fr0;
    stamp->n_fr0 = 15;
    return 0;
}

/* H6 pass-1: spec-lines estimate over the (possibly baked) L
 * row. Stereo only (the mc core path takes no lines). */
int
ac4enc_spectral_lines(const double *L, int nL,
                      const ac4enc_measure_t *m,
                      double target_db, int has_bake,
                      double bake_db, int *nlines,
                      int lines[7])
{
    double freq;
    if (!nlines || !lines || !m)
        return -EINVAL;
    if (!L || nL < 0)
        return -EINVAL;
    if (!(target_db == 0.0 ||
          (-31.0 <= target_db && target_db <= -10.0)))
        return -EINVAL;
    if (has_bake != 0 && has_bake != 1)
        return -EINVAL;
    if (has_bake && isnan(bake_db))
        return -EINVAL;
    if (isnan(m->m_db) || isnan(m->f_flat) ||
        isnan(m->tf_tone))
        return -EINVAL;
    if (target_db != 0.0) {
        double gain = stereo_gain(m->m_db, m->f_flat,
                                  m->tf_tone, target_db,
                                  has_bake, bake_db);
        double bake = pow(10.0, gain / 20.0);
        double *baked = malloc(
            (size_t)(nL > 0 ? nL : 1) * sizeof *baked);
        if (!baked)
            return -ENOMEM;
        for (int k = 0; k < nL; k++) {
            int bi;
            if (lib_bake_round(L[k] * bake, &bi)) {
                free(baked);
                return -EIO;
            }
            baked[k] = (double)bi;
        }
        freq = mdct_estimate_freq(baked, nL, 50.0,
                                  20000.0);
        free(baked);
    } else {
        freq = mdct_estimate_freq(L, nL, 50.0, 20000.0);
    }
    *nlines = mdct_lines_for_freq(freq, 3, lines);
    if (*nlines < 0) {
        *nlines = 0;
        return -EIO;
    }
    return 0;
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

/* Fill prologue products; validates stamp composition
 * (preset/dmx/peak ranges) eagerly at open. */
static int
prologue(ac4enc_ctx_t *c, const ac4enc_opts_t *opts)
{
    int baked;
    c->do_bake = 0;
    c->do_peak = 0;
    c->bake_two = 0;
    c->have_stamp = 0;
    c->flat_file = 0;
    for (int k = 0; k < 6; k++) {
        c->b0[k] = 1.0;
        c->b1[k] = 1.0;
    }
    if (!opts->has_measure)
        return 0;
    c->flat_file =
        opts->f_flat > ENC_CORE_FLAT_F_THRESH ? 1 : 0;
    baked = opts->target_db != 0.0;
    if (!baked)
        return 0;
    c->do_bake = 1;
    c->do_peak = 1;
    if (c->nch == 2) {
        double gain = stereo_gain(opts->m_db, opts->f_flat,
                                  opts->tf_tone,
                                  opts->target_db,
                                  opts->has_bake,
                                  opts->bake_db);
        double bake = pow(10.0, gain / 20.0);
        c->b0[0] = bake;
        c->b0[1] = bake;
        if (stereo_stamp(opts->m_db, opts->in_peak,
                         opts->d_peak, opts->has_d_peak, 0,
                         opts->f_flat, opts->tf_tone,
                         opts->target_db, opts->preset,
                         c->si_p6, c->si_i12, c->si_s4,
                         c->si_fr0, &c->stamp_i))
            return -1;
        if (stereo_stamp(opts->m_db, opts->in_peak,
                         opts->d_peak, opts->has_d_peak, 1,
                         opts->f_flat, opts->tf_tone,
                         opts->target_db, opts->preset,
                         c->sf_p6, c->sf_i12, c->sf_s4,
                         c->sf_fr0, &c->stamp_f))
            return -1;
    } else {
        double gain;
        double bake, lfe_bake;
        int hits51 = 0;
        gain = mc_gain(opts->m_db, opts->f_flat,
                       opts->tf_tone, opts->target_db,
                       opts->has_bake, opts->bake_db,
                       &hits51);
        bake = pow(10.0, gain / 20.0);
        lfe_bake = hits51
            ? pow(10.0, ENC_CORE_LFE_HITS_BAKE_DB / 20.0)
            : bake;
        for (int k = 0; k < 6; k++) {
            double bc = k == 3 ? lfe_bake : bake;
            c->b0[k] = bc;
            c->b1[k] = pow(
                10.0, ENC_CORE_LEG_COMP_51_DB[k] / 20.0);
        }
        c->bake_two = 1;
        if (mc_stamp(opts->m_db, opts->in_peak,
                     opts->d_peak, opts->has_d_peak, 0,
                     opts->f_flat, opts->target_db,
                     opts->preset, opts->dmx, c->si_p6,
                     c->si_i12, c->si_s4, c->si_fr0,
                     &c->stamp_i))
            return -1;
        if (mc_stamp(opts->m_db, opts->in_peak,
                     opts->d_peak, opts->has_d_peak, 1,
                     opts->f_flat, opts->target_db,
                     opts->preset, opts->dmx, c->sf_p6,
                     c->sf_i12, c->sf_s4, c->sf_fr0,
                     &c->stamp_f))
            return -1;
    }
    c->have_stamp = 1;
    return 0;
}

static int
open_engine(ac4enc_ctx_t *c)
{
    enc_step_cfg_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.nch = c->nch;
    cfg.rate_kbps = c->rate_kbps;
    cfg.layout = c->layout;
    cfg.interval = c->interval;
    cfg.window = c->window;
    cfg.ns2_tone = c->ns2_tone;
    cfg.shorts = c->shorts;
    cfg.joint_shorts = c->joint_shorts;
    cfg.forced = c->forced;
    cfg.nforced = c->nforced;
    cfg.drc = c->has_drc ? &c->drcbuf : NULL;
    cfg.dmx = c->dmx;
    cfg.priming = c->priming;
    cfg.iframe_base = c->iframe_base;
    cfg.iframe_total = c->iframe_total;
    memcpy(cfg.bake0, c->b0, sizeof cfg.bake0);
    memcpy(cfg.bake1, c->b1, sizeof cfg.bake1);
    cfg.bake_two = c->bake_two;
    cfg.do_bake = c->do_bake;
    cfg.do_peak = c->do_peak;
    cfg.stamp = NULL; /* per-domain, set at first feed */
    cfg.flat_file = c->flat_file;
    if (c->nch == 2 && c->has_spec_lines) {
        cfg.nlines = c->n_spec_lines;
        memcpy(cfg.lines, c->spec_lines,
               sizeof cfg.lines);
    }
    return enc_step_open(&c->eng, &cfg);
}

int
ac4enc_open(ac4enc_ctx_t **ctx, const ac4enc_opts_t *opts)
{
    ac4enc_ctx_t *c;
    int nch, floor, rc;
    if (!ctx || !opts || !opts->layout)
        return -EINVAL;
    if (!ac4enc_fp_ok())
        return -EIO;
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
        !is01(opts->joint_shorts) || !is01(opts->priming) ||
        !is01(opts->has_bake) || !is01(opts->has_measure) ||
        !is01(opts->has_d_peak) ||
        !is01(opts->has_spec_lines))
        return -EINVAL;
    if (nch == 6 && (opts->joint_shorts || !opts->priming))
        return -EINVAL;
    if (opts->has_bake && isnan(opts->bake_db))
        return -EINVAL;
    if (opts->has_bake && !opts->has_measure)
        return -EINVAL;
    if (opts->nforced < 0 ||
        (opts->nforced > 0 && !opts->forced))
        return -EINVAL;
    if (opts->preset &&
        enc_core_preset_id(opts->preset) < 0)
        return -EINVAL;
    if (opts->drc && (opts->drc->nmodes < 1 ||
                      !opts->drc->modes))
        return -EINVAL;
    if (opts->has_measure) {
        if (isnan(opts->m_db) || isnan(opts->f_flat) ||
            isnan(opts->tf_tone) || opts->in_peak < 0)
            return -EINVAL;
        if (opts->has_d_peak &&
            (isnan(opts->d_peak) || opts->d_peak < 0.0))
            return -EINVAL;
    }
    if (opts->sample_rate != 0 && opts->sample_rate != 48000)
        return -EINVAL;
    if (opts->has_spec_lines) {
        if (opts->n_spec_lines < 0 ||
            opts->n_spec_lines > 7)
            return -EINVAL;
        for (int k = 0; k < opts->n_spec_lines; k++)
            if (opts->spec_lines[k] < 0 ||
                opts->spec_lines[k] >= 2048)
                return -EINVAL;
    }
    c = calloc(1, sizeof *c);
    if (!c)
        return -ENOMEM;
    c->err_sub = AC4ENC_EOK;
    c->err_stage = "-";
    c->err_frame = -1;
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
        /* Convert the public mirror (same layout, copied
         * field-wise: no aliasing). */
        c->modes = malloc((size_t)opts->drc->nmodes *
                          sizeof *c->modes);
        if (!c->modes)
            goto oom;
        for (int k = 0; k < opts->drc->nmodes; k++) {
            c->modes[k].mid = opts->drc->modes[k].mid;
            c->modes[k].kind = opts->drc->modes[k].kind;
            c->modes[k].gain = opts->drc->modes[k].gain;
            memcpy(&c->modes[k].curve,
                   &opts->drc->modes[k].curve,
                   sizeof c->modes[k].curve);
        }
        c->drcbuf.eac3 = opts->drc->eac3;
        c->drcbuf.modes = c->modes;
        c->drcbuf.nmodes = opts->drc->nmodes;
        c->has_drc = 1;
    }
    if (opts->dmx) {
        c->dmx = malloc(5 * sizeof *c->dmx);
        if (!c->dmx)
            goto oom;
        memcpy(c->dmx, opts->dmx, 5 * sizeof *c->dmx);
    }
    c->sample_rate =
        opts->sample_rate == 0 ? 48000 : opts->sample_rate;
    c->has_measure = opts->has_measure;
    if (opts->has_measure) {
        c->ms.m_db = opts->m_db;
        c->ms.f_flat = opts->f_flat;
        c->ms.tf_tone = opts->tf_tone;
        c->ms.in_peak = opts->in_peak;
        c->ms.d_peak = opts->d_peak;
        c->ms.has_d_peak = opts->has_d_peak;
    }
    c->has_spec_lines = opts->has_spec_lines;
    c->n_spec_lines = opts->n_spec_lines;
    memcpy(c->spec_lines, opts->spec_lines,
           sizeof c->spec_lines);
    if (prologue(c, opts)) {
        /* Stamp-shape refusal (e.g. uncomposable loudness
         * stamp): the config is valid but the core refuses
         * the encode, so -EIO (fail-fast at open instead of
         * late at flush). */
        ac4enc_close(c);
        return -EIO;
    }
    rc = open_engine(c);
    if (rc) {
        /* Engine open validates the interval rule + DRC
         * contents: config errors, -EINVAL by shape. */
        ac4enc_close(c);
        return rc == -ENOMEM ? -ENOMEM : -EINVAL;
    }
    *ctx = c;
    return 0;
oom:
    ac4enc_close(c);
    return -ENOMEM;
}

void
ac4enc_close(ac4enc_ctx_t *c)
{
    if (!c)
        return;
    enc_step_close(c->eng);
    free(c->layout);
    free(c->interval);
    free(c->preset);
    free(c->window);
    free(c->forced);
    free(c->modes);
    free(c->dmx);
    free(c);
}

int
ac4enc_reset(ac4enc_ctx_t *c)
{
    int rc;
    if (!c)
        return -EINVAL;
    enc_step_close(c->eng);
    c->eng = NULL;
    c->domain = 0;
    c->fed_total = 0;
    c->input_ended = 0;
    c->enc_err = 0;
    c->err_sub = AC4ENC_EOK;
    c->err_stage = "-";
    c->err_frame = -1;
    c->err_detail[0] = '\0';
    c->stamp_set = 0;
    rc = open_engine(c);
    if (rc) {
        /* Degraded: same opts failed reopen (OOM). The ctx
         * stays closed-able but unusable; record it. */
        c->enc_err = rc;
        c->err_sub = rc == -ENOMEM ? AC4ENC_ENOMEM
                                   : AC4ENC_EBADOPT;
        c->err_stage = "reset";
        return rc;
    }
    return 0;
}

/* ---- feed / emit / flush ---- */

/* Per-domain stamp select (int default, float twin on float
 * input). No-feed streams keep the int default (v1 parity). */
static int
ensure_stamp(ac4enc_ctx_t *c, const char *stage)
{
    const enc_core_stamp_t *st;
    int rc;
    if (c->stamp_set || !c->have_stamp)
        return 0;
    st = c->domain == 2 ? &c->stamp_f : &c->stamp_i;
    rc = enc_step_set_stamp(c->eng, st);
    if (rc)
        return from_engine(c, stage, rc);
    c->stamp_set = 1;
    return 0;
}

/* Serve one ready frame (out_len required). Not ready -> 0.
 * Short buffer -> *out_len = needed, -EINVAL, nothing lost
 * (the frame stays pending for a roomy retry). */
static int
serve(ac4enc_ctx_t *c, const char *stage, uint8_t *out,
      size_t outcap, size_t *out_len, int *is_iframe)
{
    size_t olen = 0;
    int is_if = 0, rc, need;
    *out_len = 0;
    if (enc_step_ready(c->eng) != 1)
        return 0;
    need = enc_step_next_size(c->eng);
    if (need < 0)
        return record(c, AC4ENC_EENCODE, stage,
                      enc_step_emitted(c->eng), -EIO);
    if (!out || (size_t)need > outcap) {
        *out_len = (size_t)need;
        return record(c, AC4ENC_ESHORTBUF, stage,
                      enc_step_emitted(c->eng), -EINVAL);
    }
    rc = enc_step_emit(c->eng, out, outcap, &olen, &is_if);
    if (rc != 1)
        return from_engine(c, stage, rc);
    *out_len = olen;
    if (is_iframe)
        *is_iframe = is_if;
    return 1;
}

/* Shared feed core. Pre-checks the emit buffer BEFORE feeding
 * (a short buffer consumes no input); serves at most one frame
 * per call, so undrained frames never accumulate past one. */
static int
feed2(ac4enc_ctx_t *c, const char *stage, int domain,
      const int *const *pi, const double *const *pf, int n,
      int is_tail, uint8_t *out, size_t outcap,
      size_t *out_len, int *is_iframe)
{
    int ns[6], rc, will_emit;
    if (!c || !out_len)
        return -EINVAL;
    *out_len = 0;
    if (c->enc_err)
        return c->enc_err;
    if (!c->eng)
        return record(c, AC4ENC_ENOMEM, stage, -1,
                      -ENOMEM);
    if (c->input_ended)
        return record(c, AC4ENC_EBADCALL, stage, -1,
                      -EINVAL);
    if (n <= 0 || n > AC4ENC_FRAME_SAMPLES)
        return record(c, AC4ENC_EBADCALL, stage, -1,
                      -EINVAL);
    if ((!pi && !pf) || (pi && pf))
        return record(c, AC4ENC_EBADCALL, stage, -1,
                      -EINVAL);
    for (int ch = 0; ch < c->nch; ch++) {
        if ((domain == 1 && !pi[ch]) ||
            (domain == 2 && !pf[ch]))
            return record(c, AC4ENC_EBADCALL, stage, -1,
                          -EINVAL);
        ns[ch] = n;
    }
    if (c->domain == 0) {
        c->domain = domain;
    } else if (c->domain != domain) {
        return record(c, AC4ENC_EBADCALL, stage, -1,
                      -EINVAL);
    }
    if (is_tail) {
        /* Tail ends input: frame #emitted is forthcoming
         * iff it precedes the now-known plan. Totals are
         * 64-bit (m2); the int plan saturates (only the
         * < comparison below consumes it). */
        long long total = c->fed_total + n;
        int plan;
        if (c->nch == 6 || c->priming) {
            int tin = total > INT_MAX ? INT_MAX : (int)total;
            plan = enc_core_nframes_for_samples(tin);
        } else if (total <= 0) {
            plan = 0;
        } else {
            long long p = (total + AC4ENC_FRAME_SAMPLES - 1) /
                AC4ENC_FRAME_SAMPLES;
            plan = p > INT_MAX ? INT_MAX : (int)p;
        }
        will_emit = enc_step_emitted(c->eng) < plan;
    } else {
        will_emit = enc_step_ready(c->eng) == 1 ||
            enc_step_ready_after(c->eng, n);
    }
    if (will_emit) {
        int need = enc_step_next_size(c->eng);
        if (need < 0)
            return record(c, AC4ENC_EENCODE, stage, -1,
                          -EIO);
        if (!out || (size_t)need > outcap) {
            *out_len = (size_t)need;
            return record(c, AC4ENC_ESHORTBUF, stage,
                          enc_step_emitted(c->eng),
                          -EINVAL);
        }
    }
    rc = domain == 1 ? enc_step_feed_int(c->eng, pi, ns)
                     : enc_step_feed(c->eng, pf, ns);
    if (rc)
        return from_engine(c, stage, rc);
    c->fed_total += n;
    rc = ensure_stamp(c, stage);
    if (rc)
        return rc;
    if (is_tail) {
        c->input_ended = 1;
        rc = enc_step_end(c->eng);
        if (rc)
            return from_engine(c, stage, rc);
    }
    return serve(c, stage, out, outcap, out_len,
                 is_iframe);
}

int
ac4enc_encode_frame2(ac4enc_ctx_t *c,
                     const int *const *pcm, uint8_t *out,
                     size_t outcap, size_t *out_len,
                     int *is_iframe)
{
    if (!c || !pcm)
        return -EINVAL;
    return feed2(c, "feed", 1, pcm, NULL,
                 AC4ENC_FRAME_SAMPLES, 0, out, outcap,
                 out_len, is_iframe);
}

int
ac4enc_encode_frame_f2(ac4enc_ctx_t *c,
                       const double *const *pcm,
                       uint8_t *out, size_t outcap,
                       size_t *out_len, int *is_iframe)
{
    if (!c || !pcm)
        return -EINVAL;
    return feed2(c, "feed", 2, NULL, pcm,
                 AC4ENC_FRAME_SAMPLES, 0, out, outcap,
                 out_len, is_iframe);
}

int
ac4enc_encode_frame(ac4enc_ctx_t *c,
                    const int *const *pcm, uint8_t *out,
                    size_t outcap, int *is_iframe)
{
    size_t olen = 0;
    if (!c || !pcm)
        return -EINVAL;
    return feed2(c, "feed", 1, pcm, NULL,
                 AC4ENC_FRAME_SAMPLES, 0, out, outcap,
                 &olen, is_iframe);
}

int
ac4enc_encode_frame_f(ac4enc_ctx_t *c,
                      const double *const *pcm, uint8_t *out,
                      size_t outcap, int *is_iframe)
{
    size_t olen = 0;
    if (!c || !pcm)
        return -EINVAL;
    return feed2(c, "feed", 2, NULL, pcm,
                 AC4ENC_FRAME_SAMPLES, 0, out, outcap,
                 &olen, is_iframe);
}

int
ac4enc_encode_tail2(ac4enc_ctx_t *c,
                    const int *const *pcm, int n_valid,
                    uint8_t *out, size_t outcap,
                    size_t *out_len, int *is_iframe)
{
    if (!c || !pcm)
        return -EINVAL;
    return feed2(c, "tail", 1, pcm, NULL, n_valid, 1, out,
                 outcap, out_len, is_iframe);
}

int
ac4enc_encode_tail_f2(ac4enc_ctx_t *c,
                      const double *const *pcm, int n_valid,
                      uint8_t *out, size_t outcap,
                      size_t *out_len, int *is_iframe)
{
    if (!c || !pcm)
        return -EINVAL;
    return feed2(c, "tail", 2, NULL, pcm, n_valid, 1, out,
                 outcap, out_len, is_iframe);
}

int
ac4enc_encode_tail(ac4enc_ctx_t *c,
                   const int *const *pcm, int n_valid,
                   uint8_t *out, size_t outcap,
                   int *is_iframe)
{
    size_t olen = 0;
    if (!c || !pcm)
        return -EINVAL;
    return feed2(c, "tail", 1, pcm, NULL, n_valid, 1, out,
                 outcap, &olen, is_iframe);
}

int
ac4enc_encode_tail_f(ac4enc_ctx_t *c,
                     const double *const *pcm, int n_valid,
                     uint8_t *out, size_t outcap,
                     int *is_iframe)
{
    size_t olen = 0;
    if (!c || !pcm)
        return -EINVAL;
    return feed2(c, "tail", 2, NULL, pcm, n_valid, 1, out,
                 outcap, &olen, is_iframe);
}

int
ac4enc_flush2(ac4enc_ctx_t *c, uint8_t *out, size_t outcap,
              size_t *out_len, int *is_iframe)
{
    int rc;
    if (!c || !out || !out_len)
        return -EINVAL;
    *out_len = 0;
    if (c->enc_err)
        return c->enc_err;
    if (!c->eng)
        return record(c, AC4ENC_ENOMEM, "flush", -1,
                      -ENOMEM);
    if (!c->input_ended) {
        c->input_ended = 1;
        rc = ensure_stamp(c, "flush");
        if (rc)
            return rc;
        rc = enc_step_end(c->eng);
        if (rc)
            return from_engine(c, "flush", rc);
    }
    return serve(c, "flush", out, outcap, out_len,
                 is_iframe);
}

const char *
ac4enc_last_error(void)
{
    return enc_core_last_error();
}
