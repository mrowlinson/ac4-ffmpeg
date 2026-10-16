/* encharness: drive ac4enc_lib over a file, dump bare frames.
 * Usage: encharness --in F --fmt wav|pcm_s16le|pcm_s24le|pcm_f32le
 *   --ch N --layout stereo|5.1 --rate KBPS [--interval S]
 *   [--target-db T] [--bake-db B] [--shorts 0|1] [--joint-shorts 0|1]
 *   [--priming 0|1] [--forced i,j,k] [--preset NAME]
 *   [--fold none|8stereo|51stereo|851] [--force-float] [--neutral]
 *   --frames-out FB --meta-out MT
 * --forced takes CORE frame indices (what the CLI derives after
 * its csvfile k+1 shift: user index u -> core index u+1).
 * Exit: 0 ok, 1 lib/encode error, 2 usage/io error.
 * frames-out: concatenated bare frames. meta-out: measure line +
 * one "frame i len iframe" line per frame.
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ac4enc_lib.h"
#include "../src/enc_core.h"
#include "../src/pycompat.h"

static void
usage(void)
{
    fprintf(stderr,
            "usage: encharness --in F --fmt FMT --ch N "
            "--layout L --rate K [opts] --frames-out FB "
            "--meta-out MT\n");
}

static const char *
argval(int argc, char **argv, const char *key)
{
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], key))
            return argv[i + 1];
    return NULL;
}

static int
argflag(int argc, char **argv, const char *key)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], key))
            return 1;
    return 0;
}

static int
parse_int(const char *s, int *out)
{
    char *end = NULL;
    long v;
    if (!s)
        return -1;
    errno = 0;
    v = strtol(s, &end, 10);
    if (errno || !end || *end || v < INT32_MIN || v > INT32_MAX)
        return -1;
    *out = (int)v;
    return 0;
}

static int
parse_double(const char *s, double *out)
{
    char *end = NULL;
    double v;
    if (!s)
        return -1;
    errno = 0;
    v = strtod(s, &end);
    if (errno || !end || *end)
        return -1;
    *out = v;
    return 0;
}

static uint16_t
u16le(const uint8_t *b)
{
    return (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
}

static uint32_t
u32le(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
        ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static int32_t
s16le(const uint8_t *b)
{
    uint16_t u = u16le(b);
    return u >= 0x8000 ? (int32_t)u - 0x10000 : (int32_t)u;
}

static uint8_t *
read_file(const char *path, size_t *len_out)
{
    FILE *f = NULL;
    uint8_t *buf = NULL;
    size_t len = 0, cap = 0;
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open %s\n", path);
        return NULL;
    }
    for (;;) {
        size_t n;
        if (len == cap) {
            size_t ncap = cap ? cap * 2 : 65536;
            uint8_t *nb = realloc(buf, ncap);
            if (!nb) {
                free(buf);
                fclose(f);
                return NULL;
            }
            buf = nb;
            cap = ncap;
        }
        n = fread(buf + len, 1, cap - len, f);
        len += n;
        if (n == 0)
            break;
    }
    fclose(f);
    *len_out = len;
    return buf;
}

/* Minimal RIFF s16 parse. Returns ch-major ints. */
static int
read_wav_s16(const char *path, int want_ch, int **chs_out,
             int *ns_out)
{
    uint8_t *raw = NULL;
    size_t len = 0;
    size_t pos;
    int have_fmt = 0, nch = 0, rate = 0, bits = 0, fmt = 0;
    const uint8_t *data = NULL;
    size_t data_len = 0;
    int ns = 0;
    int *chs = NULL;
    raw = read_file(path, &len);
    if (!raw)
        return -1;
    if (len < 12 || memcmp(raw, "RIFF", 4) ||
        memcmp(raw + 8, "WAVE", 4)) {
        fprintf(stderr, "error: %s: not a RIFF/WAVE file\n",
                path);
        free(raw);
        return -1;
    }
    pos = 12;
    while (pos + 8 <= len) {
        uint32_t sz = u32le(raw + pos + 4);
        size_t body = pos + 8;
        if (body + sz > len)
            break;
        if (!memcmp(raw + pos, "fmt ", 4)) {
            if (sz < 16) {
                fprintf(stderr, "error: %s: bad fmt chunk\n",
                        path);
                free(raw);
                return -1;
            }
            fmt = u16le(raw + body);
            nch = u16le(raw + body + 2);
            rate = (int)u32le(raw + body + 4);
            bits = u16le(raw + body + 14);
            have_fmt = 1;
        } else if (!memcmp(raw + pos, "data", 4)) {
            data = raw + body;
            data_len = sz;
        }
        pos = body + sz + (sz & 1);
    }
    if (!have_fmt || !data) {
        fprintf(stderr, "error: %s: missing fmt/data\n",
                path);
        free(raw);
        return -1;
    }
    if (fmt != 1 || bits != 16) {
        fprintf(stderr, "error: %s: only PCM s16 wav "
                        "(fmt=%d bits=%d)\n",
                path, fmt, bits);
        free(raw);
        return -1;
    }
    if (rate != 48000) {
        fprintf(stderr, "error: %s: rate %d != 48000\n", path,
                rate);
        free(raw);
        return -1;
    }
    if (nch != want_ch) {
        fprintf(stderr, "error: %s: nch %d != %d\n", path,
                nch, want_ch);
        free(raw);
        return -1;
    }
    ns = (int)(data_len / ((size_t)nch * 2));
    chs = malloc((size_t)(ns > 0 ? ns : 1) *
                 (size_t)nch * sizeof *chs);
    if (!chs) {
        free(raw);
        return -1;
    }
    for (int s = 0; s < ns; s++)
        for (int c = 0; c < nch; c++)
            chs[c * ns + s] =
                s16le(data + ((size_t)s * (size_t)nch +
                              (size_t)c) * 2);
    free(raw);
    *chs_out = chs;
    *ns_out = ns;
    return 0;
}

