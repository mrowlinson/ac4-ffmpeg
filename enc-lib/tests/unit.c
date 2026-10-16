/* unit.c: API-misuse negatives + _m-vs-original equivalence.
 * Exit 0 all pass; prints FAIL lines otherwise. Built to scratch. */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ac4enc_lib.h"
#include "../src/enc_core.h"
#include "../src/mdct.h"

#include <pthread.h>

static int fails = 0;

#define CHECK(cond, msg) do { \
        if (!(cond)) { \
            printf("FAIL: %s\n", msg); \
            fails++; \
        } \
    } while (0)

static void
t_misuse(void)
{
    ac4enc_ctx_t *c = NULL;
    ac4enc_opts_t o;
    int pcm[2048];
    const int *rows[2] = { pcm, pcm };
    double dpcm[2048];
    const double *drows[2] = { dpcm, dpcm };
    uint8_t out[128];
    size_t olen = 0;
    memset(&o, 0, sizeof o);
    memset(pcm, 0, sizeof pcm);
    memset(dpcm, 0, sizeof dpcm);
    /* open negatives */
    o.layout = "mono";
    o.rate_kbps = 64;
    CHECK(ac4enc_open(&c, &o) == -EINVAL, "bad layout");
    o.layout = "stereo";
    o.rate_kbps = 100;
    CHECK(ac4enc_open(&c, &o) == -EINVAL, "bad rate step");
    o.rate_kbps = 64;
    o.layout = "5.1";
    CHECK(ac4enc_open(&c, &o) == -EINVAL, "rate below floor");
    o.layout = "stereo";
    o.preset = "bogus";
    CHECK(ac4enc_open(&c, &o) == -EINVAL, "bad preset");
    o.preset = NULL;
    o.target_db = -5.0;
    CHECK(ac4enc_open(&c, &o) == -EINVAL, "bad target");
    o.target_db = -24.0;
    o.shorts = 2;
    CHECK(ac4enc_open(&c, &o) == -EINVAL, "bad shorts");
    o.shorts = 1;
    o.has_measure = 1;
    o.m_db = NAN;
    CHECK(ac4enc_open(&c, &o) == -EINVAL, "NaN measure");
    o.m_db = -20.0;
    o.f_flat = 0.1;
    o.tf_tone = 0.9;
    o.in_peak = 100;
    o.ns2_tone = 1;
    o.priming = 1;
    o.iframe_total = -1;
    CHECK(ac4enc_open(&c, &o) == 0, "good open");
    /* feed: incremental (real buffers; 1 = frame ready) */
    {
        static uint8_t ob[AC4ENC_OUTCAP_SAFE];
        size_t flen = 0;
        int is_if = -1, frc, n = 0;
        frc = ac4enc_encode_frame2(c, rows, ob, sizeof ob,
                                   &flen, &is_if);
        CHECK(frc == 1 && flen > 0, "frame emits priming");
        n++;
        CHECK(ac4enc_encode_frame_f(c, drows, NULL, 0,
                                    NULL) == -EINVAL,
              "mixed domains rejected");
        CHECK(ac4enc_encode_tail(c, rows, 0, NULL, 0,
                                 NULL) == -EINVAL,
              "tail 0 rejected");
        CHECK(ac4enc_encode_tail(c, rows, 2049, NULL, 0,
                                 NULL) == -EINVAL,
              "tail >2048 rejected");
        frc = ac4enc_encode_tail2(c, rows, 100, ob,
                                  sizeof ob, &flen, &is_if);
        CHECK(frc == 1 && flen > 0, "tail emits");
        n++;
        CHECK(ac4enc_encode_frame(c, rows, NULL, 0, NULL) ==
                  -EINVAL,
              "frame after tail rejected");
        olen = 0;
        CHECK(ac4enc_flush2(c, out, sizeof out, &olen,
                            NULL) == -EINVAL,
              "small outcap rejected");
        CHECK(olen > sizeof out, "short reports need");
        for (;;) {
            frc = ac4enc_flush2(c, ob, sizeof ob, &flen,
                                NULL);
            if (frc <= 0)
                break;
            n++;
            CHECK(flen > 0, "frame len > 0");
        }
        CHECK(frc == 0 && n == 4, "4 frames then done");
        CHECK(ac4enc_fed_samples(c) == 2048 + 100,
              "m2 fed total");
        CHECK(ac4enc_emitted_frames(c) == n,
              "m2 emitted total");
    }
    CHECK(ac4enc_encode_frame(c, rows, NULL, 0, NULL) ==
              -EINVAL,
          "encode after flush rejected");
    ac4enc_close(c);
    ac4enc_close(NULL);
    /* measure negatives */
    {
        double m, f, tf;
        int pk;
        const int *rr[2] = { pcm, pcm };
        int lens[2] = { 2048, 2048 };
        CHECK(ac4enc_measure(rr, lens, 3, &m, &f, &tf,
                             &pk) == -EINVAL,
              "measure bad nch");
        CHECK(ac4enc_measure(NULL, lens, 2, &m, &f, &tf,
                             &pk) == -EINVAL,
              "measure NULL chs");
    }
}

/* Deterministic PRNG. */
static unsigned
rnd(unsigned *s)
{
    *s = *s * 1103515245u + 12345u;
    return (*s >> 16) & 0x7FFFu;
}

/* Feed an int stream (full frames + tail + flush). Returns the
 * first error rc, or 0 with *nfr frames emitted. */
static int
drive_int(ac4enc_ctx_t *c, int **rows, int nch, int ns,
          uint8_t *ob, size_t obcap, int *nfr)
{
    int full = ns / AC4ENC_FRAME_SAMPLES;
    int rem = ns % AC4ENC_FRAME_SAMPLES;
    int n = 0, is_if = 0, erc;
    size_t olen = 0;
    for (int f = 0; f < full; f++) {
        const int *rr[6];
        for (int ch = 0; ch < nch; ch++)
            rr[ch] = rows[ch] + f * AC4ENC_FRAME_SAMPLES;
        erc = ac4enc_encode_frame2(c, rr, ob, obcap, &olen,
                                   &is_if);
        if (erc < 0)
            return erc;
        if (erc == 1)
            n++;
    }
    if (rem > 0) {
        const int *rr[6];
        for (int ch = 0; ch < nch; ch++)
            rr[ch] = rows[ch] + full * AC4ENC_FRAME_SAMPLES;
        erc = ac4enc_encode_tail2(c, rr, rem, ob, obcap,
                                  &olen, &is_if);
        if (erc < 0)
            return erc;
        if (erc == 1)
            n++;
    }
    for (;;) {
        erc = ac4enc_flush2(c, ob, obcap, &olen, &is_if);
        if (erc <= 0)
            break;
        n++;
    }
    *nfr = n;
    return erc;
}

/* Open a measured ctx over int rows (no spec lines: refusal
 * and count pins don't need the H6 path). */
static int
open_measured(ac4enc_ctx_t **cp, int **rows, int nch,
              int ns, const char *layout, int rate,
              double target, int has_bake, double bake_db)
{
    ac4enc_opts_t o;
    ac4enc_measure_t m;
    const int *chs[6];
    int lens[6];
    memset(&o, 0, sizeof o);
    for (int ch = 0; ch < nch; ch++) {
        chs[ch] = rows[ch];
        lens[ch] = ns;
    }
    if (ac4enc_measure2(chs, lens, nch, &m))
        return -1;
    o.layout = layout;
    o.rate_kbps = rate;
    o.interval = "1sec";
    o.ns2_tone = 1;
    o.shorts = 1;
    o.priming = 1;
    o.iframe_total = -1;
    o.target_db = target;
    o.has_bake = has_bake;
    o.bake_db = bake_db;
    o.has_measure = 1;
    o.m_db = m.m_db;
    o.f_flat = m.f_flat;
    o.tf_tone = m.tf_tone;
    o.in_peak = m.in_peak;
    o.d_peak = m.d_peak;
    o.has_d_peak = m.has_d_peak;
    return ac4enc_open(cp, &o);
}

