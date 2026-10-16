/* unit.c: API-misuse negatives + _m-vs-original equivalence.
 * Exit 0 all pass; prints FAIL lines otherwise. Built to scratch. */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ac4enc_lib.h"
#include "../src/enc_core.h"

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

int
main(void)
{
    t_misuse();
    t_equiv();
    if (!fails)
        printf("unit: all pass\n");
    return fails ? 1 : 0;
}