static long long
floor_div_256(long long v)
{
    if (v >= 0)
        return v / 256;
    return -((-v + 255) / 256);
}

static int
read_raw(const char *path, const char *fmt, int want_ch,
         int **chs_out, int *ns_out)
{
    uint8_t *raw = NULL;
    size_t len = 0;
    int bps = 0, code = 0;
    int ns = 0;
    int *chs = NULL;
    raw = read_file(path, &len);
    if (!raw)
        return -1;
    if (!strcmp(fmt, "pcm_s16le")) {
        bps = 2;
        code = 0;
    } else if (!strcmp(fmt, "pcm_s24le")) {
        bps = 3;
        code = 1;
    } else if (!strcmp(fmt, "pcm_f32le")) {
        bps = 4;
        code = 2;
    } else {
        fprintf(stderr, "error: bad fmt %s\n", fmt);
        free(raw);
        return -1;
    }
    if (len % ((size_t)bps * (size_t)want_ch)) {
        fprintf(stderr, "error: %s: ragged raw size\n",
                path);
        free(raw);
        return -1;
    }
    ns = (int)(len / ((size_t)bps * (size_t)want_ch));
    chs = malloc((size_t)(ns > 0 ? ns : 1) *
                 (size_t)want_ch * sizeof *chs);
    if (!chs) {
        free(raw);
        return -1;
    }
    for (int s = 0; s < ns; s++) {
        for (int c = 0; c < want_ch; c++) {
            size_t o = ((size_t)s * (size_t)want_ch +
                        (size_t)c) * (size_t)bps;
            if (code == 0) {
                chs[c * ns + s] = s16le(raw + o);
            } else if (code == 1) {
                long long v = (long long)raw[o] |
                    ((long long)raw[o + 1] << 8) |
                    ((long long)raw[o + 2] << 16);
                if (v >= 0x800000LL)
                    v -= 0x1000000LL;
                chs[c * ns + s] =
                    (int)floor_div_256(v);
            } else {
                float f = 0;
                double d = 0;
                long long r = 0;
                memcpy(&f, raw + o, 4);
                d = (double)f * 32768.0;
                if (isnan(d) || isinf(d)) {
                    fprintf(stderr, "error: %s: non-finite "
                                    "f32\n",
                            path);
                    free(chs);
                    free(raw);
                    return -1;
                }
                if (d > 8e15)
                    r = 32767;
                else if (d < -8e15)
                    r = -32768;
                else {
                    if (pycompat_round_half_even_ll(d,
                                                    &r)) {
                        free(chs);
                        free(raw);
                        return -1;
                    }
                    if (r > 32767)
                        r = 32767;
                    else if (r < -32768)
                        r = -32768;
                }
                chs[c * ns + s] = (int)r;
            }
        }
    }
    free(raw);
    *chs_out = chs;
    *ns_out = ns;
    return 0;
}