/* _m entry with true measure must equal the original driver. */
static void
t_equiv(void)
{
    static uint8_t a[1 << 20], b[1 << 20];
    unsigned s = 42;
    for (int iter = 0; iter < 12; iter++) {
        int nch = iter % 2 ? 6 : 2;
        int is_f = (iter / 2) % 2;
        int t0 = (iter / 4) % 2;
        int ns = 100 + (int)(rnd(&s) % 9000);
        int lens[6];
        int *ich[6];
        double *fch[6];
        ac4enc_measure_t m;
        int na, nb, mrc;
        double target = t0 ? 0.0 : -24.0;
        for (int c = 0; c < nch; c++) {
            lens[c] = ns;
            ich[c] = malloc((size_t)ns * sizeof *ich[c]);
            fch[c] = malloc((size_t)ns * sizeof *fch[c]);
            for (int k = 0; k < ns; k++) {
                int v = (int)(rnd(&s) % 20000) - 10000;
                ich[c][k] = v;
                fch[c][k] =
                    (double)v + (rnd(&s) % 1000) / 1000.0;
            }
        }
        if (is_f)
            mrc = ac4enc_measure2_f(
                (const double *const *)fch, lens, nch, &m);
        else
            mrc = ac4enc_measure2(
                (const int *const *)ich, lens, nch, &m);
        CHECK(mrc == 0, "measure ok");
        {
            enc_core_measure_t cm;
            cm.m_db = m.m_db;
            cm.f_flat = m.f_flat;
            cm.tf_tone = m.tf_tone;
            cm.in_peak = m.in_peak;
            cm.d_peak = m.d_peak;
            cm.has_d_peak = m.has_d_peak;
            if (nch == 2 && !is_f) {
                na = enc_core_encode_pcm(
                    ich[0], ns, ich[1], ns, 64, "stereo",
                    "1sec", "zero", NULL, 1, 1, target, 0,
                    0.0, 0, NULL, 0, "manual", NULL, NULL,
                    1, 0, -1, a, sizeof a);
                nb = enc_core_encode_pcm_m(
                    ich[0], ns, ich[1], ns, 64, "stereo",
                    "1sec", "zero", NULL, 1, 1, target, 0,
                    0.0, 0, NULL, 0, "manual", NULL, NULL,
                    1, 0, -1, &cm, b, sizeof b);
            } else if (nch == 2) {
                na = enc_core_encode_pcm_f(
                    fch[0], ns, fch[1], ns, 96, "stereo",
                    "48", "zero", NULL, 1, 0, target, 0,
                    0.0, 1, NULL, 0, "ebu_r128", NULL,
                    NULL, 1, 0, -1, a, sizeof a);
                nb = enc_core_encode_pcm_f_m(
                    fch[0], ns, fch[1], ns, 96, "stereo",
                    "48", "zero", NULL, 1, 0, target, 0,
                    0.0, 1, NULL, 0, "ebu_r128", NULL,
                    NULL, 1, 0, -1, &cm, b, sizeof b);
            } else if (!is_f) {
                na = enc_core_encode_pcm_mc(
                    (const int *const *)ich, lens, 128,
                    "1sec", "zero", NULL, target, 0, 0.0,
                    1, NULL, NULL, NULL, 0, "manual", NULL,
                    NULL, a, sizeof a);
                nb = enc_core_encode_pcm_mc_m(
                    (const int *const *)ich, lens, 128,
                    "1sec", "zero", NULL, target, 0, 0.0,
                    1, NULL, 0, "manual", NULL, NULL, &cm,
                    b, sizeof b);
            } else {
                na = enc_core_encode_pcm_mc_f(
                    (const double *const *)fch, lens, 192,
                    "1sec", "zero", NULL, target, 0, 0.0,
                    1, NULL, NULL, NULL, 0, "manual", NULL,
                    NULL, a, sizeof a);
                nb = enc_core_encode_pcm_mc_f_m(
                    (const double *const *)fch, lens, 192,
                    "1sec", "zero", NULL, target, 0, 0.0,
                    1, NULL, 0, "manual", NULL, NULL, &cm,
                    b, sizeof b);
            }
        }
        if (na < 0 || nb < 0) {
            CHECK(na < 0 && nb < 0, "both refuse together");
        } else {
            CHECK(na == nb && !memcmp(a, b, (size_t)na),
                  "_m equals original");
        }
        for (int c = 0; c < nch; c++) {
            free(ich[c]);
            free(fch[c]);
        }
    }
}

/* S1: sparse-impulse input at the loudness floor must encode.
 * The sparse-iframe fr0 stamp is legitimately 11 bytes
 * (enc.py sparse branch); the old `nfr0 != 12` check refused
 * it with a silent -1. Shape mirrors imp-V300 (24x V=300
 * single-sample impulses @4096 spacing, stereo 48 kHz). */
static void
t_sparse(void)
{
    static const uint8_t want_fr0[11] =
        { 0x60, 0x9c, 0x58, 0x80, 0x03, 0x00, 0xf2, 0xc2,
          0x54, 0xb5, 0x00 };
    static uint8_t blob[1 << 20];
    static uint8_t fr0[12];
    int ns = 96000;
    int *L = malloc((size_t)ns * sizeof *L);
    int *R = malloc((size_t)ns * sizeof *R);
    int nfr;
    CHECK(L && R, "sparse alloc");
    if (!L || !R) {
        free(L);
        free(R);
        return;
    }
    memset(L, 0, (size_t)ns * sizeof *L);
    memset(R, 0, (size_t)ns * sizeof *R);
    for (int s = 0; s < ns; s += 4096) {
        L[s] = 300;
        R[s] = 300;
    }
    /* Stamp shapes. */
    nfr = enc_core_compose_sub0(1, 1, 0x60, 0, NULL, 0x9c,
                                fr0);
    CHECK(nfr == 11, "sparse fr0 is 11 bytes");
    CHECK(!memcmp(fr0, want_fr0, 11), "sparse fr0 bytes");
    nfr = enc_core_compose_sub0(1, 0, 0x60, 0x1230, NULL,
                                0x9c, fr0);
    CHECK(nfr == 12, "dense fr0 stays 12 bytes");
    /* Core int path encodes (was silent -1). */
    {
        int got = enc_core_encode_pcm(
            L, ns, R, ns, 64, "stereo", "1sec", "zero",
            NULL, 1, 1, -24.0, 0, 0.0, 0, NULL, 0,
            "manual", NULL, NULL, 1, 0, -1, blob,
            sizeof blob);
        CHECK(got > 0, "sparse core encode succeeds");
        CHECK(enc_core_last_error()[0] == '\0',
              "no error recorded on success");
    }
    /* Lib path (measured + caller spec lines, incremental
     * pull protocol) emits 49 frames total. */
    {
        ac4enc_ctx_t *c = NULL;
        ac4enc_opts_t o;
        ac4enc_measure_t m;
        const int *chs[2] = { L, R };
        int lens[2] = { ns, ns };
        static uint8_t big[AC4ENC_OUTCAP_SAFE];
        double *Ld = malloc((size_t)ns * sizeof *Ld);
        int full = ns / AC4ENC_FRAME_SAMPLES;
        int rem = ns % AC4ENC_FRAME_SAMPLES;
        int nframes = 0, frc, nl = 0, sl[7];
        size_t olen = 0;
        int is_if = 0;
        memset(&o, 0, sizeof o);
        CHECK(Ld != NULL, "sparse lines alloc");
        CHECK(ac4enc_measure2(chs, lens, 2, &m) == 0,
              "sparse measure ok");
        CHECK(m.tf_tone < 0.6 && m.f_flat > 0.5,
              "test vector is sparse-flagged");
        o.layout = "stereo";
        o.rate_kbps = 64;
        o.interval = "1sec";
        o.ns2_tone = 1;
        o.shorts = 1;
        o.priming = 1;
        o.iframe_total = -1;
        o.target_db = -24.0;
        o.has_measure = 1;
        o.m_db = m.m_db;
        o.f_flat = m.f_flat;
        o.tf_tone = m.tf_tone;
        o.in_peak = m.in_peak;
        o.d_peak = m.d_peak;
        o.has_d_peak = m.has_d_peak;
        if (Ld) {
            for (int k = 0; k < ns; k++)
                Ld[k] = (double)L[k];
            CHECK(ac4enc_spectral_lines(Ld, ns, &m,
                                        o.target_db, 0, 0.0,
                                        &nl, sl) == 0,
                  "sparse lines ok");
            o.has_spec_lines = 1;
            o.n_spec_lines = nl;
            memcpy(o.spec_lines, sl, sizeof sl);
        }
        CHECK(ac4enc_open(&c, &o) == 0, "sparse lib open");
        if (c) {
            for (int f = 0; f < full; f++) {
                const int *rr[2] = {
                    L + f * AC4ENC_FRAME_SAMPLES,
                    R + f * AC4ENC_FRAME_SAMPLES
                };
                int erc = ac4enc_encode_frame2(
                    c, rr, big, sizeof big, &olen,
                    &is_if);
                if (erc < 0) {
                    CHECK(0, "sparse lib feed");
                    break;
                }
                if (erc == 1)
                    nframes++;
            }
            if (rem > 0) {
                const int *rr[2] = {
                    L + full * AC4ENC_FRAME_SAMPLES,
                    R + full * AC4ENC_FRAME_SAMPLES
                };
                int erc = ac4enc_encode_tail2(
                    c, rr, rem, big, sizeof big, &olen,
                    &is_if);
                CHECK(erc >= 0, "sparse lib tail");
                if (erc == 1)
                    nframes++;
            }
            for (;;) {
                frc = ac4enc_flush2(c, big, sizeof big,
                                    &olen, &is_if);
                if (frc <= 0)
                    break;
                nframes++;
            }
            CHECK(frc == 0 && nframes == 49,
                  "sparse lib emits 49 frames");
            CHECK(ac4enc_last_error()[0] == '\0',
                  "lib error empty on success");
            ac4enc_close(c);
        }
        free(Ld);
    }
    free(L);
    free(R);
}

/* M2: every option field validated at open. */
static void
t_open_neg2(void)
{
    ac4enc_ctx_t *c = NULL;
    ac4enc_opts_t o;
    ac4enc_drc_mode_t modes[1];
    ac4enc_drc_t drc;
    memset(&o, 0, sizeof o);
    o.layout = "stereo";
    o.rate_kbps = 64;
    o.ns2_tone = 1;
    o.shorts = 1;
    o.priming = 1;
    o.iframe_total = -1;
    o.target_db = -24.0;
    o.has_measure = 1;
    o.m_db = -20.0;
    o.f_flat = 0.1;
    o.tf_tone = 0.9;
    o.in_peak = 100;
    CHECK(ac4enc_open(&c, &o) == 0, "neg2 baseline opens");
    ac4enc_close(c);
    c = NULL;
    o.interval = "abc";
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "garbage interval rejected");
    o.interval = "1";
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "interval step 1 rejected");
    o.interval = "48";
    CHECK(ac4enc_open(&c, &o) == 0, "decimal interval ok");
    ac4enc_close(c);
    c = NULL;
    /* FUZZ-CENC crash-2: LLONG_MAX step overflowed 1 + step.
     * Boundary-exact: LLONG_MAX-1 opens, LLONG_MAX fails. */
    o.interval = "9223372036854775807";
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "interval LLONG_MAX rejected");
    o.interval = "99999999999999999999";
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "saturating interval rejected");
    o.interval = "9223372036854775806";
    CHECK(ac4enc_open(&c, &o) == 0,
          "interval LLONG_MAX-1 opens");
    ac4enc_close(c);
    c = NULL;
    {
        int set[48];
        CHECK(enc_core_iframe_set(
                  48, "9223372036854775807",
                  NULL, 0, set) < 0,
              "iframe_set LLONG_MAX rejected");
        CHECK(enc_core_iframe_set(
                  48, "9223372036854775806",
                  NULL, 0, set) == 2,
              "iframe_set LLONG_MAX-1 marks {0,1}");
        CHECK(enc_core_joint_iframes(
                  48, 0, 48, "9223372036854775807",
                  NULL, 0, set) < 0,
              "joint_iframes LLONG_MAX rejected");
        CHECK(enc_core_joint_iframes(
                  48, 0, 48, "9223372036854775806",
                  NULL, 0, set) == 1,
              "joint_iframes LLONG_MAX-1 marks {0}");
    }
    o.interval = NULL;
    o.has_bake = 1;
    o.bake_db = NAN;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "NaN bake_db rejected");
    o.bake_db = 3.0;
    CHECK(ac4enc_open(&c, &o) == 0, "explicit bake ok");
    ac4enc_close(c);
    c = NULL;
    o.has_bake = 0;
    o.f_flat = NAN;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "NaN f_flat rejected");
    o.f_flat = 0.1;
    o.tf_tone = NAN;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "NaN tf_tone rejected");
    o.tf_tone = 0.9;
    o.in_peak = -1;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "negative in_peak rejected");
    o.in_peak = 100;
    o.has_d_peak = 1;
    o.d_peak = -2.0;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "negative d_peak rejected");
    o.d_peak = NAN;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "NaN d_peak rejected");
    o.d_peak = 100.0;
    CHECK(ac4enc_open(&c, &o) == 0, "d_peak ok");
    ac4enc_close(c);
    c = NULL;
    o.has_d_peak = 0;
    o.has_bake = 2;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "has_bake=2 rejected");
    o.has_bake = 0;
    o.has_measure = 2;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "has_measure=2 rejected");
    o.has_measure = 1;
    o.has_bake = 1;
    o.has_measure = 0;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "bake without measure rejected");
    o.has_bake = 0;
    o.has_measure = 1;
    o.layout = "5.1";
    o.rate_kbps = 128;
    o.joint_shorts = 1;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "mc joint_shorts rejected");
    o.joint_shorts = 0;
    o.priming = 0;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "mc priming=0 rejected");
    o.priming = 1;
    o.layout = "stereo";
    o.rate_kbps = 64;
    o.sample_rate = 44100;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "44100 rejected");
    o.sample_rate = 48000;
    CHECK(ac4enc_open(&c, &o) == 0, "48000 ok");
    ac4enc_close(c);
    c = NULL;
    o.sample_rate = 0;
    o.has_spec_lines = 1;
    o.n_spec_lines = 8;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "8 spec lines rejected");
    o.n_spec_lines = 1;
    o.spec_lines[0] = 2048;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "spec line 2048 rejected");
    o.spec_lines[0] = -1;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "spec line -1 rejected");
    o.spec_lines[0] = 100;
    CHECK(ac4enc_open(&c, &o) == 0, "spec line ok");
    ac4enc_close(c);
    c = NULL;
    o.has_spec_lines = 0;
    memset(modes, 0, sizeof modes);
    drc.eac3 = 0;
    drc.modes = modes;
    drc.nmodes = 0;
    o.drc = &drc;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "drc nmodes=0 rejected");
    drc.nmodes = 1;
    modes[0].mid = 0;
    modes[0].kind = 99;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "drc bad kind rejected");
    modes[0].kind = AC4ENC_DRC_DEFAULT;
    drc.eac3 = 8;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "drc eac3=8 rejected");
    drc.eac3 = 0;
    modes[0].mid = 9;
    CHECK(ac4enc_open(&c, &o) == -EINVAL,
          "drc mid=9 rejected");
    modes[0].mid = 0;
    CHECK(ac4enc_open(&c, &o) == 0, "drc default ok");
    ac4enc_close(c);
    c = NULL;
    o.drc = NULL;
}

/* M1: strerror/subcode/stage/frame detail. */
static void
t_diag(void)
{
    ac4enc_ctx_t *c = NULL;
    ac4enc_opts_t o;
    static uint8_t ob[AC4ENC_OUTCAP_SAFE];
    int pcm[2048];
    const int *rows[2] = { pcm, pcm };
    size_t olen = 0;
    CHECK(!strcmp(ac4enc_strerror(0), "ok"), "strerror ok");
    CHECK(!strcmp(ac4enc_strerror(1), "frame ready"),
          "strerror ready");
    CHECK(!strcmp(ac4enc_strerror(-EINVAL),
                  "invalid argument or option"),
          "strerror einval");
    CHECK(!strcmp(ac4enc_strerror(-ENOMEM),
                  "out of memory"),
          "strerror enomem");
    CHECK(!strcmp(ac4enc_strerror(-EIO), "encode failure"),
          "strerror eio");
    CHECK(!strcmp(ac4enc_strerror(-9999), "unknown"),
          "strerror unknown");
    CHECK(!strcmp(ac4enc_subcode_str(AC4ENC_EBAKE_PEAK),
                  "baked peak overflow"),
          "subcode str");
    CHECK(ac4enc_err_sub(NULL) == AC4ENC_EOK,
          "null sub ok");
    CHECK(ac4enc_err_frame(NULL) == -1, "null frame -1");
    memset(&o, 0, sizeof o);
    memset(pcm, 0, sizeof pcm);
    o.layout = "stereo";
    o.rate_kbps = 64;
    o.ns2_tone = 1;
    o.shorts = 1;
    o.priming = 1;
    o.iframe_total = -1;
    o.target_db = -24.0;
    o.has_measure = 1;
    o.m_db = -20.0;
    o.f_flat = 0.1;
    o.tf_tone = 0.9;
    o.in_peak = 100;
    CHECK(ac4enc_open(&c, &o) == 0, "diag open");
    CHECK(ac4enc_err_sub(c) == AC4ENC_EOK, "sub ok");
    CHECK(!strcmp(ac4enc_err_stage(c), "-"),
          "stage dash");
    CHECK(ac4enc_encode_frame(c, rows, NULL, 0, NULL) ==
              -EINVAL,
          "diag short");
    CHECK(ac4enc_err_sub(c) == AC4ENC_ESHORTBUF,
          "sub shortbuf");
    CHECK(!strcmp(ac4enc_err_stage(c), "feed"),
          "stage feed");
    CHECK(ac4enc_encode_frame2(c, rows, ob, sizeof ob,
                               &olen, NULL) == 1,
          "diag emits after short");
    ac4enc_close(c);
    /* Bake overflow refusal carries subcode + stage. */
    c = NULL;
    o.has_bake = 1;
    o.bake_db = 40.0;
    CHECK(ac4enc_open(&c, &o) == 0, "diag bake open");
    pcm[0] = INT_MAX;
    CHECK(ac4enc_encode_frame2(c, rows, ob, sizeof ob,
                               &olen, NULL) == -EIO,
          "bake overflow eio");
    CHECK(ac4enc_err_sub(c) == AC4ENC_EBAKE_OVERFLOW,
          "sub bake overflow");
    CHECK(!strcmp(ac4enc_err_stage(c), "feed"),
          "stage feed bake");
    CHECK(ac4enc_encode_frame2(c, rows, ob, sizeof ob,
                               &olen, NULL) == -EIO,
          "sticky after eio");
    ac4enc_close(c);
    /* Baked-peak refusal at end. */
    c = NULL;
    o.bake_db = 6.0;
    CHECK(ac4enc_open(&c, &o) == 0, "diag peak open");
    for (int k = 0; k < 2048; k++)
        pcm[k] = 20000;
    CHECK(ac4enc_encode_frame2(c, rows, ob, sizeof ob,
                               &olen, NULL) == 1,
          "peak feeds");
    CHECK(ac4enc_encode_tail2(c, rows, 100, ob, sizeof ob,
                              &olen, NULL) == -EIO,
          "peak refused at tail");
    CHECK(ac4enc_err_sub(c) == AC4ENC_EBAKE_PEAK,
          "sub bake peak");
    CHECK(!strcmp(ac4enc_err_stage(c), "tail"),
          "stage tail");
    ac4enc_close(c);
}