int
main(int argc, char **argv)
{
    const char *in = argval(argc, argv, "--in");
    const char *fmt = argval(argc, argv, "--fmt");
    const char *ch_s = argval(argc, argv, "--ch");
    const char *layout = argval(argc, argv, "--layout");
    const char *rate_s = argval(argc, argv, "--rate");
    const char *interval = argval(argc, argv, "--interval");
    const char *target_s = argval(argc, argv, "--target-db");
    const char *bake_s = argval(argc, argv, "--bake-db");
    const char *shorts_s = argval(argc, argv, "--shorts");
    const char *jshorts_s =
        argval(argc, argv, "--joint-shorts");
    const char *priming_s = argval(argc, argv, "--priming");
    const char *forced_s = argval(argc, argv, "--forced");
    const char *preset = argval(argc, argv, "--preset");
    const char *fold = argval(argc, argv, "--fold");
    const char *frames_out =
        argval(argc, argv, "--frames-out");
    const char *meta_out = argval(argc, argv, "--meta-out");
    int force_float = argflag(argc, argv, "--force-float");
    int neutral = argflag(argc, argv, "--neutral");
    int nch_in = 0, rate = 0;
    int *ich = NULL;
    int ns = 0;
    int nch = 0, out_ch = 0;
    double *drows = NULL;
    int *irows = NULL;
    int use_float = 0;
    ac4enc_opts_t opts;
    ac4enc_ctx_t *ctx = NULL;
    FILE *ff = NULL, *fm = NULL;
    int *forced = NULL, nforced = 0;
    int rc = 2;
    if (!in || !fmt || !ch_s || !layout || !rate_s ||
        !frames_out || !meta_out || parse_int(ch_s, &nch_in) ||
        parse_int(rate_s, &rate) || nch_in <= 0) {
        usage();
        return 2;
    }
    if (strcmp(fmt, "wav") && strcmp(fmt, "pcm_s16le") &&
        strcmp(fmt, "pcm_s24le") && strcmp(fmt, "pcm_f32le")) {
        usage();
        return 2;
    }
    if (!strcmp(layout, "stereo"))
        out_ch = 2;
    else if (!strcmp(layout, "5.1"))
        out_ch = 6;
    else {
        usage();
        return 2;
    }
    if (!fold)
        fold = "none";
    if (strcmp(fold, "none") && strcmp(fold, "8stereo") &&
        strcmp(fold, "51stereo") && strcmp(fold, "851")) {
        usage();
        return 2;
    }
    if (strcmp(fold, "none") && force_float) {
        fprintf(stderr, "error: --fold with --force-float\n");
        return 2;
    }
    if (!strcmp(fmt, "wav")) {
        if (read_wav_s16(in, nch_in, &ich, &ns))
            return 2;
    } else {
        if (read_raw(in, fmt, nch_in, &ich, &ns))
            return 2;
    }
    memset(&opts, 0, sizeof opts);
    opts.layout = layout;
    opts.rate_kbps = rate;
    opts.interval = interval;
    opts.preset = preset;
    opts.ns2_tone = 1;
    opts.shorts = 1;
    opts.joint_shorts = 0;
    opts.priming = 1;
    opts.iframe_base = 0;
    opts.iframe_total = -1;
    opts.target_db = -24.0;
    if (target_s && parse_double(target_s, &opts.target_db)) {
        fprintf(stderr, "error: bad --target-db\n");
        goto done;
    }
    if (bake_s) {
        if (parse_double(bake_s, &opts.bake_db)) {
            fprintf(stderr, "error: bad --bake-db\n");
            goto done;
        }
        opts.has_bake = 1;
    }
    if (shorts_s && parse_int(shorts_s, &opts.shorts))
        goto bad;
    if (jshorts_s && parse_int(jshorts_s, &opts.joint_shorts))
        goto bad;
    if (priming_s && parse_int(priming_s, &opts.priming))
        goto bad;
    if (forced_s) {
        /* Count commas. */
        nforced = 1;
        for (const char *p = forced_s; *p; p++)
            if (*p == ',')
                nforced++;
        forced = malloc((size_t)nforced * sizeof *forced);
        if (!forced)
            goto done;
        {
            char *dup = malloc(strlen(forced_s) + 1);
            char *tok = NULL;
            int k = 0;
            if (!dup)
                goto done;
            strcpy(dup, forced_s);
            tok = strtok(dup, ",");
            while (tok && k < nforced) {
                if (parse_int(tok, &forced[k])) {
                    free(dup);
                    fprintf(stderr,
                            "error: bad --forced\n");
                    goto done;
                }
                k++;
                tok = strtok(NULL, ",");
            }
            free(dup);
            nforced = k;
        }
        opts.forced = forced;
        opts.nforced = nforced;
    }
    /* Rows: fold (float) / force-float / direct int. */
    if (!strcmp(fold, "none") && !force_float) {
        if (nch_in != out_ch) {
            fprintf(stderr, "error: ch %d != layout ch %d "
                            "(need --fold?)\n",
                    nch_in, out_ch);
            goto done;
        }
        nch = out_ch;
        irows = ich;
        ich = NULL;
        use_float = 0;
    } else if (force_float) {
        const int *rr[6];
        ac4enc_measure_t m;
        if (nch_in != out_ch) {
            fprintf(stderr, "error: ch %d != layout ch %d\n",
                    nch_in, out_ch);
            goto done;
        }
        nch = out_ch;
        drows = malloc((size_t)(ns > 0 ? ns : 1) *
                       (size_t)nch * sizeof *drows);
        if (!drows)
            goto done;
        for (int c = 0; c < nch; c++)
            for (int k = 0; k < ns; k++)
                drows[c * ns + k] = (double)ich[c * ns + k];
        for (int c = 0; c < nch; c++)
            rr[c] = NULL;
        (void)m;
        use_float = 1;
    } else {
        const double *src[8];
        double *tmp = NULL;
        double *dst = NULL;
        int got = 0;
        int want_in = !strcmp(fold, "51stereo") ? 6 : 8;
        if (nch_in != want_in) {
            fprintf(stderr, "error: fold %s needs %d ch\n",
                    fold, want_in);
            goto done;
        }
        nch = out_ch;
        tmp = malloc((size_t)(ns > 0 ? ns : 1) * 8 *
                     sizeof *tmp);
        dst = malloc((size_t)(ns > 0 ? ns : 1) *
                     (size_t)nch * sizeof *dst);
        if (!tmp || !dst) {
            free(tmp);
            free(dst);
            goto done;
        }
        for (int c = 0; c < nch_in; c++) {
            for (int k = 0; k < ns; k++)
                tmp[c * ns + k] =
                    (double)ich[c * ns + k];
            src[c] = tmp + c * ns;
        }
        if (!strcmp(fold, "8stereo")) {
            double *od[2] = { dst, dst + ns };
            if (out_ch != 2) {
                fprintf(stderr, "error: 8stereo needs "
                                "stereo layout\n");
                free(tmp);
                free(dst);
                goto done;
            }
            got = enc_core_fold_8ch_to_stereo(src,
                                              (const int[]){
                                                  ns, ns,
                                                  ns, ns,
                                                  ns, ns,
                                                  ns, ns },
                                              od);
        } else if (!strcmp(fold, "51stereo")) {
            double *od[2] = { dst, dst + ns };
            if (out_ch != 2) {
                fprintf(stderr, "error: 51stereo needs "
                                "stereo layout\n");
                free(tmp);
                free(dst);
                goto done;
            }
            got = enc_core_mix51_to_stereo(
                src,
                (const int[]){ ns, ns, ns, ns, ns, ns },
                od);
        } else {
            double *od[6];
            if (out_ch != 6) {
                fprintf(stderr, "error: 851 needs 5.1 "
                                "layout\n");
                free(tmp);
                free(dst);
                goto done;
            }
            for (int c = 0; c < 6; c++)
                od[c] = dst + c * ns;
            got = enc_core_fold_8ch_to_51(
                src,
                (const int[]){ ns, ns, ns, ns, ns, ns, ns,
                               ns },
                od);
        }
        free(tmp);
        if (got != ns) {
            fprintf(stderr, "error: fold returned %d != %d\n",
                    got, ns);
            free(dst);
            goto done;
        }
        drows = dst;
        use_float = 1;
    }
    if (!neutral) {
        int lens[8];
        ac4enc_measure_t m;
        int mrc;
        for (int c = 0; c < nch; c++)
            lens[c] = ns;
        if (use_float) {
            const double *rr[6];
            for (int c = 0; c < nch; c++)
                rr[c] = drows + c * ns;
            mrc = ac4enc_measure2_f(rr, lens, nch, &m);
        } else {
            const int *rr[6];
            for (int c = 0; c < nch; c++)
                rr[c] = irows + c * ns;
            mrc = ac4enc_measure2(rr, lens, nch, &m);
        }
        if (mrc) {
            fprintf(stderr, "error: measure failed %d\n",
                    mrc);
            goto done;
        }
        opts.has_measure = 1;
        opts.m_db = m.m_db;
        opts.f_flat = m.f_flat;
        opts.tf_tone = m.tf_tone;
        opts.in_peak = m.in_peak;
        opts.d_peak = m.d_peak;
        opts.has_d_peak = m.has_d_peak;
        fm = fopen(meta_out, "w");
        if (!fm) {
            fprintf(stderr, "error: cannot write %s\n",
                    meta_out);
            goto done;
        }
        fprintf(fm, "measure m=%.17g f=%.17g tf=%.17g "
                    "peak=%d dpeak=%.17g hasd=%d\n",
                m.m_db, m.f_flat, m.tf_tone, m.in_peak,
                m.d_peak, m.has_d_peak);
        if (nch == 2) {
            /* H6 pass over the L row (mc ignores lines). */
            const double *Lr = NULL;
            double *Lc = NULL;
            int nl = 0, sl[7], slrc;
            if (use_float) {
                Lr = drows;
            } else {
                Lc = malloc((size_t)(ns > 0 ? ns : 1) *
                            sizeof *Lc);
                if (!Lc)
                    goto done;
                for (int k = 0; k < ns; k++)
                    Lc[k] = (double)irows[k];
                Lr = Lc;
            }
            slrc = ac4enc_spectral_lines(
                Lr, ns, &m, opts.target_db,
                opts.has_bake, opts.bake_db, &nl, sl);
            free(Lc);
            if (slrc) {
                fprintf(stderr,
                        "error: spectral lines rc=%d\n",
                        slrc);
                goto done;
            }
            opts.has_spec_lines = 1;
            opts.n_spec_lines = nl;
            memcpy(opts.spec_lines, sl, sizeof sl);
            fprintf(fm, "speclines n=%d\n", nl);
        }
    } else {
        fm = fopen(meta_out, "w");
        if (!fm) {
            fprintf(stderr, "error: cannot write %s\n",
                    meta_out);
            goto done;
        }
        fprintf(fm, "measure neutral\n");
    }
    if (ac4enc_open(&ctx, &opts)) {
        fprintf(stderr, "error: ac4enc_open failed\n");
        goto done;
    }
    ff = fopen(frames_out, "wb");
    if (!ff) {
        fprintf(stderr, "error: cannot write %s\n",
                frames_out);
        goto done;
    }
    {
        int full = ns / AC4ENC_FRAME_SAMPLES;
        int rem = ns % AC4ENC_FRAME_SAMPLES;
        int idx = 0, maxp, f;
        size_t olen = 0;
        int is_if = 0, erc;
        uint8_t *pkt = NULL;
        maxp = ac4enc_max_packet(rate);
        if (maxp <= 0) {
            fprintf(stderr, "error: bad rate %d\n", rate);
            goto done;
        }
        pkt = malloc((size_t)maxp);
        if (!pkt)
            goto done;
        for (f = 0; f < full; f++) {
            if (use_float) {
                const double *rr[6];
                for (int c = 0; c < nch; c++)
                    rr[c] = drows + c * ns +
                        f * AC4ENC_FRAME_SAMPLES;
                erc = ac4enc_encode_frame_f2(ctx, rr, pkt,
                                             (size_t)maxp,
                                             &olen, &is_if);
            } else {
                const int *rr[6];
                for (int c = 0; c < nch; c++)
                    rr[c] = irows + c * ns +
                        f * AC4ENC_FRAME_SAMPLES;
                erc = ac4enc_encode_frame2(ctx, rr, pkt,
                                           (size_t)maxp,
                                           &olen, &is_if);
            }
            if (erc < 0) {
                fprintf(stderr, "error: encode frame %d "
                                "rc=%d (%s/%s)\n",
                        f, erc,
                        ac4enc_subcode_str(
                            ac4enc_err_sub(ctx)),
                        ac4enc_err_stage(ctx));
                free(pkt);
                goto done;
            }
            if (erc == 1) {
                if (fwrite(pkt, 1, olen, ff) != olen) {
                    fprintf(stderr,
                            "error: frames write\n");
                    free(pkt);
                    goto done;
                }
                fprintf(fm, "frame %d len %lu iframe %d\n",
                        idx, (unsigned long)olen, is_if);
                idx++;
            }
        }
        if (rem > 0) {
            if (use_float) {
                const double *rr[6];
                for (int c = 0; c < nch; c++)
                    rr[c] = drows + c * ns +
                        full * AC4ENC_FRAME_SAMPLES;
                erc = ac4enc_encode_tail_f2(ctx, rr, rem,
                                            pkt,
                                            (size_t)maxp,
                                            &olen, &is_if);
            } else {
                const int *rr[6];
                for (int c = 0; c < nch; c++)
                    rr[c] = irows + c * ns +
                        full * AC4ENC_FRAME_SAMPLES;
                erc = ac4enc_encode_tail2(ctx, rr, rem, pkt,
                                          (size_t)maxp,
                                          &olen, &is_if);
            }
            if (erc < 0) {
                fprintf(stderr, "error: encode tail "
                                "rc=%d (%s/%s)\n",
                        erc,
                        ac4enc_subcode_str(
                            ac4enc_err_sub(ctx)),
                        ac4enc_err_stage(ctx));
                free(pkt);
                goto done;
            }
            if (erc == 1) {
                if (fwrite(pkt, 1, olen, ff) != olen) {
                    fprintf(stderr,
                            "error: frames write\n");
                    free(pkt);
                    goto done;
                }
                fprintf(fm, "frame %d len %lu iframe %d\n",
                        idx, (unsigned long)olen, is_if);
                idx++;
            }
        }
        free(pkt);
        for (;;) {
            uint8_t out[AC4ENC_OUTCAP_SAFE];
            int frc;
            olen = 0;
            is_if = 0;
            frc = ac4enc_flush2(ctx, out, sizeof out, &olen,
                                &is_if);
            if (frc < 0) {
                fprintf(stderr, "error: flush rc=%d "
                                "(%s/%s)\n",
                        frc,
                        ac4enc_subcode_str(
                            ac4enc_err_sub(ctx)),
                        ac4enc_err_stage(ctx));
                goto done;
            }
            if (frc == 0)
                break;
            if (olen > 0 &&
                fwrite(out, 1, olen, ff) != olen) {
                fprintf(stderr, "error: frames write\n");
                goto done;
            }
            fprintf(fm, "frame %d len %lu iframe %d\n", idx,
                    (unsigned long)olen, is_if);
            idx++;
        }
        fprintf(fm, "frames %d\n", idx);
    }
    rc = 0;
    goto done;
bad:
    fprintf(stderr, "error: bad numeric option\n");
done:
    if (ff)
        fclose(ff);
    if (fm)
        fclose(fm);
    ac4enc_close(ctx);
    free(ich);
    free(irows);
    free(drows);
    free(forced);
    return rc;
}