/* M5: runtime self-check green in a normal build. */
static void
t_fp(void)
{
    CHECK(ac4enc_fp_ok() == 1, "fp env ok");
}

/* M7: reset reuses the ctx (same bytes, clears errors). */
static void
t_reset(void)
{
    ac4enc_ctx_t *c = NULL;
    ac4enc_opts_t o;
    static uint8_t a[AC4ENC_OUTCAP_SAFE * 4];
    static uint8_t b[AC4ENC_OUTCAP_SAFE * 4];
    static uint8_t ob[AC4ENC_OUTCAP_SAFE];
    int pcm[2048];
    const int *rows[2] = { pcm, pcm };
    size_t na = 0, nb = 0, olen = 0;
    memset(&o, 0, sizeof o);
    for (int k = 0; k < 2048; k++)
        pcm[k] = (k * 37) % 1000 - 500;
    o.layout = "stereo";
    o.rate_kbps = 64;
    o.ns2_tone = 1;
    o.shorts = 1;
    o.priming = 1;
    o.iframe_total = -1;
    o.target_db = -24.0;
    o.has_measure = 1;
    o.m_db = -20.0;
    o.f_flat = 0.1;
    o.tf_tone = 0.9;
    o.in_peak = 500;
    CHECK(ac4enc_open(&c, &o) == 0, "reset open");
    CHECK(ac4enc_encode_frame2(c, rows, ob, sizeof ob,
                               &olen, NULL) == 1,
          "reset feeds");
    memcpy(a + na, ob, olen);
    na += olen;
    CHECK(ac4enc_encode_tail2(c, rows, 100, ob, sizeof ob,
                              &olen, NULL) == 1,
          "reset tails");
    memcpy(a + na, ob, olen);
    na += olen;
    for (;;) {
        int frc = ac4enc_flush2(c, ob, sizeof ob, &olen,
                                NULL);
        if (frc <= 0) {
            CHECK(frc == 0, "reset drains");
            break;
        }
        memcpy(a + na, ob, olen);
        na += olen;
    }
    CHECK(ac4enc_fed_samples(c) == 2048 + 100,
          "reset fed total");
    CHECK(ac4enc_reset(c) == 0, "reset ok");
    CHECK(ac4enc_fed_samples(c) == 0, "m2 reset fed");
    CHECK(ac4enc_emitted_frames(c) == 0, "m2 reset emitted");
    CHECK(ac4enc_encode_frame2(c, rows, ob, sizeof ob,
                               &olen, NULL) == 1,
          "reset refeeds");
    memcpy(b + nb, ob, olen);
    nb += olen;
    CHECK(ac4enc_encode_tail2(c, rows, 100, ob, sizeof ob,
                              &olen, NULL) == 1,
          "reset retails");
    memcpy(b + nb, ob, olen);
    nb += olen;
    for (;;) {
        int frc = ac4enc_flush2(c, ob, sizeof ob, &olen,
                                NULL);
        if (frc <= 0)
            break;
        memcpy(b + nb, ob, olen);
        nb += olen;
    }
    CHECK(na == nb && !memcmp(a, b, na),
          "reset same bytes");
    CHECK(ac4enc_reset(NULL) == -EINVAL,
          "reset null rejected");
    ac4enc_close(c);
}

/* M8: max_packet query + pinning (no emit exceeds it). */
static void
t_maxpacket(void)
{
    CHECK(ac4enc_max_packet(64) == 342, "max64");
    CHECK(ac4enc_max_packet(768) == 4096, "max768");
    CHECK(ac4enc_max_packet(48) > 0, "max48 sane");
    CHECK(ac4enc_max_packet(0) == -EINVAL, "max0 bad");
    CHECK(ac4enc_max_packet(-1) == -EINVAL, "maxneg bad");
    CHECK(AC4ENC_OUTCAP_SAFE >=
              (size_t)ac4enc_max_packet(768),
          "safe covers max");
}

/* Strip sync+size+trailer -> bare frames. Bytes or -1. */
static int
strip_blob(const uint8_t *blob, int n, uint8_t *out,
           size_t outcap, int *nframes)
{
    int pos = 0, nf = 0;
    size_t wpos = 0;
    while (pos < n) {
        int len;
        if (pos + 6 > n)
            return -1;
        if (blob[pos] != 0xAC || blob[pos + 1] != 0x41)
            return -1;
        len = (blob[pos + 2] << 8) | blob[pos + 3];
        if (pos + 4 + len + 2 > n)
            return -1;
        if (wpos + (size_t)len > outcap)
            return -1;
        memcpy(out + wpos, blob + pos + 4, (size_t)len);
        wpos += (size_t)len;
        nf++;
        pos += 4 + len + 2;
    }
    *nframes = nf;
    return (int)wpos;
}

/* Encode rows through the lib (measured opts + optional lines).
 * Collects bytes+flags; returns total or -1 (lib failed). */
static int
lib_run(const ac4enc_opts_t *o, const int *const *pi,
        const double *const *pf, int nch, int ns,
        uint8_t *out, size_t outcap, int *flags,
        int flagcap, int *pinmax)
{
    ac4enc_ctx_t *c = NULL;
    static uint8_t pkt[AC4ENC_OUTCAP_SAFE];
    int full = ns / AC4ENC_FRAME_SAMPLES;
    int rem = ns % AC4ENC_FRAME_SAMPLES;
    int nf = 0, maxp;
    size_t wpos = 0;
    if (ac4enc_open(&c, o))
        return -1;
    maxp = ac4enc_max_packet(o->rate_kbps);
    for (int f = 0; f < full; f++) {
        size_t olen = 0;
        int is_if = 0, erc;
        if (pf) {
            const double *rr[6];
            for (int ch = 0; ch < nch; ch++)
                rr[ch] = pf[ch] + f * AC4ENC_FRAME_SAMPLES;
            erc = ac4enc_encode_frame_f2(c, rr, pkt,
                                         sizeof pkt, &olen,
                                         &is_if);
        } else {
            const int *rr[6];
            for (int ch = 0; ch < nch; ch++)
                rr[ch] = pi[ch] + f * AC4ENC_FRAME_SAMPLES;
            erc = ac4enc_encode_frame2(c, rr, pkt,
                                       sizeof pkt, &olen,
                                       &is_if);
        }
        if (erc < 0) {
            ac4enc_close(c);
            return -1;
        }
        if (erc == 1) {
            if (pinmax && (int)olen > maxp)
                *pinmax = (int)olen;
            if (wpos + olen > outcap || nf >= flagcap) {
                ac4enc_close(c);
                return -1;
            }
            memcpy(out + wpos, pkt, olen);
            wpos += olen;
            flags[nf++] = is_if;
        }
    }
    if (rem > 0) {
        size_t olen = 0;
        int is_if = 0, erc;
        if (pf) {
            const double *rr[6];
            for (int ch = 0; ch < nch; ch++)
                rr[ch] = pf[ch] + full * AC4ENC_FRAME_SAMPLES;
            erc = ac4enc_encode_tail_f2(c, rr, rem, pkt,
                                        sizeof pkt, &olen,
                                        &is_if);
        } else {
            const int *rr[6];
            for (int ch = 0; ch < nch; ch++)
                rr[ch] = pi[ch] + full * AC4ENC_FRAME_SAMPLES;
            erc = ac4enc_encode_tail2(c, rr, rem, pkt,
                                      sizeof pkt, &olen,
                                      &is_if);
        }
        if (erc < 0) {
            ac4enc_close(c);
            return -1;
        }
        if (erc == 1) {
            if (pinmax && (int)olen > maxp)
                *pinmax = (int)olen;
            if (wpos + olen > outcap || nf >= flagcap) {
                ac4enc_close(c);
                return -1;
            }
            memcpy(out + wpos, pkt, olen);
            wpos += olen;
            flags[nf++] = is_if;
        }
    }
    for (;;) {
        size_t olen = 0;
        int is_if = 0, frc;
        frc = ac4enc_flush2(c, pkt, sizeof pkt, &olen,
                            &is_if);
        if (frc < 0) {
            ac4enc_close(c);
            return -1;
        }
        if (frc == 0)
            break;
        if (pinmax && (int)olen > maxp)
            *pinmax = (int)olen;
        if (wpos + olen > outcap || nf >= flagcap) {
            ac4enc_close(c);
            return -1;
        }
        memcpy(out + wpos, pkt, olen);
        wpos += olen;
        flags[nf++] = is_if;
    }
    ac4enc_close(c);
    /* packed: bytes | nf<<24 (bufs < 16MB, nf < 256) */
    return (int)wpos | (nf << 24);
}

/* M6: priming=0 joint path — bytes + count + flags vs core. */
static void
t_prim0(void)
{
    static uint8_t libb[1 << 20], coreb[1 << 20];
    static uint8_t core[1 << 20];
    unsigned s = 777;
    int ns = 5000;
    int lens[2] = { ns, ns };
    int *ich[2];
    int flags[16], expect[16], set[16];
    int librc, nb, nf, nset, pinmax = 0;
    ac4enc_opts_t o;
    ac4enc_measure_t m;
    enc_core_measure_t cm;
    double *dL;
    int nl, sl[7];
    for (int ch = 0; ch < 2; ch++) {
        ich[ch] = malloc((size_t)ns * sizeof *ich[ch]);
        for (int k = 0; k < ns; k++)
            ich[ch][k] = (int)(rnd(&s) % 20000) - 10000;
    }
    CHECK(ac4enc_measure2((const int *const *)ich, lens,
                          2, &m) == 0,
          "prim0 measure");
    dL = malloc((size_t)ns * sizeof *dL);
    for (int k = 0; k < ns; k++)
        dL[k] = (double)ich[0][k];
    memset(&o, 0, sizeof o);
    o.layout = "stereo";
    o.rate_kbps = 64;
    o.interval = "1sec";
    o.ns2_tone = 1;
    o.shorts = 1;
    o.priming = 0;
    o.iframe_base = 0;
    o.iframe_total = -1;
    o.target_db = -24.0;
    o.has_measure = 1;
    o.m_db = m.m_db;
    o.f_flat = m.f_flat;
    o.tf_tone = m.tf_tone;
    o.in_peak = m.in_peak;
    CHECK(ac4enc_spectral_lines(dL, ns, &m, -24.0, 0, 0.0,
                                &nl, sl) == 0,
          "prim0 lines");
    o.has_spec_lines = 1;
    o.n_spec_lines = nl;
    memcpy(o.spec_lines, sl, sizeof sl);
    free(dL);
    cm.m_db = m.m_db;
    cm.f_flat = m.f_flat;
    cm.tf_tone = m.tf_tone;
    cm.in_peak = m.in_peak;
    cm.d_peak = m.d_peak;
    cm.has_d_peak = m.has_d_peak;
    nb = enc_core_encode_pcm_m(
        ich[0], ns, ich[1], ns, 64, "stereo", "1sec",
        "zero", NULL, 1, 1, -24.0, 0, 0.0, 0, NULL, 0,
        "manual", NULL, NULL, 0, 0, -1, &cm, core,
        sizeof core);
    librc = lib_run(&o, (const int *const *)ich, NULL, 2,
                    ns, libb, sizeof libb, flags, 16,
                    &pinmax);
    if (nb < 0 || librc < 0) {
        CHECK(nb < 0 && librc < 0,
              "prim0 both refuse together");
    } else {
        int libnf = librc >> 24, libn = librc & 0xFFFFFF;
        int coren = strip_blob(core, nb, coreb,
                               sizeof coreb, &nf);
        CHECK(coren >= 0 && libn == coren &&
                  !memcmp(libb, coreb, (size_t)libn),
              "prim0 bytes");
        CHECK(libnf == nf, "prim0 count");
        nset = enc_core_joint_iframes(nf, 0, nf, "1sec",
                                      NULL, 0, set);
        CHECK(nset >= 0, "prim0 recompute");
        for (int k = 0; k < 16; k++)
            expect[k] = 0;
        for (int k = 0; k < nset; k++)
            if (set[k] >= 0 && set[k] < nf)
                expect[set[k]] = 1;
        CHECK(libnf == nf &&
                  !memcmp(flags, expect,
                          (size_t)nf * sizeof *flags),
              "prim0 flags");
        CHECK(pinmax == 0, "prim0 pinning");
    }
    /* Explicit total == default resolution for base 0. */
    o.iframe_total = 3;
    librc = lib_run(&o, (const int *const *)ich, NULL, 2,
                    ns, libb, sizeof libb, flags, 16,
                    NULL);
    nb = enc_core_encode_pcm_m(
        ich[0], ns, ich[1], ns, 64, "stereo", "1sec",
        "zero", NULL, 1, 1, -24.0, 0, 0.0, 0, NULL, 0,
        "manual", NULL, NULL, 0, 0, 3, &cm, core,
        sizeof core);
    if (nb < 0 || librc < 0) {
        CHECK(nb < 0 && librc < 0,
              "prim0exp both refuse");
    } else {
        int libnf = librc >> 24, libn = librc & 0xFFFFFF;
        int coren = strip_blob(core, nb, coreb,
                               sizeof coreb, &nf);
        CHECK(coren >= 0 && libn == coren && libnf == nf &&
                  !memcmp(libb, coreb, (size_t)libn),
              "prim0exp bytes");
    }
    free(ich[0]);
    free(ich[1]);
}

/* B1: two contexts encoding concurrently (threads) match
 * each other and a sequential reference. */
struct conc_arg {
    const int *rows[2];
    int ns;
    ac4enc_opts_t opts;
    uint8_t *out;
    int *flags;
    int rc;
};

static void *
conc_run(void *arg)
{
    struct conc_arg *a = arg;
    /* out sized 1MB by the caller; flags 64. */
    a->rc = lib_run(&a->opts,
                    (const int *const *)a->rows, NULL, 2,
                    a->ns, a->out, 1 << 20, a->flags, 64,
                    NULL);
    return NULL;
}

static void
t_concurrent(void)
{
    static uint8_t ref[1 << 20], o1[1 << 20],
        o2[1 << 20];
    static int fref[64], f1[64], f2[64];
    unsigned s = 31337;
    int ns = 6000;
    int lens[2] = { ns, ns };
    int *ich[2];
    struct conc_arg a1, a2;
    pthread_t t1, t2;
    int rrc;
    ac4enc_opts_t o;
    ac4enc_measure_t m;
    for (int ch = 0; ch < 2; ch++) {
        ich[ch] = malloc((size_t)ns * sizeof *ich[ch]);
        for (int k = 0; k < ns; k++)
            ich[ch][k] = (int)(rnd(&s) % 20000) - 10000;
    }
    CHECK(ac4enc_measure2((const int *const *)ich, lens,
                          2, &m) == 0,
          "conc measure");
    memset(&o, 0, sizeof o);
    o.layout = "stereo";
    o.rate_kbps = 96;
    o.interval = "1sec";
    o.ns2_tone = 1;
    o.shorts = 1;
    o.joint_shorts = 1;
    o.priming = 1;
    o.iframe_total = -1;
    o.target_db = -24.0;
    o.has_measure = 1;
    o.m_db = m.m_db;
    o.f_flat = m.f_flat;
    o.tf_tone = m.tf_tone;
    o.in_peak = m.in_peak;
    rrc = lib_run(&o, (const int *const *)ich, NULL, 2,
                  ns, ref, sizeof ref, fref, 64, NULL);
    CHECK(rrc >= 0, "conc reference");
    a1.rows[0] = ich[0];
    a1.rows[1] = ich[1];
    a1.ns = ns;
    a1.opts = o;
    a1.out = o1;
    a1.flags = f1;
    a1.rc = -1;
    a2 = a1;
    a2.out = o2;
    a2.flags = f2;
    CHECK(pthread_create(&t1, NULL, conc_run, &a1) == 0,
          "conc spawn1");
    CHECK(pthread_create(&t2, NULL, conc_run, &a2) == 0,
          "conc spawn2");
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    CHECK(a1.rc >= 0 && a2.rc >= 0, "conc both ok");
    if (rrc >= 0 && a1.rc >= 0 && a2.rc >= 0) {
        int rn = rrc & 0xFFFFFF, n1 = a1.rc & 0xFFFFFF,
            n2 = a2.rc & 0xFFFFFF;
        CHECK(rn == n1 && rn == n2 &&
                  !memcmp(ref, o1, (size_t)rn) &&
                  !memcmp(ref, o2, (size_t)rn),
              "conc bytes match");
        CHECK(!memcmp(fref, f1, sizeof fref) &&
                  !memcmp(fref, f2, sizeof fref),
              "conc flags match");
    }
    free(ich[0]);
    free(ich[1]);
}

/* B2: incremental lib == whole-stream core across a matrix
 * (layouts x domains x targets x shorts x joint x priming x
 * lengths), bytes + count + flags; pinning on every frame. */
static void
t_increquiv(void)
{
    static uint8_t libb[1 << 20], coreb[1 << 20];
    static uint8_t core[1 << 20];
    static const int nss[] = { 1, 2047, 2048, 2049,
                               5000, 9000 };
    unsigned s = 90210;
    for (int vi = 0; vi < 14; vi++) {
        int nch = vi % 3 == 2 ? 6 : 2;
        int is_f = (vi / 3) % 2;
        int t0 = (vi / 6) % 2;
        int joint = vi == 9 || vi == 10;
        int prim0 = vi == 11 || vi == 12;
        int short0 = vi == 13;
        int ns = nss[vi % 6];
        int lens[6];
        int *ich[6];
        double *fch[6];
        double *dL = NULL;
        int nl = 0, sl[7];
        ac4enc_opts_t o;
        ac4enc_measure_t m;
        enc_core_measure_t cm;
        int flags[64], expect[64], set[64];
        int librc, nb = -2, nf = 0, coren, nset;
        int libnf, libn, pinmax = 0;
        double target = t0 ? 0.0 : -24.0;
        int rate = nch == 2 ? 64 : 128;
        if (joint && nch != 2)
            continue;
        if (prim0 && (nch != 2 || is_f))
            continue;
        for (int ch = 0; ch < nch; ch++) {
            lens[ch] = ns;
            ich[ch] =
                malloc((size_t)ns * sizeof *ich[ch]);
            fch[ch] =
                malloc((size_t)ns * sizeof *fch[ch]);
            for (int k = 0; k < ns; k++) {
                int v =
                    (int)(rnd(&s) % 20000) - 10000;
                ich[ch][k] = v;
                fch[ch][k] = (double)v +
                    (rnd(&s) % 1000) / 1000.0;
            }
        }
        if (is_f)
            CHECK(ac4enc_measure2_f(
                      (const double *const *)fch, lens,
                      nch, &m) == 0,
                  "increq measure");
        else
            CHECK(ac4enc_measure2(
                      (const int *const *)ich, lens, nch,
                      &m) == 0,
                  "increq measure");
        memset(&o, 0, sizeof o);
        o.layout = nch == 2 ? "stereo" : "5.1";
        o.rate_kbps = rate;
        o.interval = vi % 2 ? "48" : "1sec";
        o.ns2_tone = 1;
        o.shorts = short0 ? 0 : 1;
        o.joint_shorts = joint ? 1 : 0;
        o.priming = prim0 ? 0 : 1;
        o.iframe_base = 0;
        o.iframe_total = -1;
        o.target_db = target;
        o.has_measure = 1;
        o.m_db = m.m_db;
        o.f_flat = m.f_flat;
        o.tf_tone = m.tf_tone;
        o.in_peak = m.in_peak;
        o.d_peak = m.d_peak;
        o.has_d_peak = m.has_d_peak;
        if (nch == 2) {
            /* H6 lines over L (exact-double for int). */
            const double *Lr;
            if (is_f) {
                Lr = fch[0];
            } else {
                dL = malloc((size_t)ns * sizeof *dL);
                for (int k = 0; k < ns; k++)
                    dL[k] = (double)ich[0][k];
                Lr = dL;
            }
            if (ac4enc_spectral_lines(Lr, ns, &m, target,
                                      0, 0.0, &nl,
                                      sl) == 0) {
                o.has_spec_lines = 1;
                o.n_spec_lines = nl;
                memcpy(o.spec_lines, sl, sizeof sl);
            }
            free(dL);
            dL = NULL;
        }
        cm.m_db = m.m_db;
        cm.f_flat = m.f_flat;
        cm.tf_tone = m.tf_tone;
        cm.in_peak = m.in_peak;
        cm.d_peak = m.d_peak;
        cm.has_d_peak = m.has_d_peak;
        if (nch == 2 && !is_f)
            nb = enc_core_encode_pcm_m(
                ich[0], ns, ich[1], ns, rate, "stereo",
                o.interval, "zero", NULL, 1, o.shorts,
                target, 0, 0.0, o.joint_shorts, NULL, 0,
                "manual", NULL, NULL, o.priming, 0, -1,
                &cm, core, sizeof core);
        else if (nch == 2)
            nb = enc_core_encode_pcm_f_m(
                fch[0], ns, fch[1], ns, rate, "stereo",
                o.interval, "zero", NULL, 1, o.shorts,
                target, 0, 0.0, o.joint_shorts, NULL, 0,
                "manual", NULL, NULL, o.priming, 0, -1,
                &cm, core, sizeof core);
        else if (!is_f)
            nb = enc_core_encode_pcm_mc_m(
                (const int *const *)ich, lens, rate,
                o.interval, "zero", NULL, target, 0, 0.0,
                o.shorts, NULL, 0, "manual", NULL, NULL,
                &cm, core, sizeof core);
        else
            nb = enc_core_encode_pcm_mc_f_m(
                (const double *const *)fch, lens, rate,
                o.interval, "zero", NULL, target, 0, 0.0,
                o.shorts, NULL, 0, "manual", NULL, NULL,
                &cm, core, sizeof core);
        librc = lib_run(&o,
                        is_f ? NULL
                             : (const int *const *)ich,
                        is_f ? (const double *const *)fch
                             : NULL,
                        nch, ns, libb, sizeof libb, flags,
                        64, &pinmax);
        if (nb < 0 || librc < 0) {
            CHECK(nb < 0 && librc < 0,
                  "increq both refuse together");
        } else {
            libnf = librc >> 24;
            libn = librc & 0xFFFFFF;
            coren = strip_blob(core, nb, coreb,
                               sizeof coreb, &nf);
            CHECK(coren >= 0 && libn == coren &&
                      !memcmp(libb, coreb, (size_t)libn),
                  "increq bytes");
            CHECK(libnf == nf, "increq count");
            if (nch == 6 || !prim0)
                nset = enc_core_iframe_set(
                    nf, o.interval, NULL, 0, set);
            else
                nset = enc_core_joint_iframes(
                    nf, 0, nf, o.interval, NULL, 0, set);
            CHECK(nset >= 0, "increq recompute");
            for (int k = 0; k < 64; k++)
                expect[k] = 0;
            for (int k = 0; k < nset; k++)
                if (set[k] >= 0 && set[k] < nf)
                    expect[set[k]] = 1;
            CHECK(libnf == nf &&
                      !memcmp(flags, expect,
                              (size_t)nf *
                                  sizeof *flags),
                  "increq flags");
            CHECK(pinmax == 0, "increq pinning");
        }
        for (int ch = 0; ch < nch; ch++) {
            free(ich[ch]);
            free(fch[ch]);
        }
    }
}

/* R3: guard strictness pins (S1 family, policy (a)). The
 * bake peak gates are pyref-mandated (enc.py asserts
 * peak < 32767), so refusals stay — but loud, naming the
 * gate and the offending values. */
static void
t_r3(void)
{
    static uint8_t ob[AC4ENC_OUTCAP_SAFE];
    static uint8_t blob[1 << 20];
    /* R3-1: sparse + hot peak + correction attenuation
     * encodes (the SPAN 58/59 shape, post-S1-fix). */
    {
        int ns = 2049;
        int *L = malloc((size_t)ns * sizeof *L);
        int *R = malloc((size_t)ns * sizeof *R);
        int *rows[2] = { L, R };
        ac4enc_ctx_t *c = NULL;
        ac4enc_measure_t m;
        const int *chs[2];
        int lens[2] = { ns, ns };
        int nfr = 0, got;
        unsigned st = 987654321u;
        CHECK(L && R, "r3 span alloc");
        if (!L || !R) {
            free(L);
            free(R);
            return;
        }
        for (int k = 0; k < ns; k++) {
            L[k] = (int)(rnd(&st) % 20001u) - 10000;
            R[k] = (int)(rnd(&st) % 20001u) - 10000;
        }
        L[0] = R[0] = 32000;
        chs[0] = L;
        chs[1] = R;
        CHECK(ac4enc_measure2(chs, lens, 2, &m) == 0,
              "r3 span measure ok");
        CHECK(m.tf_tone < 0.6 && m.f_flat > 0.5,
              "r3 span vector is sparse-flagged");
        CHECK(open_measured(&c, rows, 2, ns, "stereo", 64,
                            -24.0, 0, 0.0) == 0,
              "r3 span lib open");
        if (c) {
            CHECK(drive_int(c, rows, 2, ns, ob, sizeof ob,
                            &nfr) == 0,
                  "r3 span lib encodes");
            CHECK(nfr == 4, "r3 span lib emits 4 frames");
            CHECK(ac4enc_err_detail(c)[0] == '\0',
                  "r3 span detail empty on success");
            ac4enc_close(c);
        }
        got = enc_core_encode_pcm(L, ns, R, ns, 64,
                                  "stereo", "1sec", "zero",
                                  NULL, 1, 1, -24.0, 0, 0.0,
                                  0, NULL, 0, "manual", NULL,
                                  NULL, 1, 0, -1, blob,
                                  sizeof blob);
        CHECK(got > 0, "r3 span core encodes");
        CHECK(enc_core_last_error()[0] == '\0',
              "r3 span core silent on success");
        free(L);
        free(R);
    }
    /* R3-2/3 stereo: quiet bed + hot impulse refuses loud
     * (genuine bake overflow; pyref asserts the same). */
    {
        int ns = 8192;
        int *L = malloc((size_t)ns * sizeof *L);
        int *R = malloc((size_t)ns * sizeof *R);
        int *rows[2] = { L, R };
        ac4enc_ctx_t *c = NULL;
        int nfr = 0, got, v = 0;
        const char *d, *e;
        CHECK(L && R, "r3 hot alloc");
        if (!L || !R) {
            free(L);
            free(R);
            return;
        }
        for (int k = 0; k < ns; k++) {
            L[k] = (int)round(1000.0 *
                sin(2.0 * 3.141592653589793 * 440.0 * k /
                    48000.0));
            R[k] = (int)round(1000.0 *
                sin(2.0 * 3.141592653589793 * 880.0 * k /
                    48000.0));
        }
        L[0] = R[0] = L[4096] = R[4096] = 20000;
        CHECK(open_measured(&c, rows, 2, ns, "stereo", 128,
                            -24.0, 0, 0.0) == 0,
              "r3 hot lib open");
        if (c) {
            CHECK(drive_int(c, rows, 2, ns, ob, sizeof ob,
                            &nfr) == -EIO,
                  "r3 hot lib refuses");
            CHECK(ac4enc_err_sub(c) == AC4ENC_EBAKE_PEAK,
                  "r3 hot sub peak");
            d = ac4enc_err_detail(c);
            CHECK(strstr(d, "baked peak gate: baked_peak="),
                  "r3 hot detail names gate");
            CHECK(sscanf(d,
                         "baked peak gate: baked_peak=%d",
                         &v) == 1 && v == 37281,
                  "r3 hot detail peak 37281");
            CHECK(strstr(d, "max-|bake| x"),
                  "r3 hot detail bake");
            CHECK(ac4enc_reset(c) == 0, "r3 hot reset ok");
            CHECK(ac4enc_err_sub(c) == AC4ENC_EOK,
                  "r3 hot reset clears sub");
            CHECK(ac4enc_err_detail(c)[0] == '\0',
                  "r3 hot reset clears detail");
            ac4enc_close(c);
            c = NULL;
        }
        got = enc_core_encode_pcm(L, ns, R, ns, 128,
                                  "stereo", "1sec", "zero",
                                  NULL, 1, 1, -24.0, 0, 0.0,
                                  0, NULL, 0, "manual", NULL,
                                  NULL, 1, 0, -1, blob,
                                  sizeof blob);
        CHECK(got == -1, "r3 hot core refuses");
        e = enc_core_last_error();
        CHECK(strstr(e, ": baked peak refused (baked="),
              "r3 hot core names gate");
        CHECK(strstr(e, "in_peak=20000)"),
              "r3 hot core in_peak");
        /* Control: no correction -> no guard trip. */
        CHECK(open_measured(&c, rows, 2, ns, "stereo", 128,
                            0.0, 0, 0.0) == 0,
              "r3 hot t0 open");
        if (c) {
            CHECK(drive_int(c, rows, 2, ns, ob, sizeof ob,
                            &nfr) == 0,
                  "r3 hot t0 encodes");
            CHECK(nfr == 6, "r3 hot t0 emits 6 frames");
            ac4enc_close(c);
        }
        free(L);
        free(R);
    }
    /* R3-3 mc: hot legs refuse loud (pk gate). */
    {
        int ns = 4096;
        int *rows[6];
        ac4enc_ctx_t *c = NULL;
        const int *crows[6];
        int lens[6];
        int nfr = 0, got, ok = 1;
        const char *d, *e;
        for (int ch = 0; ch < 6; ch++) {
            rows[ch] =
                malloc((size_t)ns * sizeof *rows[ch]);
            if (!rows[ch])
                ok = 0;
        }
        CHECK(ok, "r3 mc alloc");
        if (!ok) {
            for (int ch = 0; ch < 6; ch++)
                free(rows[ch]);
            return;
        }
        for (int ch = 0; ch < 6; ch++) {
            for (int k = 0; k < ns; k++)
                rows[ch][k] = (int)round(16383.0 *
                    sin(2.0 * 3.141592653589793 *
                        440.0 * k / 48000.0 +
                        ch * 0.7));
            crows[ch] = rows[ch];
            lens[ch] = ns;
        }
        CHECK(open_measured(&c, rows, 6, ns, "5.1", 128,
                            -24.0, 1, 12.0) == 0,
              "r3 mc lib open");
        if (c) {
            CHECK(drive_int(c, rows, 6, ns, ob, sizeof ob,
                            &nfr) == -EIO,
                  "r3 mc lib refuses");
            CHECK(ac4enc_err_sub(c) == AC4ENC_EBAKE_PEAK,
                  "r3 mc sub peak");
            d = ac4enc_err_detail(c);
            CHECK(strstr(d, "baked peak gate: baked_peak="),
                  "r3 mc detail names gate");
            ac4enc_close(c);
            c = NULL;
        }
        got = enc_core_encode_pcm_mc(crows, lens, 128,
                                     "1sec", "zero", NULL,
                                     -24.0, 1, 12.0, 1, NULL,
                                     NULL, NULL, 0, "manual",
                                     NULL, NULL, blob,
                                     sizeof blob);
        CHECK(got == -1, "r3 mc core refuses");
        e = enc_core_last_error();
        CHECK(strstr(e, "enc_core_encode_pcm_mc: baked peak "
                        "refused (baked="),
              "r3 mc core names gate");
        CHECK(open_measured(&c, rows, 6, ns, "5.1", 128,
                            0.0, 0, 0.0) == 0,
              "r3 mc t0 open");
        if (c) {
            CHECK(drive_int(c, rows, 6, ns, ob, sizeof ob,
                            &nfr) == 0,
                  "r3 mc t0 encodes");
            CHECK(nfr == 4, "r3 mc t0 emits 4 frames");
            ac4enc_close(c);
        }
        for (int ch = 0; ch < 6; ch++)
            free(rows[ch]);
    }
    /* bake_round (int32) refusal names ch/idx/value. */
    {
        ac4enc_ctx_t *c = NULL;
        ac4enc_opts_t o;
        int pcm[2048];
        int *rows[2] = { pcm, pcm };
        int nfr = 0;
        const char *d;
        memset(&o, 0, sizeof o);
        memset(pcm, 0, sizeof pcm);
        o.layout = "stereo";
        o.rate_kbps = 64;
        o.ns2_tone = 1;
        o.shorts = 1;
        o.priming = 1;
        o.iframe_total = -1;
        o.target_db = -24.0;
        o.has_bake = 1;
        o.bake_db = 40.0;
        o.has_measure = 1;
        o.m_db = -20.0;
        o.f_flat = 0.1;
        o.tf_tone = 0.9;
        o.in_peak = 100;
        CHECK(ac4enc_open(&c, &o) == 0, "r3 ebake open");
        pcm[0] = INT_MAX;
        if (c) {
            CHECK(drive_int(c, rows, 2, 2048, ob,
                            sizeof ob, &nfr) == -EIO,
                  "r3 ebake refuses");
            CHECK(ac4enc_err_sub(c) ==
                      AC4ENC_EBAKE_OVERFLOW,
                  "r3 ebake sub overflow");
            d = ac4enc_err_detail(c);
            CHECK(strstr(d, "bake_round gate: int32 "
                            "overflow at ch=0 idx=0"),
                  "r3 ebake detail locus");
            CHECK(strstr(d, "x=2147483647"),
                  "r3 ebake detail value");
            ac4enc_close(c);
        }
    }
    /* Detail lifecycle: NULL-safe, empty unless a bake
     * refusal is the sticky error. */
    CHECK(!strcmp(ac4enc_err_detail(NULL), ""),
          "r3 detail null safe");
}

/* m1: INT_MIN magnitudes saturate (no abs() UB). */
static void
t_absmin(void)
{
    int L[4] = { 0, INT_MIN, 100, -200 };
    int R[4] = { 1, 2, 3, INT_MIN };
    const int *rr[2] = { L, R };
    int lens[2] = { 4, 4 };
    int M[6][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 },
                    { 0, 0 }, { 0, 0 }, { INT_MIN, 5 } };
    const int *mr[6];
    int mlens[6];
    ac4enc_measure_t m;
    CHECK(enc_core_abs_sat(0) == 0, "abssat 0");
    CHECK(enc_core_abs_sat(-5) == 5, "abssat neg");
    CHECK(enc_core_abs_sat(INT_MAX) == INT_MAX,
          "abssat max");
    CHECK(enc_core_abs_sat(INT_MIN) == INT_MAX,
          "abssat min");
    CHECK(ac4enc_measure2(rr, lens, 2, &m) == 0,
          "absmin stereo ok");
    CHECK(m.in_peak == INT_MAX, "absmin stereo sat");
    for (int c = 0; c < 6; c++) {
        mr[c] = M[c];
        mlens[c] = 2;
    }
    CHECK(ac4enc_measure2(mr, mlens, 6, &m) == 0,
          "absmin mc ok");
    CHECK(m.in_peak == INT_MAX, "absmin mc sat");
}

/* m9: empty stream (0 samples fed) is valid: flush drains the
 * priming plan (2 frames), fed stays 0. Bytes match the core
 * neutral wrapper on empty input. */
static void
t_empty(void)
{
    static uint8_t libb[AC4ENC_OUTCAP_SAFE * 2];
    static uint8_t libb2[AC4ENC_OUTCAP_SAFE * 2];
    static uint8_t core[1 << 20], coreb[1 << 20];
    static uint8_t ob[AC4ENC_OUTCAP_SAFE];
    static int z[1];
    static const char *layouts[2] = { "stereo", "5.1" };
    static const int rates[2] = { 64, 128 };
    CHECK(ac4enc_fed_samples(NULL) == -1, "m2 null fed");
    CHECK(ac4enc_emitted_frames(NULL) == -1,
          "m2 null emitted");
    for (int li = 0; li < 2; li++) {
        ac4enc_ctx_t *c = NULL;
        ac4enc_opts_t o;
        size_t wpos = 0, wpos2 = 0;
        int flags[8], nf = 0, frc, coren, cnf = 0, nb;
        int nch = li == 0 ? 2 : 6;
        memset(&o, 0, sizeof o);
        o.layout = layouts[li];
        o.rate_kbps = rates[li];
        o.interval = "1sec";
        o.ns2_tone = 1;
        o.shorts = 1;
        o.priming = 1;
        o.iframe_total = -1;
        o.target_db = 0.0;
        CHECK(ac4enc_open(&c, &o) == 0, "empty open");
        CHECK(ac4enc_fed_samples(c) == 0, "empty fed 0");
        CHECK(ac4enc_emitted_frames(c) == 0,
              "empty emitted 0");
        for (;;) {
            size_t olen = 0;
            int is_if = 0;
            frc = ac4enc_flush2(c, ob, sizeof ob, &olen,
                                &is_if);
            if (frc <= 0)
                break;
            if (wpos + olen > sizeof libb || nf >= 8) {
                CHECK(0, "empty overflow");
                break;
            }
            memcpy(libb + wpos, ob, olen);
            wpos += olen;
            flags[nf++] = is_if;
        }
        CHECK(frc == 0, "empty drains");
        CHECK(nf == 2, "empty 2 frames");
        CHECK(nf == 2 && flags[0] == 1 && flags[1] == 1,
              "empty priming iframes");
        CHECK(ac4enc_fed_samples(c) == 0,
              "empty fed stays 0");
        CHECK(ac4enc_emitted_frames(c) == nf,
              "empty emitted 2");
        /* Stable done: drain again -> 0 immediately. */
        {
            size_t olen = 0;
            CHECK(ac4enc_flush2(c, ob, sizeof ob, &olen,
                                NULL) == 0,
                  "empty stable done");
        }
        /* Core twin on empty input (neutral, ms==NULL). */
        if (nch == 2) {
            nb = enc_core_encode_pcm_m(z, 0, z, 0, rates[li],
                                       "stereo", "1sec",
                                       "zero", NULL, 1, 1,
                                       0.0, 0, 0.0, 0, NULL,
                                       0, "manual", NULL,
                                       NULL, 1, 0, -1, NULL,
                                       core, sizeof core);
        } else {
            const int *chs[6];
            int lens[6];
            for (int k = 0; k < 6; k++) {
                chs[k] = z;
                lens[k] = 0;
            }
            nb = enc_core_encode_pcm_mc_m(
                chs, lens, rates[li], "1sec", "zero", NULL,
                0.0, 0, 0.0, 1, NULL, 0, "manual", NULL,
                NULL, NULL, core, sizeof core);
        }
        CHECK(nb > 0, "empty core encodes");
        coren = strip_blob(core, nb, coreb, sizeof coreb,
                           &cnf);
        CHECK(coren == (int)wpos && cnf == nf &&
                  !memcmp(libb, coreb, wpos),
              "empty bytes vs core");
        /* Reset repeats identically. */
        CHECK(ac4enc_reset(c) == 0, "empty reset");
        CHECK(ac4enc_fed_samples(c) == 0,
              "empty reset fed");
        for (;;) {
            size_t olen = 0;
            int is_if = 0;
            frc = ac4enc_flush2(c, ob, sizeof ob, &olen,
                                &is_if);
            if (frc <= 0)
                break;
            if (wpos2 + olen > sizeof libb2)
                break;
            memcpy(libb2 + wpos2, ob, olen);
            wpos2 += olen;
        }
        CHECK(frc == 0 && wpos2 == wpos &&
                  !memcmp(libb, libb2, wpos),
              "empty reset same bytes");
        ac4enc_close(c);
    }
    /* Measured-empty (target 0) drains the same plan. */
    {
        ac4enc_ctx_t *c = NULL;
        ac4enc_opts_t o;
        const int *rr[2] = { z, z };
        int lens[2] = { 0, 0 };
        ac4enc_measure_t m;
        int nf = 0, frc;
        CHECK(ac4enc_measure2(rr, lens, 2, &m) == 0,
              "empty measure ok");
        memset(&o, 0, sizeof o);
        o.layout = "stereo";
        o.rate_kbps = 64;
        o.interval = "1sec";
        o.ns2_tone = 1;
        o.shorts = 1;
        o.priming = 1;
        o.iframe_total = -1;
        o.target_db = 0.0;
        o.has_measure = 1;
        o.m_db = m.m_db;
        o.f_flat = m.f_flat;
        o.tf_tone = m.tf_tone;
        o.in_peak = m.in_peak;
        o.d_peak = m.d_peak;
        o.has_d_peak = m.has_d_peak;
        CHECK(ac4enc_open(&c, &o) == 0,
              "empty measured open");
        for (;;) {
            size_t olen = 0;
            frc = ac4enc_flush2(c, ob, sizeof ob, &olen,
                                NULL);
            if (frc <= 0)
                break;
            nf++;
        }
        CHECK(frc == 0 && nf == 2, "empty measured 2");
        CHECK(ac4enc_fed_samples(c) == 0,
              "empty measured fed 0");
        ac4enc_close(c);
    }
}

/* S5FU: stereo spectral staging is x2.0 (DEE), not x3.0.
 * The uniform 3.0 decoded every stereo file exactly 3/2
 * (+3.52 dB) hot vs DEE at the same dialnorm (G7 997Hz:
 * ours -26.56 vs DEE/DRP -30.12/-30.17); 5.1 stages x3.0
 * on both sides (G5 match). Pins the constants, the legs
 * staging ratio on a 997Hz tone, and end-to-end lib bytes. */
static unsigned long long
s5fu_fnv(const uint8_t *b, size_t n)
{
    unsigned long long h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) {
        h ^= (unsigned long long)b[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static void
t_s5fu(void)
{
    static double dL[24000], dR[24000];
    static int iL[24000], iR[24000];
    static double win[4096];
    static uint8_t libb[1 << 20];
    static int lines[7];
    const int *pi[2] = { iL, iR };
    int lens[2] = { 24000, 24000 };
    int flags[64], nl = 0, sl[7], librc, libn, pinmax = 0;
    ac4enc_opts_t o;
    ac4enc_measure_t m;
    enc_core_leg_t leg;
    double g, ratio = 0.0, qpeak = 0.0;
    int Lpeak = 0;
    CHECK(ENC_CORE_DIG_GAIN_STEREO == 2.0,
          "s5fu stereo gain 2.0");
    CHECK(ENC_CORE_DIG_GAIN == 3.0, "s5fu mc gain 3.0");
    for (int k = 0; k < 24000; k++) {
        double v = 16384.0 *
            sin(2.0 * 3.141592653589793 * 997.0 * k /
                48000.0);
        dL[k] = v;
        dR[k] = v;
        iL[k] = (int)round(v);
        iR[k] = (int)round(v);
    }
    mdct_kbd_window(4096, 3.0, win);
    nl = mdct_lines_for_freq(
        mdct_estimate_freq(dL, 24000, 50.0, 20000.0), 3,
        lines);
    CHECK(nl == 7, "s5fu 7 lines");
    CHECK(enc_core_quantize_frame(dL, 24000, 10 * 2048,
                                  lines, nl, win,
                                  ENC_CORE_DIG_GAIN_STEREO,
                                  &leg) == 0,
          "s5fu legs ok");
    CHECK(leg.n > 0, "s5fu legs nonempty");
    for (int k = 0; k < leg.n; k++)
        if (fabs((double)leg.qs[k]) > fabs(qpeak)) {
            qpeak = (double)leg.qs[k];
            Lpeak = leg.lines[k];
        }
    g = enc_core_gain_for_ref(leg.ref);
    {
        double xf = mdct_forward_direct_1(dL, 24000,
                                          10 * 2048, 2048,
                                          Lpeak, win);
        double xemit = copysign(pow(fabs(qpeak), 4.0 / 3.0),
                                qpeak) * g;
        if (xf != 0.0)
            ratio = xemit / xf;
    }
    CHECK(ratio > 1.995 && ratio < 2.005,
          "s5fu staging x2.0");
    CHECK(ac4enc_measure2(pi, lens, 2, &m) == 0,
          "s5fu measure");
    memset(&o, 0, sizeof o);
    o.layout = "stereo";
    o.rate_kbps = 64;
    o.interval = "1sec";
    o.ns2_tone = 1;
    o.shorts = 1;
    o.priming = 1;
    o.iframe_total = -1;
    o.target_db = -24.0;
    o.has_measure = 1;
    o.m_db = m.m_db;
    o.f_flat = m.f_flat;
    o.tf_tone = m.tf_tone;
    o.in_peak = m.in_peak;
    o.d_peak = m.d_peak;
    o.has_d_peak = m.has_d_peak;
    if (ac4enc_spectral_lines(dL, 24000, &m, -24.0, 0, 0.0,
                              &nl, sl) == 0) {
        o.has_spec_lines = 1;
        o.n_spec_lines = nl;
        memcpy(o.spec_lines, sl, sizeof sl);
    }
    librc = lib_run(&o, pi, NULL, 2, 24000, libb,
                    sizeof libb, flags, 64, &pinmax);
    CHECK(librc >= 0, "s5fu lib encodes");
    libn = librc & 0xFFFFFF;
    CHECK(libn == 4779, "s5fu 4779 bytes");
    CHECK(s5fu_fnv(libb, (size_t)libn) ==
              0xea3fb9da0725054dULL,
          "s5fu golden");
}

int
main(void)
{
    t_misuse();
    t_equiv();
    t_sparse();
    t_open_neg2();
    t_diag();
    t_fp();
    t_reset();
    t_maxpacket();
    t_prim0();
    t_concurrent();
    t_increquiv();
    t_r3();
    t_absmin();
    t_empty();
    t_s5fu();
    if (!fails)
        printf("unit: all pass\n");
    return fails ? 1 : 0;
}
