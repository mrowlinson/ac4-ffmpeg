/* enc_core_enc.c: frame assembly, rate loops, TOC,
 * full-stream drivers. See enc_core.h for the contract. */
#include "enc_core.h"
#include "enc_step.h"
#include <errno.h>
#include <limits.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asf.h"
#include "loudness.h"
#include "mdct.h"
#include "pycompat.h"
#include "sfb_offsets.h"

const uint8_t ENC_CORE_P_TAIL[5] = { 0x9c, 0x00, 0x18, 0x00, 0x40 };
const uint8_t ENC_CORE_I12_MID[2] = { 0x58, 0x80 };
const uint8_t ENC_CORE_I12_MID_TAIL[2] = { 0x58, 0x82 };
const uint8_t ENC_CORE_I12_TAIL[6] =
    { 0x60, 0x1e, 0x58, 0x4a, 0x96, 0xa0 };
const uint8_t ENC_CORE_I11_TAIL[5] = { 0xf2, 0xc2, 0x54, 0xb5, 0x00 };
const uint8_t ENC_CORE_SUB2_DEE[4] = { 0xa0, 0x40, 0x40, 0x00 };
const uint8_t ENC_CORE_I15_TAIL51[3] = { 0xc9, 0x90, 0x80 };
const double ENC_CORE_LEG_COMP_51_DB[6] =
    { -3.70, -3.63, -3.76, -3.17, -3.57, -3.59 };

/* Last-error string (S1 loud diagnostic). Cleared on entry
 * of every public encode driver; set by stamp-composition
 * refusals. Thread-local (r2 B1: concurrent encodes on
 * separate ctx must not clobber each other's diagnostic). */
static _Thread_local char core_errmsg[256];

static void
core_clear_error(void)
{
    core_errmsg[0] = '\0';
}

/* Record a loudness-stamp composition refusal. ni/nfr0 are the
 * composed I-stamp lengths, want_ni/want_fr0 the accepted ones. */
static void
stamp_fail(const char *entry, int sparse, int np6, int ni,
           int nfr0, int want_ni, int want_fr0)
{
    snprintf(core_errmsg, sizeof core_errmsg,
        "%s: loudness stamp compose refused "
        "(sparse=%d np6=%d ni=%d nfr0=%d, want 6/%d/%d)",
        entry, sparse, np6, ni, nfr0, want_ni, want_fr0);
}

const char *
enc_core_last_error(void)
{
    return core_errmsg;
}

/* Growable byte buffer for intermediate substream assemblies. */
typedef struct {
    uint8_t *p;
    size_t n;
    size_t cap;
} bytebuf_t;

static int
bb_reserve(bytebuf_t *b, size_t extra)
{
    if (b->n + extra <= b->cap)
        return 0;
    size_t cap = b->cap ? b->cap : 64;
    while (cap < b->n + extra)
        cap *= 2;
    uint8_t *p = realloc(b->p, cap);
    if (!p)
        return -1;
    b->p = p;
    b->cap = cap;
    return 0;
}

static int
bb_put(bytebuf_t *b, const uint8_t *d, size_t n)
{
    if (bb_reserve(b, n))
        return -1;
    memcpy(b->p + b->n, d, n);
    b->n += n;
    return 0;
}

static int
bb_putc(bytebuf_t *b, uint8_t v, size_t n)
{
    if (bb_reserve(b, n))
        return -1;
    memset(b->p + b->n, v, n);
    b->n += n;
    return 0;
}

static void
bb_free(bytebuf_t *b)
{
    free(b->p);
    b->p = NULL;
    b->n = b->cap = 0;
}

/* Checked put: the BitWriter.fit rule as -1 (never abort). Raw
 * bw_put stays for literals that obviously fit. */
static int
ck(bitwriter_t *w, int64_t v, int n)
{
    if (v < 0 || (n < 63 && (v >> n) != 0))
        return -1;
    bw_put(w, (uint64_t)v, n);
    return 0;
}

/* Copy an aligned writer's bytes into out (outcap checked). */
static int
w_extract(bitwriter_t *w, uint8_t *out, size_t outcap)
{
    size_t n = bw_nbits(w) / 8;
    if (bw_nbits(w) % 8 != 0 || n > outcap || n > (size_t)0x7fffffff)
        return -1;
    if (bw_bytes(w, out, outcap) != n)
        return -1;
    return (int)n;
}

/* ---- loudness stamping ---- */

int
enc_core_content16(double m, int peak, double f, double target_db)
{
    if (isinf(m) && m < 0)
        m = ENC_CORE_M_UNGATED_FLOOR;
    double pdb = 20.0 * log10((peak > 1 ? peak : 1) / 32768.0);
    int v = pycompat_round_half_even(
        160.0 * (target_db - m + pdb) + 49152.0);
    if (f > 0.1)
        v += 512;
    if (v < 0)
        v = 0;
    if (v > 0xFFFF)
        v = 0xFFFF;
    return v & ~0xF;
}

int
enc_core_content16_f(double m, double peak, double f,
                     double target_db)
{
    double pdb, arg;
    int v;
    if (isinf(m) && m < 0)
        m = ENC_CORE_M_UNGATED_FLOOR;
    pdb = 20.0 * log10((peak > 1 ? peak : 1) / 32768.0);
    arg = 160.0 * (target_db - m + pdb) + 49152.0;
    v = pycompat_round_half_even(arg);
    if (f > 0.1)
        v += 512;
    if (v < 0)
        v = 0;
    if (v > 0xFFFF)
        v = 0xFFFF;
    return v & ~0xF;
}

int
enc_core_tail16(int sparse, double tf)
{
    if (!sparse)
        return tf < 0.75 ? ENC_CORE_TAIL16_GATED
                         : ENC_CORE_TAIL16_DENSE;
    return tf < 0.3 ? ENC_CORE_TAIL16_DIRAC : ENC_CORE_TAIL16_IMPULSE;
}

int
enc_core_compose_sub0(int iframe, int sparse, int stamp,
                      int content16, const uint8_t *mid23, int pid,
                      uint8_t *out)
{
    if (!out || stamp < 0 || stamp > 255 || pid < 0 || pid > 255)
        return -1;
    if (!iframe) {
        out[0] = (uint8_t)stamp;
        out[1] = (uint8_t)pid;
        memcpy(out + 2, ENC_CORE_P_TAIL + 1, 4);
        return 6;
    }
    if (sparse) {
        out[0] = (uint8_t)stamp;
        out[1] = (uint8_t)pid;
        memcpy(out + 2, ENC_CORE_I12_MID, 2);
        out[4] = 0x03;
        out[5] = 0x00;
        memcpy(out + 6, ENC_CORE_I11_TAIL, 5);
        return 11;
    }
    if (content16 < 0 || content16 > 0xFFFF)
        return -1;
    if (!mid23)
        mid23 = ENC_CORE_I12_MID;
    out[0] = (uint8_t)stamp;
    out[1] = (uint8_t)pid;
    out[2] = mid23[0];
    out[3] = mid23[1];
    out[4] = (uint8_t)(content16 >> 8);
    out[5] = (uint8_t)content16;
    memcpy(out + 6, ENC_CORE_I12_TAIL, 6);
    return 12;
}

int
enc_core_dmx_block17(int loro_c, int loro_s, int ltrt_c,
                     int ltrt_s, int preferred)
{
    int64_t v = 1;
    v = (v << 3) | loro_c;
    v = (v << 3) | loro_s;
    v = (v << 1) | 1;
    v = (v << 3) | ltrt_c;
    v = (v << 3) | ltrt_s;
    v = (v << 1) | 0;
    v = (v << 2) | preferred;
    return (int)v;
}

int
enc_core_compose_sub0_51(int stamp, int content16,
                         const uint8_t *mid23, int pid,
                         const int *dmx, uint8_t *out)
{
    if (!out || stamp < 0 || stamp > 255 || pid < 0 || pid > 255 ||
        content16 < 0 || content16 > 0xFFFF)
        return -1;
    if (!mid23)
        mid23 = ENC_CORE_I12_MID;
    out[0] = (uint8_t)stamp;
    out[1] = (uint8_t)pid;
    out[2] = mid23[0];
    out[3] = mid23[1];
    out[4] = (uint8_t)(content16 >> 8);
    out[5] = (uint8_t)content16;
    memcpy(out + 6, ENC_CORE_I12_TAIL, 6);
    memcpy(out + 12, ENC_CORE_I15_TAIL51, 3);
    if (dmx) {
        int b17 = enc_core_dmx_block17(dmx[0], dmx[1], dmx[2],
                                       dmx[3], dmx[4]);
        out[12] = (uint8_t)((b17 >> 9) & 0xFF);
        out[13] = (uint8_t)((b17 >> 1) & 0xFF);
        out[14] = (uint8_t)((out[14] & 0x7F) | ((b17 & 1) << 7));
    }
    return 15;
}

int
enc_core_preset_id(const char *name)
{
    if (!name)
        return -1;
    if (!strcmp(name, "manual"))
        return 0x9c;
    if (!strcmp(name, "atsc_a85"))
        return 0x82;
    if (!strcmp(name, "freetv_op59"))
        return 0x88;
    if (!strcmp(name, "arib_b32"))
        return 0x86;
    if (!strcmp(name, "ebu_r128"))
        return 0x84;
    return -1;
}

/* ---- channel folds ---- */

static int
fold_n(const int *lens, int nch)
{
    int n = 0;
    for (int c = 0; c < nch; c++)
        if (lens[c] > n)
            n = lens[c];
    return n;
}

static double
fold_at(const double *ch, int len, int i)
{
    return i < len ? ch[i] : 0.0;
}

int
enc_core_fold_8ch_to_51(const double *const *chs, const int *lens,
                        double **out)
{
    int n;
    if (!chs || !lens || !out)
        return -1;
    n = fold_n(lens, 8);
    for (int c = 0; c < 4; c++)
        for (int i = 0; i < n; i++)
            out[c][i] = fold_at(chs[c], lens[c], i);
    for (int i = 0; i < n; i++) {
        out[4][i] = (fold_at(chs[4], lens[4], i) +
                     fold_at(chs[6], lens[6], i)) / 2.0;
        out[5][i] = (fold_at(chs[5], lens[5], i) +
                     fold_at(chs[7], lens[7], i)) / 2.0;
    }
    return n;
}

int
enc_core_mix51_to_stereo(const double *const *chs, const int *lens,
                         double **out)
{
    int n;
    double g;
    if (!chs || !lens || !out)
        return -1;
    n = fold_n(lens, 6);
    g = 1.0 / sqrt(2.0);
    for (int i = 0; i < n; i++) {
        double gC = g * fold_at(chs[2], lens[2], i);
        double gLs = g * fold_at(chs[4], lens[4], i);
        double gRs = g * fold_at(chs[5], lens[5], i);
        out[0][i] =
            fold_at(chs[0], lens[0], i) + gC + gLs;
        out[1][i] =
            fold_at(chs[1], lens[1], i) + gC + gRs;
    }
    return n;
}

int
enc_core_fold_8ch_to_stereo(const double *const *chs,
                            const int *lens, double **out)
{
    const double *folded[6];
    double *tmp[6];
    int lens6[6];
    int n, m;
    if (!chs || !lens || !out)
        return -1;
    n = fold_n(lens, 8);
    for (int c = 0; c < 6; c++) {
        tmp[c] = malloc((size_t)(n > 0 ? n : 1) * sizeof(double));
        if (!tmp[c]) {
            for (int k = 0; k < c; k++)
                free(tmp[k]);
            return -1;
        }
        folded[c] = tmp[c];
        lens6[c] = n;
    }
    m = enc_core_fold_8ch_to_51(chs, lens, tmp);
    if (m < 0)
        m = -1;
    else
        m = enc_core_mix51_to_stereo(folded, lens6, out);
    for (int c = 0; c < 6; c++)
        free(tmp[c]);
    return m;
}

/* ---- TOC / framing ---- */

int
enc_core_nframes_for_samples(int nsamples)
{
    return (nsamples + ENC_CORE_FRAME_SAMPLES - 1) /
        ENC_CORE_FRAME_SAMPLES + 2;
}

int
enc_core_seq_counter(int i)
{
    if (i == 0)
        return 1019;
    if (i == 1)
        return 1020;
    return ((i - 2) % 1020 + 1020) % 1020 + 1;
}

int
enc_core_b6hi_phase(int i)
{
    static const int tab[4] = { 3, 1, 2, 0 };
    if (i < 2)
        return 3;
    return tab[(i - 2) % 4];
}

/* Parse the iframe interval: NULL/"1sec" -> 0, else decimal step. */
static int
parse_interval(const char *interval, long long *step)
{
    if (!interval || !strcmp(interval, "1sec"))
        return 0;
    if (pycompat_int(interval, step))
        return -1;
    if (*step < 2)
        return -1;
    return 1;
}

int
enc_core_iframe_set(int nframes, const char *interval,
                    const int *forced, int nforced, int *out)
{
    long long step = 0;
    int istep, cnt = 0;
    unsigned char *mark;
    if (nframes <= 0 || !out)
        return nframes <= 0 && out ? 0 : -1;
    istep = parse_interval(interval, &step);
    if (istep < 0)
        return -1;
    mark = calloc((size_t)nframes, 1);
    if (!mark)
        return -1;
    mark[0] = 1;
    if (nframes > 1)
        mark[1] = 1;
    if (istep == 0) {
        for (long long n = 1;; n++) {
            long long j =
                1 + (n * ENC_CORE_SAMPLE_RATE) /
                    ENC_CORE_FRAME_SAMPLES;
            if (j >= nframes)
                break;
            mark[j] = 1;
        }
    } else {
        long long j = 1;
        for (;;) {
            j += step;
            if (j >= nframes)
                break;
            mark[(size_t)j] = 1;
        }
    }
    if (forced) {
        for (int k = 0; k < nforced; k++)
            if (0 <= forced[k] && forced[k] < nframes)
                mark[forced[k]] = 1;
    }
    for (int j = 0; j < nframes; j++)
        if (mark[j])
            out[cnt++] = j;
    free(mark);
    return cnt;
}

int
enc_core_joint_iframes(int nframes, int base, int total,
                       const char *interval, const int *forced,
                       int nforced, int *out)
{
    long long step = 0;
    int istep, cnt = 0;
    unsigned char *mark;
    if (nframes <= 0 || !out)
        return nframes <= 0 && out ? 0 : -1;
    istep = parse_interval(interval, &step);
    if (istep < 0)
        return -1;
    mark = calloc((size_t)nframes, 1);
    if (!mark)
        return -1;
    mark[0] = 1;
    if (istep == 0) {
        for (long long n = 1;; n++) {
            long long j =
                1 + (n * ENC_CORE_SAMPLE_RATE) /
                    ENC_CORE_FRAME_SAMPLES;
            if (j >= total)
                break;
            if (base <= j && j < base + nframes)
                mark[(size_t)(j - base)] = 1;
        }
    } else {
        long long j = 1;
        for (;;) {
            j += step;
            if (j >= total)
                break;
            if (base <= j && j < base + nframes)
                mark[(size_t)(j - base)] = 1;
        }
    }
    if (forced) {
        for (int k = 0; k < nforced; k++)
            if (base <= forced[k] && forced[k] < base + nframes)
                mark[forced[k] - base] = 1;
    }
    for (int j = 0; j < nframes; j++)
        if (mark[j])
            out[cnt++] = j;
    free(mark);
    return cnt;
}

int
enc_core_cbr_raw_sizes(int nframes, int rate_kbps, int *out)
{
    long long num, acc;
    if (nframes < 0 || !out)
        return -1;
    num = (long long)rate_kbps * 16;
    acc = ((-num) % 3 + 3) % 3;
    for (int i = 0; i < nframes; i++) {
        long long q;
        acc += num;
        q = acc / 3;
        acc = acc % 3;
        out[i] = (int)q;
    }
    return 0;
}

int
enc_core_emdf_info(bitwriter_t *w, int preset, int b_si, int si,
                   int len_pri, int len_sec, const uint8_t *resv,
                   int nresv)
{
    int nresv_expect;
    if (!w)
        return -1;
    if (preset >= 0) {
        static const int tab[5][5] = {
            { 0, 0, 0, 0, 0 }, /* none */
            { 1, 2, 2, 0, 4 }, /* deeI */
            { 0, 0, 1, 0, 1 }, /* deeP */
            { 0, 0, 2, 0, 4 }, /* I4 */
            { 0, 0, 1, 0, 1 }, /* P1 */
        };
        if (preset > 4)
            return -1;
        b_si = tab[preset][0];
        si = tab[preset][1];
        len_pri = tab[preset][2];
        len_sec = tab[preset][3];
        nresv_expect = tab[preset][4];
    } else {
        if (len_pri < 0 || len_pri > 3 || len_sec < 0 ||
            len_sec > 3)
            return -1;
        nresv_expect = (len_pri ? 1 << (2 * (len_pri - 1)) : 0) +
            (len_sec ? 1 << (2 * (len_sec - 1)) : 0);
    }
    if (nresv != nresv_expect)
        return -1;
    if (nresv > 0 && !resv)
        return -1;
    bw_put(w, 0, 2);
    bw_put(w, 0, 3);
    if (ck(w, b_si, 1))
        return -1;
    if (b_si && ck(w, si, 2))
        return -1;
    if (ck(w, len_pri, 2) || ck(w, len_sec, 2))
        return -1;
    for (int k = 0; k < nresv; k++)
        bw_put(w, resv[k], 8);
    return 0;
}

int
enc_core_write_toc(bitwriter_t *w, int i, int iframe,
                   const char *layout, int audio_si,
                   int content_type, int emdf,
                   const uint8_t *resv, int nresv)
{
    const char *cm_bits;
    int md_compat;
    if (!w || !layout)
        return -1;
    if (!strcmp(layout, "stereo")) {
        cm_bits = "10";
        md_compat = 0;
    } else if (!strcmp(layout, "5.1")) {
        cm_bits = "1110";
        md_compat = 1;
    } else {
        return -1;
    }
    iframe = !!iframe;
    bw_put(w, 2, 2);
    if (ck(w, enc_core_seq_counter(i), 10))
        return -1;
    bw_put(w, 1, 1);
    bw_put(w, 6, 3);
    bw_put(w, (unsigned)enc_core_b6hi_phase(i), 2);
    bw_put(w, 1, 1);
    bw_put(w, 13, 4);
    bw_put(w, (unsigned)iframe, 1);
    bw_put(w, 1, 1);
    bw_put(w, 1, 1);
    bw_put(w, 0, 5);
    bw_put(w, 0, 1);
    bw_put(w, 1, 1);
    bw_putbits(w, "10");
    bw_put(w, (unsigned)md_compat, 3);
    bw_put(w, 1, 1);
    bw_varbits(w, 2, 0);
    if (enc_core_emdf_info(w, emdf, 0, 0, 0, 0, resv, nresv))
        return -1;
    bw_put(w, 0, 1);
    bw_put(w, 0, 3);
    bw_put(w, 0, 1);
    bw_put(w, 0, 1);
    bw_put(w, 0, 1);
    bw_put(w, (unsigned)iframe, 1);
    bw_put(w, 0, 2);
    bw_put(w, 1, 1);
    bw_put(w, 0, 1);
    bw_put(w, 1, 1);
    bw_put(w, 1, 1);
    bw_putbits(w, cm_bits);
    bw_put(w, 0, 1);
    bw_put(w, 0, 1);
    bw_put(w, (unsigned)iframe, 1);
    if (ck(w, audio_si, 2))
        return -1;
    if (content_type) {
        bw_put(w, 1, 1);
        bw_put(w, 0, 3);
        bw_put(w, 0, 1);
    } else {
        bw_put(w, 0, 1);
    }
    return 0;
}

int
enc_core_finish_toc(bitwriter_t *w, const int *sizes, int nsizes)
{
    if (!w || !sizes || nsizes < 1 || nsizes > 3)
        return -1;
    if (ck(w, nsizes, 2))
        return -1;
    if (nsizes == 1)
        bw_put(w, 1, 1);
    for (int k = 0; k < nsizes; k++) {
        int s = sizes[k];
        if (s < 0)
            return -1;
        if (s < 1024) {
            bw_put(w, 0, 1);
            if (ck(w, s, 10))
                return -1;
        } else {
            bw_put(w, 1, 1);
            bw_put(w, (unsigned)(s % 1024), 10);
            bw_varbits_long(w, 2, (uint64_t)(s / 1024));
        }
    }
    bw_align(w, 0);
    return 0;
}

/* ---- sf_info / silence legs ---- */

static void
sf_info_fullband(bitwriter_t *w)
{
    bw_put(w, 1, 1);
    bw_put(w, ENC_CORE_MAX_SFB_FULL, 6);
}

typedef struct {
    const int *offs;
    int msfb;
    int sect_bits;
} mix_geo_t;

static int
mixed_group_geo(int orient, mix_geo_t *geo)
{
    const int *o1024 = sfb_offsets48(1024);
    const int *o128 = sfb_offsets48(128);
    if (!o1024 || !o128)
        return -1;
    if (orient == ENC_CORE_ORIENT_1024_FIRST) {
        geo[0].offs = o1024;
        geo[0].msfb = ENC_CORE_MSFB_MIX_1024;
        geo[0].sect_bits = 5;
        for (int k = 1; k < 9; k++) {
            geo[k].offs = o128;
            geo[k].msfb = ENC_CORE_MSFB_MIX_128;
            geo[k].sect_bits = 3;
        }
        return 0;
    }
    if (orient == ENC_CORE_ORIENT_128_FIRST) {
        for (int k = 0; k < 8; k++) {
            geo[k].offs = o128;
            geo[k].msfb = ENC_CORE_MSFB_MIX_128;
            geo[k].sect_bits = 3;
        }
        geo[8].offs = o1024;
        geo[8].msfb = ENC_CORE_MSFB_MIX_1024;
        geo[8].sect_bits = 5;
        return 0;
    }
    return -1;
}

static int
sf_info_mixed(bitwriter_t *w, int orient)
{
    if (orient != ENC_CORE_ORIENT_1024_FIRST &&
        orient != ENC_CORE_ORIENT_128_FIRST)
        return -1;
    bw_put(w, 0, 1);
    if (orient == ENC_CORE_ORIENT_1024_FIRST) {
        bw_put(w, ENC_CORE_TL_1024, 2);
        bw_put(w, ENC_CORE_TL_128, 2);
        bw_put(w, ENC_CORE_MSFB_MIX_1024, 6);
        bw_put(w, ENC_CORE_MSFB_MIX_128, 4);
    } else {
        bw_put(w, ENC_CORE_TL_128, 2);
        bw_put(w, ENC_CORE_TL_1024, 2);
        bw_put(w, ENC_CORE_MSFB_MIX_128, 4);
        bw_put(w, ENC_CORE_MSFB_MIX_1024, 6);
    }
    bw_put(w, ENC_CORE_MIX_GROUPING, 7);
    return 0;
}

static void
sf_data_silence(bitwriter_t *w, int max_sfb)
{
    int remaining;
    bw_put(w, 0, 4);
    remaining = max_sfb - 1;
    while (remaining >= 31) {
        bw_put(w, 31, 5);
        remaining -= 31;
    }
    bw_put(w, (unsigned)remaining, 5);
    bw_put(w, 0, 8);
    bw_put(w, 0, 1);
}

static void
mono_data(bitwriter_t *w, int lfe)
{
    if (lfe) {
        bw_put(w, ENC_CORE_MAX_SFB_LFE, 3);
        sf_data_silence(w, ENC_CORE_MAX_SFB_LFE);
    } else {
        bw_put(w, 0, 1);
        sf_info_fullband(w);
        sf_data_silence(w, ENC_CORE_MAX_SFB_FULL);
    }
}

static void
stereo_data(bitwriter_t *w)
{
    bw_put(w, 0, 1);
    bw_put(w, 0, 1);
    sf_info_fullband(w);
    bw_put(w, 0, 1);
    sf_info_fullband(w);
    sf_data_silence(w, ENC_CORE_MAX_SFB_FULL);
    sf_data_silence(w, ENC_CORE_MAX_SFB_FULL);
}

static void
two_channel_data(bitwriter_t *w)
{
    bw_put(w, 0, 1);
    sf_info_fullband(w);
    sf_info_fullband(w);
    sf_data_silence(w, ENC_CORE_MAX_SFB_FULL);
    sf_data_silence(w, ENC_CORE_MAX_SFB_FULL);
}

/* Validate one LONG leg for asf_write_sf_data (never abort into
 * the dep's asserts): line/q ranges, ref range, v2 chain rule. */
static int
leg_sf_ok(const int *lines, const int *qs, int n, int ref,
          int max_sfb, const int *refs, int *has_refs_out,
          const int *offs)
{
    int has = 0;
    int qb[64];
    int nqb = 0;
    if (n < 0 || ref < 0 || ref > 255 || max_sfb < 1 ||
        max_sfb > 63 || !offs)
        return -1;
    if (n > 0 && (!lines || !qs))
        return -1;
    for (int k = 0; k < n; k++) {
        int q = qs[k] < 0 ? -qs[k] : qs[k];
        if (lines[k] < 0 || lines[k] >= offs[max_sfb] || q < 1 ||
            q > ASF_MAX_Q)
            return -1;
    }
    if (refs) {
        for (int b = 0; b < max_sfb; b++) {
            if (refs[b] < -1 || refs[b] > 255)
                return -1;
            if (refs[b] >= 0)
                has = 1;
        }
    }
    if (n == 0 && has)
        return -1; /* Python IndexError-class (refs[qb[0]]) */
    if (n > 0 && has) {
        for (int k = 0; k < n; k++) {
            int b = asf_band_of(lines[k], offs, max_sfb);
            int seen = 0;
            if (b < 0)
                return -1;
            for (int j = 0; j < nqb; j++)
                if (qb[j] == b)
                    seen = 1;
            if (!seen)
                qb[nqb++] = b;
        }
        /* ascending (insertion; nqb <= 63) */
        for (int k = 1; k < nqb; k++) {
            int t = qb[k], j = k - 1;
            while (j >= 0 && qb[j] > t) {
                qb[j + 1] = qb[j];
                j--;
            }
            qb[j + 1] = t;
        }
        for (int k = 0; k < nqb; k++)
            if (refs[qb[k]] < 0)
                return -1;
        if (refs[qb[0]] != ref)
            return -1;
        for (int k = 1; k < nqb; k++) {
            int step = refs[qb[k]] - refs[qb[k - 1]];
            if (step < -60 || step > 60)
                return -1;
        }
    }
    if (has_refs_out)
        *has_refs_out = has;
    return 0;
}

static int
leg_write(bitwriter_t *w, const enc_core_leg_t *leg, int max_sfb)
{
    const int *offs = sfb_offsets48(2048);
    int has = 0;
    if (!leg || !offs)
        return -1;
    if (leg_sf_ok(leg->lines, leg->qs, leg->n, leg->ref,
                  max_sfb, leg->refs, &has, offs))
        return -1;
    asf_write_sf_data(w, leg->lines, leg->qs, leg->n, leg->ref,
                      max_sfb, has ? leg->refs : NULL, has,
                      offs, 5);
    return 0;
}

/* Validate one mixed group for asf_write_mixed_leg. */
static int
mgroup_ok(const enc_core_mgroup_t *g, const mix_geo_t *geo)
{
    for (int k = 0; k < g->n; k++) {
        int q = g->qs[k] < 0 ? -g->qs[k] : g->qs[k];
        if (g->lines[k] < 0 ||
            g->lines[k] >= geo->offs[geo->msfb] || q < 1 ||
            q > ASF_MAX_Q)
            return -1;
    }
    for (int b = 0; b < 63; b++)
        if (g->refs[b] < -1 || g->refs[b] > 255)
            return -1;
    return 0;
}

/* Full mixed-leg chain rule (first ref + 60-step DPCM). */
static int
mixed_chain_ok(const enc_core_mixed_leg_t *leg,
               const mix_geo_t *geo, int ref)
{
    int first = 1;
    int prev = 0;
    for (int gi = 0; gi < 9; gi++) {
        int qb[64];
        int nqb = 0;
        for (int k = 0; k < leg->g[gi].n; k++) {
            int b = asf_band_of(leg->g[gi].lines[k],
                                geo[gi].offs, geo[gi].msfb);
            int seen = 0;
            if (b < 0)
                return -1;
            for (int j = 0; j < nqb; j++)
                if (qb[j] == b)
                    seen = 1;
            if (!seen)
                qb[nqb++] = b;
        }
        for (int k = 1; k < nqb; k++) {
            int t = qb[k], j = k - 1;
            while (j >= 0 && qb[j] > t) {
                qb[j + 1] = qb[j];
                j--;
            }
            qb[j + 1] = t;
        }
        for (int k = 0; k < nqb; k++) {
            int r = leg->g[gi].refs[qb[k]];
            if (r < 0 || r > 255)
                return -1;
            if (first) {
                if (r != ref)
                    return -1;
                first = 0;
            } else if (r - prev < -60 || r - prev > 60) {
                return -1;
            }
            prev = r;
        }
    }
    return 0;
}

static int
mixed_write(bitwriter_t *w, const enc_core_mixed_leg_t *leg,
            int ref, int orient)
{
    mix_geo_t geo[9];
    asf_group_t groups[9];
    if (!leg || ref < 0 || ref > 255)
        return -1;
    if (mixed_group_geo(orient, geo))
        return -1;
    for (int gi = 0; gi < 9; gi++)
        if (mgroup_ok(&leg->g[gi], &geo[gi]))
            return -1;
    if (mixed_chain_ok(leg, geo, ref))
        return -1;
    for (int gi = 0; gi < 9; gi++) {
        groups[gi].lines = leg->g[gi].lines;
        groups[gi].wins = NULL;
        groups[gi].qs = leg->g[gi].qs;
        groups[gi].n = leg->g[gi].n;
        groups[gi].offs = geo[gi].offs;
        groups[gi].max_sfb = geo[gi].msfb;
        groups[gi].sect_bits = geo[gi].sect_bits;
        groups[gi].nwins = 1;
        groups[gi].refs_by_band = leg->g[gi].refs;
    }
    asf_write_mixed_leg(w, groups, 9, ref);
    return 0;
}

static int
audio_51_w(bitwriter_t *w, const enc_core_leg_t *lfe,
           const enc_core_leg_t *l, const enc_core_leg_t *r,
           const enc_core_leg_t *ls, const enc_core_leg_t *rs,
           const enc_core_leg_t *c)
{
    const enc_core_leg_t *pairs[4] = { l, r, ls, rs };
    if (!w || !lfe || !l || !r || !ls || !rs || !c)
        return -1;
    bw_put(w, 0, 3);
    bw_put(w, ENC_CORE_MAX_SFB_LFE, 3);
    if (leg_write(w, lfe, ENC_CORE_MAX_SFB_LFE))
        return -1;
    bw_put(w, 0, 2);
    bw_put(w, 0, 1);
    for (int p = 0; p < 4; p += 2) {
        bw_put(w, 0, 1);
        sf_info_fullband(w);
        sf_info_fullband(w);
        if (leg_write(w, pairs[p], ENC_CORE_MAX_SFB_FULL))
            return -1;
        if (leg_write(w, pairs[p + 1], ENC_CORE_MAX_SFB_FULL))
            return -1;
    }
    bw_put(w, 0, 1);
    sf_info_fullband(w);
    if (leg_write(w, c, ENC_CORE_MAX_SFB_FULL))
        return -1;
    bw_align(w, 0);
    return 0;
}

int
enc_core_write_audio_51(const enc_core_leg_t *lfe,
                        const enc_core_leg_t *l,
                        const enc_core_leg_t *r,
                        const enc_core_leg_t *ls,
                        const enc_core_leg_t *rs,
                        const enc_core_leg_t *c, uint8_t *out,
                        size_t outcap)
{
    bitwriter_t w;
    int n;
    if (!out)
        return -1;
    bw_init(&w);
    n = audio_51_w(&w, lfe, l, r, ls, rs, c);
    if (n == 0)
        n = w_extract(&w, out, outcap);
    else
        n = -1;
    bw_free(&w);
    return n;
}

static int
audio_51_mixed_w(bitwriter_t *w, const enc_core_leg_t *lfe,
                 const enc_core_mixed_leg_t *l, int refL,
                 const enc_core_mixed_leg_t *r, int refR,
                 const enc_core_mixed_leg_t *ls, int refLs,
                 const enc_core_mixed_leg_t *rs, int refRs,
                 const enc_core_mixed_leg_t *c, int refC,
                 int orient)
{
    if (!w || !lfe || !l || !r || !ls || !rs || !c)
        return -1;
    if (orient != ENC_CORE_ORIENT_1024_FIRST &&
        orient != ENC_CORE_ORIENT_128_FIRST)
        return -1;
    bw_put(w, 0, 3);
    bw_put(w, ENC_CORE_MAX_SFB_LFE, 3);
    if (leg_write(w, lfe, ENC_CORE_MAX_SFB_LFE))
        return -1;
    bw_put(w, 0, 2);
    bw_put(w, 0, 1);
    {
        const enc_core_mixed_leg_t *legs[4] = { l, r, ls, rs };
        int refs[4] = { refL, refR, refLs, refRs };
        for (int p = 0; p < 4; p += 2) {
            bw_put(w, 0, 1);
            if (sf_info_mixed(w, orient))
                return -1;
            if (sf_info_mixed(w, orient))
                return -1;
            if (mixed_write(w, legs[p], refs[p], orient))
                return -1;
            if (mixed_write(w, legs[p + 1], refs[p + 1],
                            orient))
                return -1;
        }
    }
    bw_put(w, 0, 1);
    if (sf_info_mixed(w, orient))
        return -1;
    if (mixed_write(w, c, refC, orient))
        return -1;
    bw_align(w, 0);
    return 0;
}

int
enc_core_write_audio_51_mixed(const enc_core_leg_t *lfe,
                              const enc_core_mixed_leg_t *l,
                              int refL,
                              const enc_core_mixed_leg_t *r,
                              int refR,
                              const enc_core_mixed_leg_t *ls,
                              int refLs,
                              const enc_core_mixed_leg_t *rs,
                              int refRs,
                              const enc_core_mixed_leg_t *c,
                              int refC, int orient, uint8_t *out,
                              size_t outcap)
{
    bitwriter_t w;
    int n;
    if (!out)
        return -1;
    bw_init(&w);
    n = audio_51_mixed_w(&w, lfe, l, refL, r, refR, ls, refLs,
                         rs, refRs, c, refC, orient);
    if (n == 0)
        n = w_extract(&w, out, outcap);
    else
        n = -1;
    bw_free(&w);
    return n;
}

static int
audio_data_w(bitwriter_t *w, const char *layout)
{
    if (!w || !layout)
        return -1;
    if (!strcmp(layout, "stereo")) {
        bw_put(w, 0, 2);
        stereo_data(w);
    } else if (!strcmp(layout, "5.1")) {
        bw_put(w, 0, 3);
        mono_data(w, 1);
        bw_put(w, 0, 2);
        bw_put(w, 0, 1);
        two_channel_data(w);
        two_channel_data(w);
        mono_data(w, 0);
    } else {
        return -1;
    }
    bw_align(w, 0);
    return 0;
}

int
enc_core_write_audio_data(const char *layout, int iframe,
                          uint8_t *out, size_t outcap)
{
    bitwriter_t w;
    int n;
    (void)iframe;
    if (!out)
        return -1;
    bw_init(&w);
    n = audio_data_w(&w, layout);
    if (n == 0)
        n = w_extract(&w, out, outcap);
    else
        n = -1;
    bw_free(&w);
    return n;
}

/* Raw spectral legs (refs None): validate ranges only. */
static int
raw_legs_ok(const int *linesL, const int *qsL, int nL,
            const int *linesR, const int *qsR, int nR)
{
    const int *offs = sfb_offsets48(2048);
    if (!offs)
        return -1;
    if (leg_sf_ok(linesL, qsL, nL, 100,
                  ENC_CORE_MAX_SFB_FULL, NULL, NULL, offs))
        return -1;
    if (leg_sf_ok(linesR, qsR, nR, 100,
                  ENC_CORE_MAX_SFB_FULL, NULL, NULL, offs))
        return -1;
    return 0;
}

int
enc_core_write_audio_spectral(const int *linesL, const int *qsL,
                              int nL, int refL, const int *linesR,
                              const int *qsR, int nR, int refR,
                              uint8_t *out, size_t outcap)
{
    const int *offs = sfb_offsets48(2048);
    bitwriter_t w;
    int n;
    if (!out || !offs || refL < 0 || refL > 255 || refR < 0 ||
        refR > 255)
        return -1;
    if (raw_legs_ok(linesL, qsL, nL, linesR, qsR, nR))
        return -1;
    bw_init(&w);
    bw_put(&w, 0, 2);
    bw_put(&w, 0, 1);
    bw_put(&w, 0, 1);
    sf_info_fullband(&w);
    bw_put(&w, 0, 1);
    sf_info_fullband(&w);
    asf_write_sf_data(&w, linesL, qsL, nL, refL,
                      ENC_CORE_MAX_SFB_FULL, NULL, 0, offs, 5);
    asf_write_sf_data(&w, linesR, qsR, nR, refR,
                      ENC_CORE_MAX_SFB_FULL, NULL, 0, offs, 5);
    bw_align(&w, 0);
    n = w_extract(&w, out, outcap);
    bw_free(&w);
    return n;
}

static int
audio_fullband_w(bitwriter_t *w, const enc_core_leg_t *l,
                 const enc_core_leg_t *r)
{
    if (!w || !l || !r)
        return -1;
    bw_put(w, 0, 2);
    bw_put(w, 0, 1);
    bw_put(w, 0, 1);
    sf_info_fullband(w);
    bw_put(w, 0, 1);
    sf_info_fullband(w);
    if (leg_write(w, l, ENC_CORE_MAX_SFB_FULL))
        return -1;
    if (leg_write(w, r, ENC_CORE_MAX_SFB_FULL))
        return -1;
    bw_align(w, 0);
    return 0;
}

int
enc_core_write_audio_fullband(const enc_core_leg_t *l,
                              const enc_core_leg_t *r, uint8_t *out,
                              size_t outcap)
{
    bitwriter_t w;
    int n;
    if (!out)
        return -1;
    bw_init(&w);
    n = audio_fullband_w(&w, l, r);
    if (n == 0)
        n = w_extract(&w, out, outcap);
    else
        n = -1;
    bw_free(&w);
    return n;
}

int
enc_core_write_audio_joint_mixed(const enc_core_mixed_leg_t *m,
                                 int refM,
                                 const enc_core_mixed_leg_t *s,
                                 int refS, int orient, uint8_t *out,
                                 size_t outcap)
{
    bitwriter_t w;
    int n = -1;
    if (!out || !m || !s)
        return -1;
    if (orient != ENC_CORE_ORIENT_1024_FIRST &&
        orient != ENC_CORE_ORIENT_128_FIRST)
        return -1;
    bw_init(&w);
    bw_put(&w, 0, 2);
    bw_put(&w, 1, 1);
    if (sf_info_mixed(&w, orient) == 0) {
        bw_put(&w, 2, 2);
        if (mixed_write(&w, m, refM, orient) == 0 &&
            mixed_write(&w, s, refS, orient) == 0) {
            bw_align(&w, 0);
            n = w_extract(&w, out, outcap);
        }
    }
    bw_free(&w);
    return n;
}

int
enc_core_write_audio_mixed(const enc_core_mixed_leg_t *l, int refL,
                           const enc_core_mixed_leg_t *r, int refR,
                           int orient, uint8_t *out, size_t outcap)
{
    bitwriter_t w;
    int n = -1;
    if (!out || !l || !r)
        return -1;
    if (orient != ENC_CORE_ORIENT_1024_FIRST &&
        orient != ENC_CORE_ORIENT_128_FIRST)
        return -1;
    bw_init(&w);
    bw_put(&w, 0, 2);
    bw_put(&w, 0, 1);
    bw_put(&w, 0, 1);
    if (sf_info_mixed(&w, orient) == 0) {
        bw_put(&w, 0, 1);
        if (sf_info_mixed(&w, orient) == 0 &&
            mixed_write(&w, l, refL, orient) == 0 &&
            mixed_write(&w, r, refR, orient) == 0) {
            bw_align(&w, 0);
            n = w_extract(&w, out, outcap);
        }
    }
    bw_free(&w);
    return n;
}

/* ---- DRC / metadata / substreams ---- */

int
enc_core_write_drc_curve(bitwriter_t *w,
                         const enc_core_drc_curve_t *c)
{
    if (!w || !c)
        return -1;
    if (ck(w, c->nb_lo, 4) || ck(w, c->nb_hi, 4) ||
        ck(w, c->maxboost, 4))
        return -1;
    if (c->maxboost) {
        if (ck(w, c->lev_b, 5) || ck(w, c->nr_b, 1))
            return -1;
        if (c->nr_b &&
            (ck(w, c->g_b, 4) || ck(w, c->l_b, 5)))
            return -1;
    }
    if (ck(w, c->maxcut, 5))
        return -1;
    if (c->maxcut) {
        if (ck(w, c->lev_c, 6) || ck(w, c->nr_c, 1))
            return -1;
        if (c->nr_c &&
            (ck(w, c->g_c, 5) || ck(w, c->l_c, 5)))
            return -1;
    }
    if (ck(w, c->tcdef, 1))
        return -1;
    if (!c->tcdef) {
        for (int k = 0; k < 4; k++)
            if (ck(w, c->tc[k], 8))
                return -1;
        if (!c->has_thr) {
            bw_put(w, 0, 1);
        } else {
            bw_put(w, 1, 1);
            if (ck(w, c->thr0, 5) || ck(w, c->thr1, 5))
                return -1;
        }
    }
    return 0;
}

int
enc_core_drc_curve_bits(const enc_core_drc_curve_t *c)
{
    int n;
    if (!c)
        return -1;
    n = 4 + 4 + 4;
    if (c->maxboost) {
        n += 5 + 1;
        if (c->nr_b)
            n += 4 + 5;
    }
    n += 5;
    if (c->maxcut) {
        n += 6 + 1;
        if (c->nr_c)
            n += 5 + 5;
    }
    n += 1;
    if (!c->tcdef) {
        n += 32 + 1;
        if (c->has_thr)
            n += 5 + 5;
    }
    return n;
}

int
enc_core_write_drc_frame(bitwriter_t *w, int eac3,
                         const enc_core_drc_mode_t *modes,
                         int nmodes, int iframe)
{
    int tail = 0;
    if (!w || !modes || nmodes < 1)
        return -1;
    bw_put(w, 1, 1);
    if (iframe) {
        if (ck(w, nmodes - 1, 3))
            return -1;
        for (int k = 0; k < nmodes; k++) {
            if (ck(w, modes[k].mid, 3))
                return -1;
            bw_put(w, 0, 1);
            if (modes[k].kind == ENC_CORE_DRC_DEFAULT) {
                bw_put(w, 1, 1);
            } else if (modes[k].kind == ENC_CORE_DRC_CURVE) {
                bw_put(w, 0, 1);
                bw_put(w, 1, 1);
                if (enc_core_write_drc_curve(
                        w, &modes[k].curve))
                    return -1;
            } else if (modes[k].kind == ENC_CORE_DRC_GAINS) {
                bw_put(w, 0, 1);
                bw_put(w, 0, 1);
                bw_put(w, 0, 2);
            } else {
                return -1;
            }
        }
        if (ck(w, eac3, 3))
            return -1;
    }
    for (int k = 0; k < nmodes; k++) {
        if (modes[k].kind == ENC_CORE_DRC_GAINS) {
            bw_put(w, 9, 6);
            bw_put(w, 0, 1);
            bw_put(w, 0, 2);
            if (ck(w, modes[k].gain, 7))
                return -1;
        } else if (modes[k].kind == ENC_CORE_DRC_DEFAULT ||
                   modes[k].kind == ENC_CORE_DRC_CURVE) {
            tail = 1;
        } else {
            return -1;
        }
    }
    if (tail) {
        bw_put(w, 0, 1);
        bw_put(w, 0, 2);
    }
    return 0;
}

int
enc_core_drc_frame_bits(const enc_core_drc_mode_t *modes,
                        int nmodes, int iframe)
{
    int n, tail = 0;
    if (!modes || nmodes < 1)
        return -1;
    n = 1;
    if (iframe) {
        n += 3 + 3;
        for (int k = 0; k < nmodes; k++) {
            n += 3 + 1 + 1;
            if (modes[k].kind == ENC_CORE_DRC_CURVE)
                n += 1 + enc_core_drc_curve_bits(
                    &modes[k].curve);
            else if (modes[k].kind == ENC_CORE_DRC_GAINS)
                n += 1 + 2;
        }
    }
    for (int k = 0; k < nmodes; k++) {
        if (modes[k].kind == ENC_CORE_DRC_GAINS)
            n += 16;
        else
            tail = 1;
    }
    if (tail)
        n += 3;
    return n;
}

static int
metadata_w(bitwriter_t *w, int iframe, const enc_core_drc_t *drc)
{
    bw_put(w, ENC_CORE_DIALNORM_BITS, 7);
    bw_put(w, 0, 1);
    bw_put(w, 0, 1);
    bw_put(w, 0, 1);
    if (drc) {
        int size;
        if (!drc->modes || drc->nmodes < 1)
            return -1;
        size = !iframe ? 2
                       : enc_core_drc_frame_bits(
                             drc->modes, drc->nmodes, iframe);
        if (size < 0)
            return -1;
        bw_put(w, (unsigned)(size & 0x7F), 7);
        if (size > 127) {
            bw_put(w, 1, 1);
            bw_put(w, (unsigned)(size >> 7), 3);
            bw_put(w, 0, 1);
        } else {
            bw_put(w, 0, 1);
        }
    } else {
        bw_put(w, 0, 7);
        bw_put(w, 0, 1);
    }
    if (drc) {
        if (iframe) {
            if (enc_core_write_drc_frame(
                    w, drc->eac3, drc->modes,
                    drc->nmodes, iframe))
                return -1;
        } else {
            bw_put(w, 0, 1);
        }
    } else {
        bw_put(w, 0, 1);
    }
    if (drc) {
        bw_put(w, 1, 1);
        if (iframe) {
            bw_put(w, 0, 2);
            bw_put(w, 2, 2);
            bw_put(w, 0, 3);
        }
    } else {
        bw_put(w, 0, 1);
    }
    bw_put(w, 0, 1);
    bw_align(w, 0);
    return 0;
}

int
enc_core_write_metadata(const char *layout, int iframe,
                        const enc_core_drc_t *drc, uint8_t *out,
                        size_t outcap)
{
    bitwriter_t w;
    int n;
    (void)layout;
    if (!out)
        return -1;
    bw_init(&w);
    n = metadata_w(&w, !!iframe, drc);
    if (n == 0)
        n = w_extract(&w, out, outcap);
    else
        n = -1;
    bw_free(&w);
    return n;
}

/* sub_audio into a bytebuf (shared by public + internal users). */
static int
sub_audio_bb(bytebuf_t *out, const uint8_t *audio, int n_audio,
             const char *layout, int iframe,
             const enc_core_drc_t *drc)
{
    bitwriter_t w;
    uint8_t head[2];
    uint8_t *meta;
    int n_meta;
    if (!audio || n_audio < 0 || n_audio >= (1 << 15))
        return -1;
    meta = malloc(1024);
    if (!meta)
        return -1;
    n_meta = enc_core_write_metadata(layout, iframe, drc, meta,
                                     1024);
    if (n_meta < 0) {
        free(meta);
        return -1;
    }
    bw_init(&w);
    bw_put(&w, (unsigned)n_audio, 15);
    bw_put(&w, 0, 1);
    bw_align(&w, 0);
    if (bw_bytes(&w, head, 2) != 2) {
        bw_free(&w);
        free(meta);
        return -1;
    }
    bw_free(&w);
    if (bb_put(out, head, 2) || bb_put(out, audio, (size_t)n_audio) ||
        bb_put(out, meta, (size_t)n_meta)) {
        free(meta);
        return -1;
    }
    free(meta);
    return 0;
}

int
enc_core_write_sub_audio(const uint8_t *audio, int n_audio,
                         const char *layout, int iframe,
                         const enc_core_drc_t *drc, const int *dmx,
                         uint8_t *out, size_t outcap)
{
    bytebuf_t b = { NULL, 0, 0 };
    (void)dmx;
    if (!out)
        return -1;
    if (sub_audio_bb(&b, audio, n_audio, layout, iframe, drc)) {
        bb_free(&b);
        return -1;
    }
    if (b.n > outcap) {
        bb_free(&b);
        return -1;
    }
    memcpy(out, b.p, b.n);
    {
        int n = (int)b.n;
        bb_free(&b);
        return n;
    }
}

int
enc_core_write_substream(const char *layout, int iframe,
                         const enc_core_drc_t *drc, const int *dmx,
                         uint8_t *out, size_t outcap)
{
    uint8_t *audio;
    int n_audio, n;
    if (!out)
        return -1;
    audio = malloc(65536);
    if (!audio)
        return -1;
    n_audio =
        enc_core_write_audio_data(layout, iframe, audio, 65536);
    if (n_audio < 0) {
        free(audio);
        return -1;
    }
    n = enc_core_write_sub_audio(audio, n_audio, layout, iframe,
                                 drc, dmx, out, outcap);
    free(audio);
    return n;
}

/* Assemble TOC + fill + subs into a padded raw body. subs carry
 * (ptr, len) pairs; returns bytes or -1 (None-class when over). */
static int
assemble_raw(int i, int iframe, const char *layout,
             int content_type, const uint8_t *const *subs,
             const int *nlens, int nsubs, int raw_size,
             uint8_t *out, size_t outcap)
{
    static const uint8_t zeros[4] = { 0, 0, 0, 0 };
    bitwriter_t w;
    bytebuf_t body = { NULL, 0, 0 };
    uint8_t *toc;
    int sizes[3];
    int n_toc, n;
    int emdf = iframe ? ENC_CORE_EMDF_I4 : ENC_CORE_EMDF_P1;
    int nresv = iframe ? 4 : 1;
    if (!subs || !nlens || nsubs < 1 || nsubs > 3 || !out ||
        raw_size < 0)
        return -1;
    toc = malloc(1024);
    if (!toc)
        return -1;
    bw_init(&w);
    n = enc_core_write_toc(&w, i, iframe, layout, 1,
                           content_type, emdf, zeros, nresv);
    if (n == 0) {
        for (int k = 0; k < nsubs; k++)
            sizes[k] = nlens[k];
        n = enc_core_finish_toc(&w, sizes, nsubs);
    }
    if (n == 0)
        n = w_extract(&w, toc, 1024);
    bw_free(&w);
    if (n < 0) {
        free(toc);
        return -1;
    }
    n_toc = n;
    n = -1;
    if (bb_put(&body, toc, (size_t)n_toc) ||
        bb_putc(&body, 0, 1))
        goto done;
    for (int k = 0; k < nsubs; k++)
        if (bb_put(&body, subs[k], (size_t)nlens[k]))
            goto done;
    if ((int)body.n > raw_size)
        goto done; /* Python assert-class -> -1 */
    if (bb_putc(&body, 0, (size_t)(raw_size - (int)body.n)))
        goto done;
    if (body.n > outcap)
        goto done;
    memcpy(out, body.p, body.n);
    n = (int)body.n;
done:
    free(toc);
    bb_free(&body);
    return n;
}

int
enc_core_write_raw_frame(int i, int raw_size, const char *layout,
                         int iframe, const enc_core_drc_t *drc,
                         const int *dmx, uint8_t *out,
                         size_t outcap)
{
    uint8_t *sub;
    const uint8_t *subs[1];
    int nlens[1];
    int n;
    if (!out)
        return -1;
    sub = malloc(65536);
    if (!sub)
        return -1;
    n = enc_core_write_substream(layout, iframe, drc, dmx, sub,
                                 65536);
    if (n < 0) {
        free(sub);
        return -1;
    }
    subs[0] = sub;
    nlens[0] = n;
    n = assemble_raw(i, !!iframe, layout, 0, subs, nlens, 1,
                     raw_size, out, outcap);
    free(sub);
    return n;
}

int
enc_core_write_sync_frame(const uint8_t *raw, int n_raw,
                          const char *trailer, uint8_t *out,
                          size_t outcap)
{
    uint8_t head[4];
    uint16_t word;
    uint8_t *cat;
    if (!raw || !trailer || !out || n_raw < 0 ||
        n_raw >= 0xFFFF)
        return -1;
    head[0] = (uint8_t)(ENC_CORE_SYNC >> 8);
    head[1] = (uint8_t)ENC_CORE_SYNC;
    head[2] = (uint8_t)(n_raw >> 8);
    head[3] = (uint8_t)n_raw;
    if (!strcmp(trailer, "crc")) {
        cat = malloc((size_t)n_raw + 2);
        if (!cat)
            return -1;
        cat[0] = head[2];
        cat[1] = head[3];
        memcpy(cat + 2, raw, (size_t)n_raw);
        word = ac4_crc16(cat, (size_t)n_raw + 2);
        free(cat);
    } else if (!strcmp(trailer, "zero")) {
        word = 0;
    } else {
        return -1;
    }
    if ((size_t)n_raw + 6 > outcap)
        return -1;
    memcpy(out, head, 4);
    memcpy(out + 4, raw, (size_t)n_raw);
    out[4 + n_raw] = (uint8_t)(word >> 8);
    out[5 + n_raw] = (uint8_t)word;
    return n_raw + 6;
}

/* All-absent empty leg (Python ({}, 100, {})). */
static void
empty_leg(enc_core_leg_t *leg)
{
    leg->n = 0;
    for (int b = 0; b < 63; b++)
        leg->refs[b] = -1;
    leg->ref = 100;
    leg->msfb = 1;
}

/* priming_sub1 (fr0 silence audio at -24): zero coeffs on both legs. */
static int
priming_sub1_bb(bytebuf_t *out, const enc_core_drc_t *drc,
                const int *dmx)
{
    enc_core_leg_t e0, e1;
    uint8_t *audio;
    int n_audio, rc = -1;
    (void)dmx;
    audio = malloc(65536);
    if (!audio)
        return -1;
    empty_leg(&e0);
    empty_leg(&e1);
    n_audio = enc_core_write_audio_fullband(&e0, &e1, audio,
                                            65536);
    if (n_audio >= 0 &&
        !sub_audio_bb(out, audio, n_audio, "stereo", 1, drc))
        rc = 0;
    free(audio);
    return rc;
}

/* ---- quantization ---- */

double
enc_core_gain_for_ref(int ref)
{
    return pow(2.0, 0.25 * (ref - 100));
}

int
enc_core_ref_for_peak(double peak, int qmax)
{
    double g, ref;
    int r;
    if (!(peak > 0.0))
        return 100;
    g = peak / pow((double)qmax, 4.0 / 3.0);
    ref = g > 0.0 ? 100.0 + 4.0 * log2(g) : 100.0;
    r = (int)ceil(ref);
    if (r < 0)
        r = 0;
    if (r > 255)
        r = 255;
    return r;
}

/* q = round(copysign(|v|^0.75, v)); -1 on range assert. */
static int
quant_q(double v, int *q)
{
    double m;
    if (v == 0.0) {
        *q = 0;
        return 0;
    }
    m = copysign(pow(fabs(v), 0.75), v);
    *q = pycompat_round_half_even(m);
    if (*q < -ASF_MAX_Q || *q > ASF_MAX_Q)
        return -1;
    return 0;
}

static int
cmp_int_asc(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

int
enc_core_quantize_frame(const double *pcm, int n_pcm, int start,
                        const int *lines, int nlines,
                        const double *window, double gain,
                        enc_core_leg_t *out)
{
    double *Xv;
    int *lo;
    double peak = 0.0;
    double g;
    int ref, n = 0, prev = 0, have = 0;
    if (!pcm || n_pcm < 0 || !lines || nlines < 0 || !out)
        return -1;
    Xv = malloc((size_t)(nlines > 0 ? nlines : 1) *
                sizeof *Xv);
    lo = malloc((size_t)(nlines > 0 ? nlines : 1) *
                sizeof *lo);
    if (!Xv || !lo) {
        free(Xv);
        free(lo);
        return -1;
    }
    for (int k = 0; k < nlines; k++) {
        Xv[k] = mdct_forward_direct_1(pcm, n_pcm, start, 2048,
                                      lines[k], window);
        if (fabs(Xv[k]) > peak)
            peak = fabs(Xv[k]);
        lo[k] = lines[k];
    }
    empty_leg(out);
    if (!(peak * gain > 0.5 * enc_core_gain_for_ref(100))) {
        free(Xv);
        free(lo);
        return 0;
    }
    if (peak * gain > ENC_CORE_X_EMIT_MAX)
        gain = ENC_CORE_X_EMIT_MAX / peak;
    ref = enc_core_ref_for_peak(peak * gain, 8000);
    g = enc_core_gain_for_ref(ref);
    /* sorted(lines) emission; dupes collapse (dict semantics). */
    qsort(lo, (size_t)nlines, sizeof *lo, cmp_int_asc);
    for (int k = 0; k < nlines; k++) {
        double v = 0.0;
        int q = 0, L = lo[k];
        if (have && L == prev)
            continue;
        have = 1;
        prev = L;
        for (int j = 0; j < nlines; j++)
            if (lines[j] == L)
                v = Xv[j];
        v = v * gain / g;
        if (quant_q(v, &q) || (q && n >= 2048)) {
            free(Xv);
            free(lo);
            return -1;
        }
        if (q) {
            out->lines[n] = L;
            out->qs[n] = q;
            n++;
        }
    }
    free(Xv);
    free(lo);
    if (n == 0) {
        empty_leg(out);
        return 0;
    }
    out->n = n;
    out->ref = ref;
    return 0;
}

/* Clamp-walk parallel bands ascending (bands[] pre-sorted). */
static void
smooth_refs(int *bands, int *refs, int n)
{
    int prev = 0, have = 0;
    for (int k = 0; k < n; k++) {
        int r = refs[k];
        (void)bands;
        if (r < 0)
            r = 0;
        if (r > 255)
            r = 255;
        if (have) {
            if (r < prev - 60)
                r = prev - 60;
            if (r > prev + 60)
                r = prev + 60;
        }
        refs[k] = r;
        prev = r;
        have = 1;
    }
}

/* Per-band target refs. Returns nt (>= 0) or -1. peaks indexed by
 * band; tbands ascending. */
static int
band_targets(const double *X, int n, double gain, int ref_shift,
             int max_bands, const int *offs, int nsfb, int cap,
             int floor_adaptive, int *tbands, int *trefs,
             double *peaks, double *gain_out)
{
    double peak = 0.0, floor;
    int nt = 0, nb;
    int *keep;
    if (!X || n < 0 || !offs || nsfb < 1 || nsfb > 63 ||
        !tbands || !trefs || !peaks || !gain_out)
        return -1;
    for (int k = 0; k < n; k++)
        if (fabs(X[k]) > peak)
            peak = fabs(X[k]);
    if (!(peak * gain > 0.5 * enc_core_gain_for_ref(100))) {
        *gain_out = gain;
        return 0;
    }
    if (peak * gain > ENC_CORE_X_EMIT_MAX)
        gain = ENC_CORE_X_EMIT_MAX / peak;
    floor = peak * ENC_CORE_FLOOR_FRAC;
    if (floor_adaptive)
        floor *= pow(2.0, ref_shift / 4.0);
    nb = cap < 0 ? nsfb : (cap < nsfb ? cap : nsfb);
    if (nb < 0)
        return -1;
    for (int b = 0; b < nb; b++) {
        int lo = offs[b], hi = offs[b + 1];
        double pb = 0.0;
        if (hi > n)
            return -1;
        for (int L = lo; L < hi; L++)
            if (fabs(X[L]) > pb)
                pb = fabs(X[L]);
        peaks[b] = pb;
        if (pb >= floor) {
            tbands[nt] = b;
            trefs[nt] =
                enc_core_ref_for_peak(pb * gain, 8000) +
                ref_shift;
            nt++;
        }
    }
    if (nt > max_bands) {
        /* stable desc by peak (insertion; ties keep band order) */
        keep = malloc((size_t)nt * sizeof *keep);
        if (!keep)
            return -1;
        for (int k = 0; k < nt; k++)
            keep[k] = k;
        for (int k = 1; k < nt; k++) {
            int t = keep[k], j = k - 1;
            while (j >= 0 &&
                   peaks[tbands[keep[j]]] < peaks[tbands[t]]) {
                keep[j + 1] = keep[j];
                j--;
            }
            keep[j + 1] = t;
        }
        {
            int kept[64];
            int kre[64];
            for (int k = 0; k < max_bands; k++) {
                kept[k] = tbands[keep[k]];
                kre[k] = trefs[keep[k]];
            }
            /* back to ascending (smooth sorts anyway) */
            for (int k = 0; k < max_bands; k++) {
                tbands[k] = kept[k];
                trefs[k] = kre[k];
            }
            for (int k = 1; k < max_bands; k++) {
                int tb = tbands[k], tr = trefs[k], j = k - 1;
                while (j >= 0 && tbands[j] > tb) {
                    tbands[j + 1] = tbands[j];
                    trefs[j + 1] = trefs[j];
                    j--;
                }
                tbands[j + 1] = tb;
                trefs[j + 1] = tr;
            }
            nt = max_bands;
        }
        free(keep);
    }
    *gain_out = gain;
    return nt;
}

/* Quantize lines with fixed per-band refs. bands ascending. */
static int
quant_with_refs(const double *X, double gain, const int *bands,
                const int *refs, int nrefs, const int *offs,
                int *lines, int *qs, int cap)
{
    int n = 0;
    for (int k = 0; k < nrefs; k++) {
        double g = enc_core_gain_for_ref(refs[k]);
        int lo = offs[bands[k]], hi = offs[bands[k] + 1];
        for (int L = lo; L < hi; L++) {
            double v = X[L] * gain / g;
            int q = 0;
            if (quant_q(v, &q))
                return -1;
            if (q) {
                if (n >= cap)
                    return -1;
                lines[n] = L;
                qs[n] = q;
                n++;
            }
        }
    }
    return n;
}

/* Distinct bands of lines[], ascending, into qb (cap 64). */
static int
live_bands(const int *lines, int n, const int *offs, int nsfb,
           int *qb)
{
    int nqb = 0;
    for (int k = 0; k < n; k++) {
        int b = asf_band_of(lines[k], offs, nsfb);
        int seen = 0;
        if (b < 0)
            return -1;
        for (int j = 0; j < nqb; j++)
            if (qb[j] == b)
                seen = 1;
        if (!seen)
            qb[nqb++] = b;
    }
    qsort(qb, (size_t)nqb, sizeof *qb, cmp_int_asc);
    return nqb;
}

static int
same_bands(const int *a, int na, const int *b, int nb)
{
    if (na != nb)
        return 0;
    for (int k = 0; k < na; k++)
        if (a[k] != b[k])
            return 0;
    return 1;
}

int
enc_core_quantize_bands(const double *X, int n, double gain,
                        int ref_shift, int max_bands,
                        const int *offs, int nsfb, int cap,
                        int floor_adaptive, enc_core_leg_t *out)
{
    const int *o = offs ? offs : sfb_offsets48(2048);
    int tbands[64], trefs[64];
    double peaks[64];
    double gg = gain;
    int bands[64], refs[64];
    int nrefs, nn = 0, broke = 0;
    if (!X || !o || nsfb < 1 || nsfb > 63 || !out)
        return -1;
    empty_leg(out);
    out->msfb = 0;
    nrefs = band_targets(X, n, gain, ref_shift, max_bands, o,
                         nsfb, cap, floor_adaptive, tbands,
                         trefs, peaks, &gg);
    if (nrefs < 0)
        return -1;
    if (nrefs == 0)
        return 0;
    memcpy(bands, tbands, (size_t)nrefs * sizeof *bands);
    memcpy(refs, trefs, (size_t)nrefs * sizeof *refs);
    smooth_refs(bands, refs, nrefs);
    for (int it = 0; it < 3; it++) {
        int qb[64], nqb;
        nn = quant_with_refs(X, gg, bands, refs, nrefs, o,
                             out->lines, out->qs, 2048);
        if (nn < 0)
            return -1;
        nqb = live_bands(out->lines, nn, o, nsfb, qb);
        if (nqb < 0)
            return -1;
        if (same_bands(qb, nqb, bands, nrefs)) {
            broke = 1;
            break;
        }
        {
            int nb[64], nr[64];
            for (int k = 0; k < nqb; k++) {
                nb[k] = qb[k];
                nr[k] = 0;
                for (int j = 0; j < nrefs; j++)
                    if (bands[j] == qb[k])
                        nr[k] = refs[j];
            }
            memcpy(bands, nb, (size_t)nqb * sizeof *bands);
            memcpy(refs, nr, (size_t)nqb * sizeof *refs);
            nrefs = nqb;
            smooth_refs(bands, refs, nrefs);
        }
    }
    if (!broke) {
        int w = 0;
        for (int k = 0; k < nn; k++) {
            int b = asf_band_of(out->lines[k], o, nsfb);
            int keep = 0;
            if (b < 0)
                return -1;
            for (int j = 0; j < nrefs; j++)
                if (bands[j] == b)
                    keep = 1;
            if (keep) {
                out->lines[w] = out->lines[k];
                out->qs[w] = out->qs[k];
                w++;
            }
        }
        nn = w;
    }
    if (nn == 0) {
        empty_leg(out);
        out->msfb = 0;
        return 0;
    }
    out->n = nn;
    for (int k = 0; k < nrefs; k++)
        out->refs[bands[k]] = refs[k];
    out->ref = refs[0];
    return 0;
}

int
enc_core_quantize_full(const double *X, int n, double gain,
                       int ref_shift, int max_bands,
                       int floor_adaptive, enc_core_leg_t *out)
{
    return enc_core_quantize_bands(X, n, gain, ref_shift,
                                   max_bands, NULL,
                                   ENC_CORE_MAX_SFB_FULL, -1,
                                   floor_adaptive, out);
}

/* Clamp-walk per-window targets as one (w, b) chain. */
static void
smooth_chain(int nb[9][64], int nr[9][64], const int *nt)
{
    int prev = 0, have = 0;
    for (int w = 0; w < 9; w++) {
        for (int k = 0; k < nt[w]; k++) {
            int r = nr[w][k];
            if (r < 0)
                r = 0;
            if (r > 255)
                r = 255;
            if (have) {
                if (r < prev - 60)
                    r = prev - 60;
                if (r > prev + 60)
                    r = prev + 60;
            }
            nr[w][k] = r;
            prev = r;
            have = 1;
        }
        (void)nb;
    }
}

int
enc_core_quantize_mixed(const double *const *specs9, int orient,
                        double gain, int ref_shift, int max_bands,
                        enc_core_mixed_leg_t *out, double *gains)
{
    mix_geo_t geo[9];
    int nsfb[9];
    int tb[9][64], tr[9][64], nt[9];
    double peaks[64];
    double gg[9];
    int broke = 0;
    if (!specs9 || !out)
        return -1;
    if (mixed_group_geo(orient, geo))
        return -1;
    for (int w = 0; w < 9; w++) {
        int is1024 = geo[w].sect_bits == 5;
        nsfb[w] = sfb_nbands48(is1024 ? 1024 : 128);
        if (nsfb[w] <= 0)
            return -1;
        nt[w] = band_targets(specs9[w],
                             is1024 ? 1024 : 128, gain,
                             ref_shift, max_bands,
                             geo[w].offs, nsfb[w],
                             geo[w].msfb, 0, tb[w], tr[w],
                             peaks, &gg[w]);
        if (nt[w] < 0)
            return -1;
        if (gains)
            gains[w] = gg[w];
        for (int b = 0; b < 63; b++)
            out->g[w].refs[b] = -1;
        out->g[w].n = 0;
    }
    smooth_chain(tb, tr, nt);
    for (int it = 0; it < 3; it++) {
        int all = 1;
        for (int w = 0; w < 9; w++) {
            int n = quant_with_refs(
                specs9[w], gg[w], tb[w], tr[w], nt[w],
                geo[w].offs, out->g[w].lines, out->g[w].qs,
                1024);
            if (n < 0)
                return -1;
            out->g[w].n = n;
        }
        for (int w = 0; w < 9; w++) {
            int qb[64], nqb;
            nqb = live_bands(out->g[w].lines, out->g[w].n,
                             geo[w].offs, nsfb[w], qb);
            if (nqb < 0)
                return -1;
            if (!same_bands(qb, nqb, tb[w], nt[w])) {
                all = 0;
                break;
            }
        }
        if (all) {
            broke = 1;
            break;
        }
        for (int w = 0; w < 9; w++) {
            int qb[64], nqb, nb[64], nr[64];
            nqb = live_bands(out->g[w].lines, out->g[w].n,
                             geo[w].offs, nsfb[w], qb);
            if (nqb < 0)
                return -1;
            for (int k = 0; k < nqb; k++) {
                nb[k] = qb[k];
                nr[k] = 0;
                for (int j = 0; j < nt[w]; j++)
                    if (tb[w][j] == qb[k])
                        nr[k] = tr[w][j];
            }
            memcpy(tb[w], nb, (size_t)nqb * sizeof *tb[w]);
            memcpy(tr[w], nr, (size_t)nqb * sizeof *tr[w]);
            nt[w] = nqb;
        }
        smooth_chain(tb, tr, nt);
    }
    if (!broke) {
        for (int w = 0; w < 9; w++) {
            int wr = 0;
            for (int k = 0; k < out->g[w].n; k++) {
                int b = asf_band_of(out->g[w].lines[k],
                                    geo[w].offs, nsfb[w]);
                int keep = 0;
                if (b < 0)
                    return -1;
                for (int j = 0; j < nt[w]; j++)
                    if (tb[w][j] == b)
                        keep = 1;
                if (keep) {
                    out->g[w].lines[wr] = out->g[w].lines[k];
                    out->g[w].qs[wr] = out->g[w].qs[k];
                    wr++;
                }
            }
            out->g[w].n = wr;
        }
    }
    for (int w = 0; w < 9; w++)
        for (int k = 0; k < nt[w]; k++)
            out->g[w].refs[tb[w][k]] = tr[w][k];
    return 0;
}

int
enc_core_tile_pure(const double *XL, const double *XR, int n,
                   const int *lines, int nlines)
{
    double tot = 0.0, out = 0.0;
    const double *Xs[2];
    if (!XL || !XR || n < 0 || !lines || nlines < 0)
        return -1;
    Xs[0] = XL;
    Xs[1] = XR;
    for (int c = 0; c < 2; c++) {
        for (int L = 0; L < n; L++) {
            double e = Xs[c][L] * Xs[c][L];
            int in = 0;
            tot += e;
            for (int k = 0; k < nlines; k++)
                if (lines[k] == L) {
                    in = 1;
                    break;
                }
            if (!in)
                out += e;
        }
    }
    if (!(tot > 0.0))
        return 1;
    return out / tot < ENC_CORE_PURE_RATIO ? 1 : 0;
}

/* ---- stereo rate loops ---- */

/* Drain an aligned writer into malloc'd bytes; count or -1. */
static int
w_drain(bitwriter_t *w, uint8_t **out)
{
    size_t n;
    if (bw_nbits(w) % 8 != 0)
        return -1;
    n = bw_nbits(w) / 8;
    *out = malloc(n ? n : 1);
    if (!*out)
        return -1;
    if (bw_bytes(w, *out, n) != n) {
        free(*out);
        *out = NULL;
        return -1;
    }
    return (int)n;
}

int
enc_core_assemble_content_frame(int i, const uint8_t *audio,
                                int n_audio, int raw_size,
                                const char *layout, int iframe,
                                int ns2_content,
                                const enc_core_stamp_t *stamp,
                                const enc_core_drc_t *drc,
                                const int *dmx, uint8_t *out,
                                size_t outcap)
{
    static const uint8_t z10[10] = { 0 };
    static const uint8_t z6[6] = { 0 };
    static const uint8_t z4[4] = { 0 };
    bytebuf_t sub1 = { NULL, 0, 0 };
    const uint8_t *subs[3];
    int nlens[3], nsubs, n = -1;
    if (!audio || n_audio < 0 || !layout || !out)
        return -1;
    iframe = !!iframe;
    if (sub_audio_bb(&sub1, audio, n_audio, layout, iframe,
                     drc))
        goto done;
    if (ns2_content) {
        if (!stamp) {
            if (iframe) {
                subs[0] = z10;
                nlens[0] = ENC_CORE_DEE_SUB0_I;
                subs[1] = sub1.p;
                nlens[1] = (int)sub1.n;
                subs[2] = z4;
                nlens[2] = ENC_CORE_DEE_SUB2_I;
                nsubs = 3;
            } else {
                subs[0] = z6;
                nlens[0] = ENC_CORE_DEE_SUB0_P;
                subs[1] = sub1.p;
                nlens[1] = (int)sub1.n;
                nsubs = 2;
            }
        } else {
            if (!stamp->p6 || !stamp->i12 || !stamp->s4)
                goto done;
            if (iframe) {
                subs[0] = stamp->i12;
                nlens[0] = stamp->n_i12;
                subs[1] = sub1.p;
                nlens[1] = (int)sub1.n;
                subs[2] = stamp->s4;
                nlens[2] = stamp->n_s4;
                nsubs = 3;
            } else {
                subs[0] = stamp->p6;
                nlens[0] = stamp->n_p6;
                subs[1] = sub1.p;
                nlens[1] = (int)sub1.n;
                nsubs = 2;
            }
        }
    } else {
        subs[0] = sub1.p;
        nlens[0] = (int)sub1.n;
        nsubs = 1;
    }
    n = assemble_raw(i, iframe, layout, 0, subs, nlens, nsubs,
                     raw_size, out, outcap);
done:
    (void)dmx;
    bb_free(&sub1);
    return n;
}

static int
try_fit(int i, const double *XL, const double *XR, int shift,
        int kcap, int raw_size, const char *layout, int iframe,
        int ns2_content, const enc_core_stamp_t *stamp,
        const enc_core_drc_t *drc, const int *dmx,
        int floor_adaptive, uint8_t *out, size_t outcap)
{
    enc_core_leg_t cL, cR;
    bitwriter_t w;
    uint8_t *audio = NULL;
    int n_audio, n = -1;
    if (!XL || !XR)
        return -1;
    if (enc_core_quantize_full(XL, 2048, ENC_CORE_DIG_GAIN,
                               shift, kcap, floor_adaptive,
                               &cL))
        return -1;
    if (enc_core_quantize_full(XR, 2048, ENC_CORE_DIG_GAIN,
                               shift, kcap, floor_adaptive,
                               &cR))
        return -1;
    if (cL.n == 0 && cR.n == 0)
        return -1;
    bw_init(&w);
    if (audio_fullband_w(&w, &cL, &cR))
        goto done;
    n_audio = w_drain(&w, &audio);
    if (n_audio < 0)
        goto done;
    n = enc_core_assemble_content_frame(
        i, audio, n_audio, raw_size, layout, iframe,
        ns2_content, stamp, drc, dmx, out, outcap);
done:
    bw_free(&w);
    free(audio);
    return n;
}

int
enc_core_encode_fullband_frame(int i, const double *XL,
                               const double *XR, int raw_size,
                               const char *layout, int iframe,
                               int ns2_content,
                               const enc_core_stamp_t *stamp,
                               const enc_core_drc_t *drc,
                               const int *dmx, int floor_adaptive,
                               uint8_t *out, size_t outcap)
{
    static const int kcaps[6] = { 63, 31, 15, 7, 3, 1 };
    if (!XL || !XR || !layout || !out)
        return -1;
    for (int k = 0; k < 6; k++) {
        int shift = 0;
        for (;;) {
            int n = try_fit(i, XL, XR, shift, kcaps[k],
                            raw_size, layout, iframe,
                            ns2_content, stamp, drc, dmx,
                            floor_adaptive, out, outcap);
            if (n >= 0)
                return n;
            shift += ENC_CORE_SHIFT_STEP;
            if (shift > ENC_CORE_SHIFT_MAX)
                break;
        }
    }
    return -1;
}

/* First qualifying (group, band) ref for a mixed leg. */
static int
leg_ref_first(const enc_core_mixed_leg_t *c, int *ref)
{
    for (int w = 0; w < 9; w++) {
        if (c->g[w].n > 0) {
            for (int b = 0; b < 63; b++) {
                if (c->g[w].refs[b] >= 0) {
                    *ref = c->g[w].refs[b];
                    return 0;
                }
            }
            return -1;
        }
    }
    return -1;
}

static int
mixed_any(const enc_core_mixed_leg_t *c)
{
    for (int w = 0; w < 9; w++)
        if (c->g[w].n > 0)
            return 1;
    return 0;
}

static int
try_fit_mixed(int i, const double *const *specsL9,
              const double *const *specsR9, int orient,
              int shift, int kcap, int raw_size,
              const char *layout, int iframe,
              const enc_core_stamp_t *stamp,
              const enc_core_drc_t *drc, const int *dmx,
              uint8_t *out, size_t outcap, int joint)
{
    enc_core_mixed_leg_t *cL, *cR;
    bitwriter_t w;
    uint8_t *audio = NULL;
    int n_audio, n = -1, refL = 100, refR = 100;
    if (!specsL9 || !specsR9)
        return -1;
    cL = malloc(sizeof *cL);
    cR = malloc(sizeof *cR);
    if (!cL || !cR) {
        free(cL);
        free(cR);
        return -1;
    }
    if (enc_core_quantize_mixed(specsL9, orient,
                                ENC_CORE_DIG_GAIN, shift,
                                kcap, cL, NULL))
        goto done;
    if (enc_core_quantize_mixed(specsR9, orient,
                                ENC_CORE_DIG_GAIN, shift,
                                kcap, cR, NULL))
        goto done;
    if (!mixed_any(cL) && !mixed_any(cR))
        goto done;
    if (mixed_any(cL) && leg_ref_first(cL, &refL))
        goto done;
    if (mixed_any(cR) && leg_ref_first(cR, &refR))
        goto done;
    bw_init(&w);
    if (joint) {
        bw_put(&w, 0, 2);
        bw_put(&w, 1, 1);
        if (sf_info_mixed(&w, orient)) {
            bw_free(&w);
            goto done;
        }
        bw_put(&w, 2, 2);
        if (mixed_write(&w, cL, refL, orient) ||
            mixed_write(&w, cR, refR, orient)) {
            bw_free(&w);
            goto done;
        }
        bw_align(&w, 0);
    } else {
        bw_put(&w, 0, 2);
        bw_put(&w, 0, 1);
        bw_put(&w, 0, 1);
        if (sf_info_mixed(&w, orient)) {
            bw_free(&w);
            goto done;
        }
        bw_put(&w, 0, 1);
        if (sf_info_mixed(&w, orient) ||
            mixed_write(&w, cL, refL, orient) ||
            mixed_write(&w, cR, refR, orient)) {
            bw_free(&w);
            goto done;
        }
        bw_align(&w, 0);
    }
    n_audio = w_drain(&w, &audio);
    bw_free(&w);
    if (n_audio < 0)
        goto done;
    n = enc_core_assemble_content_frame(
        i, audio, n_audio, raw_size, layout, iframe, 1,
        stamp, drc, dmx, out, outcap);
done:
    free(cL);
    free(cR);
    free(audio);
    return n;
}

static int
specs_all_zero(const double *const *a9,
               const double *const *b9, int orient)
{
    /* Valid row lengths by slot (Python scans exact-length
     * lists; zero-padded 1024-stride rows match too). */
    for (int s = 0; s < 2; s++) {
        const double *const *p = s ? b9 : a9;
        for (int g = 0; g < 9; g++) {
            int nn = 128;
            if ((orient == ENC_CORE_ORIENT_1024_FIRST &&
                 g == 0) ||
                (orient == ENC_CORE_ORIENT_128_FIRST &&
                 g == 8))
                nn = 1024;
            for (int k = 0; k < nn; k++)
                if (fabs(p[g][k]) > 0.0)
                    return 0;
        }
    }
    return 1;
}

static int
encode_mixed_loops(int i, const double *const *sA,
                   const double *const *sB, int orient,
                   int raw_size, const char *layout, int iframe,
                   const enc_core_stamp_t *stamp,
                   const enc_core_drc_t *drc, const int *dmx,
                   uint8_t *out, size_t outcap, int joint)
{
    static const int kcaps[5] = { 12, 7, 4, 2, 1 };
    int shift = 0;
    if (!sA || !sB || !layout || !out)
        return -1;
    if (orient != ENC_CORE_ORIENT_1024_FIRST &&
        orient != ENC_CORE_ORIENT_128_FIRST)
        return -1;
    if (specs_all_zero(sA, sB, orient))
        return -1;
    for (;;) {
        int n = try_fit_mixed(i, sA, sB, orient, shift,
                              ENC_CORE_MSFB_MIX_1024,
                              raw_size, layout, iframe, stamp,
                              drc, dmx, out, outcap, joint);
        if (n >= 0)
            return n;
        shift += ENC_CORE_SHIFT_STEP;
        if (shift > ENC_CORE_SHIFT_MAX)
            break;
    }
    shift = 0;
    for (;;) {
        for (int k = 0; k < 5; k++) {
            int n = try_fit_mixed(i, sA, sB, orient, shift,
                                  kcaps[k], raw_size, layout,
                                  iframe, stamp, drc, dmx, out,
                                  outcap, joint);
            if (n >= 0)
                return n;
        }
        shift += ENC_CORE_SHIFT_STEP;
        if (shift > ENC_CORE_SHIFT_MAX)
            return -1;
    }
}

int
enc_core_encode_mixed_frame(int i, const double *const *specsL9,
                            const double *const *specsR9,
                            int orient, int raw_size,
                            const char *layout, int iframe,
                            const enc_core_stamp_t *stamp,
                            const enc_core_drc_t *drc,
                            const int *dmx, uint8_t *out,
                            size_t outcap)
{
    return encode_mixed_loops(i, specsL9, specsR9, orient,
                              raw_size, layout, iframe, stamp,
                              drc, dmx, out, outcap, 0);
}

int
enc_core_encode_joint_mixed_frame(int i,
                                  const double *const *specsM9,
                                  const double *const *specsS9,
                                  int orient, int raw_size,
                                  const char *layout, int iframe,
                                  const enc_core_stamp_t *stamp,
                                  const enc_core_drc_t *drc,
                                  const int *dmx, uint8_t *out,
                                  size_t outcap)
{
    return encode_mixed_loops(i, specsM9, specsS9, orient,
                              raw_size, layout, iframe, stamp,
                              drc, dmx, out, outcap, 1);
}

/* ---- transient detector ---- */

/* 16 plain-KBD 128-line spectra covering [start, start+2048). */
static void
tile_specs_128(const double *pcm, int n_pcm, int start,
               double out[16][128])
{
    for (int k = 0; k < ENC_CORE_N_MIX_TILE128; k++)
        mdct_forward_n(pcm, n_pcm, start + 128 * k, 128,
                       NULL, out[k]);
}

/* max/min-ratio vote over masked HF energies of all channels. */
static int
per_channel_votes(const double *hfs, int nch, const int *valid)
{
    for (int c = 0; c < nch; c++) {
        const double *hf = hfs + c * 16;
        int nh = 0, npos = 0;
        double mn = 0.0, mx = 0.0;
        for (int k = 0; k < 16; k++) {
            double h;
            if (valid && !valid[k])
                continue;
            h = hf[k];
            nh++;
            if (h > 0.0) {
                if (!npos || h < mn)
                    mn = h;
                if (!npos || h > mx)
                    mx = h;
                npos++;
            }
        }
        if (!npos)
            continue;
        if (npos < nh)
            return 1;
        if (mx / mn > ENC_CORE_MIX_HF_RATIO)
            return 1;
    }
    return 0;
}

static int
tile_orient(const double *const *specs, int nch, int start,
            int total, int *o0, int *o1, int mc)
{
    double *hfs;
    double hmax = 0.0;
    int valid[16];
    const int *vp = NULL;
    int rc = 0;
    if (!specs || nch < 1 || nch > 16 || !o0 || !o1)
        return -1;
    hfs = malloc((size_t)nch * 16 * sizeof *hfs);
    if (!hfs)
        return -1;
    for (int c = 0; c < nch; c++) {
        for (int k = 0; k < 16; k++) {
            double e[128 - ENC_CORE_MIX_HF_LO];
            const double *X = specs[c] + k * 128;
            for (int j = ENC_CORE_MIX_HF_LO; j < 128; j++)
                e[j - ENC_CORE_MIX_HF_LO] = X[j] * X[j];
            hfs[c * 16 + k] = pycompat_sum(
                e, 128 - ENC_CORE_MIX_HF_LO);
            if (hfs[c * 16 + k] > hmax)
                hmax = hfs[c * 16 + k];
        }
    }
    if (!(hmax > 0.0))
        goto done;
    if (total >= 0) {
        for (int k = 0; k < 16; k++)
            valid[k] = start + 128 * k - 64 >= 0 &&
                start + 128 * k + 192 <= total;
        vp = valid;
    }
    if (!per_channel_votes(hfs, nch, vp))
        goto done;
    {
        double p0[16], p1[16];
        double e0, e1;
        for (int c = 0; c < nch; c++) {
            p0[c] = pycompat_sum(hfs + c * 16, 8);
            p1[c] = pycompat_sum(hfs + c * 16 + 8, 8);
        }
        if (!mc) {
            /* sum(hf[0:8])+sum(hf[16:24]) over the concat */
            e0 = p0[0] + p0[1];
            e1 = p1[0] + p1[1];
        } else {
            e0 = pycompat_sum(p0, (size_t)nch);
            e1 = pycompat_sum(p1, (size_t)nch);
        }
        if (e1 >= e0) {
            *o0 = ENC_CORE_TL_1024;
            *o1 = ENC_CORE_TL_128;
        } else {
            *o0 = ENC_CORE_TL_128;
            *o1 = ENC_CORE_TL_1024;
        }
        rc = 1;
    }
done:
    free(hfs);
    return rc;
}

int
enc_core_tile_transient_orient(const double *specL16,
                               const double *specR16, int start,
                               int total, int *o0, int *o1)
{
    const double *specs[2];
    if (!specL16 || !specR16)
        return -1;
    specs[0] = specL16;
    specs[1] = specR16;
    return tile_orient(specs, 2, start, total, o0, o1, 0);
}

int
enc_core_tile_transient_orient_mc(const double *const *specs,
                                  int nch, int start, int total,
                                  int *o0, int *o1)
{
    if (!specs || nch < 1)
        return -1;
    return tile_orient(specs, nch, start, total, o0, o1, 1);
}

/* (first, last) window lengths for an orient code. */
static void
frame_edge_lens(int orient, int *first, int *last)
{
    if (orient == ENC_CORE_ORIENT_1024_FIRST) {
        *first = 1024;
        *last = 128;
    } else if (orient == ENC_CORE_ORIENT_128_FIRST) {
        *first = 128;
        *last = 1024;
    } else {
        *first = 2048;
        *last = 2048;
    }
}

int
enc_core_mixed_specs_for_frame(const double *pcm, int n_pcm,
                               int start, int orient,
                               const double *cache16,
                               int edge_prev, int edge_next,
                               double out[9][1024])
{
    double wbuf[2048];
    if (!pcm || n_pcm < 0 || !cache16 || !out)
        return -1;
    memset(out, 0, 9 * 1024 * sizeof(double));
    if (orient == ENC_CORE_ORIENT_1024_FIRST) {
        int tl = 0;
        if (edge_prev < 1024) {
            if (edge_prev != 128 && edge_prev != 256 &&
                edge_prev != 512)
                return -1;
            tl = edge_prev;
        }
        mdct_bridged_window(1024, tl, 128, wbuf);
        mdct_forward_n(pcm, n_pcm, start, 1024, wbuf,
                       out[0]);
        for (int k = 0; k < 8; k++)
            memcpy(out[1 + k], cache16 + (8 + k) * 128,
                   128 * sizeof(double));
        return 0;
    }
    if (orient == ENC_CORE_ORIENT_128_FIRST) {
        int tr = 0;
        if (edge_next < 1024) {
            if (edge_next != 128 && edge_next != 256 &&
                edge_next != 512)
                return -1;
            tr = edge_next;
        }
        mdct_bridged_window(1024, 128, tr, wbuf);
        mdct_forward_n(pcm, n_pcm, start + 1024, 1024, wbuf,
                       out[8]);
        for (int k = 0; k < 8; k++)
            memcpy(out[k], cache16 + k * 128,
                   128 * sizeof(double));
        return 0;
    }
    return -1;
}

/* ---- 5.1 assembly / rate loops ---- */

int
enc_core_assemble_51(int i, const uint8_t *audio, int n_audio,
                     int raw_size, int iframe,
                     const enc_core_stamp_t *stamp51,
                     int content_type, const enc_core_drc_t *drc,
                     const int *dmx, uint8_t *out, size_t outcap)
{
    static const uint8_t z10[10] = { 0 };
    static const uint8_t z6[6] = { 0 };
    static const uint8_t z4[4] = { 0 };
    bytebuf_t sub1 = { NULL, 0, 0 };
    const uint8_t *subs[3];
    int nlens[3], nsubs, n = -1;
    if (!audio || n_audio < 0 || !out)
        return -1;
    iframe = !!iframe;
    if (sub_audio_bb(&sub1, audio, n_audio, "5.1", iframe,
                     drc))
        goto done;
    if (!stamp51) {
        if (iframe) {
            subs[0] = z10;
            nlens[0] = ENC_CORE_DEE_SUB0_I;
            subs[1] = sub1.p;
            nlens[1] = (int)sub1.n;
            subs[2] = z4;
            nlens[2] = ENC_CORE_DEE_SUB2_I;
            nsubs = 3;
        } else {
            subs[0] = z6;
            nlens[0] = ENC_CORE_DEE_SUB0_P;
            subs[1] = sub1.p;
            nlens[1] = (int)sub1.n;
            nsubs = 2;
        }
    } else {
        if (!stamp51->p6 || !stamp51->i12 || !stamp51->s4)
            goto done;
        if (iframe) {
            subs[0] = stamp51->i12;
            nlens[0] = stamp51->n_i12;
            subs[1] = sub1.p;
            nlens[1] = (int)sub1.n;
            subs[2] = stamp51->s4;
            nlens[2] = stamp51->n_s4;
            nsubs = 3;
        } else {
            subs[0] = stamp51->p6;
            nlens[0] = stamp51->n_p6;
            subs[1] = sub1.p;
            nlens[1] = (int)sub1.n;
            nsubs = 2;
        }
    }
    n = assemble_raw(i, iframe, "5.1", content_type ? 1 : 0,
                     subs, nlens, nsubs, raw_size, out,
                     outcap);
done:
    (void)dmx;
    bb_free(&sub1);
    return n;
}

static int
try_fit_51(int i, const double *const *Xs, int shift, int kcap,
           int raw_size, int iframe,
           const enc_core_stamp_t *stamp51,
           const enc_core_drc_t *drc, const int *dmx,
           int floor_adaptive_mains, uint8_t *out,
           size_t outcap)
{
    const int *offs = sfb_offsets48(2048);
    enc_core_leg_t *qs;
    bitwriter_t w;
    uint8_t *audio = NULL;
    int n_audio, n = -1, any = 0;
    if (!Xs || !offs)
        return -1;
    qs = malloc(6 * sizeof *qs);
    if (!qs)
        return -1;
    for (int ch = 0; ch < 6; ch++) {
        int rc;
        if (ch == 3)
            rc = enc_core_quantize_bands(
                Xs[ch], 2048, ENC_CORE_DIG_GAIN, shift,
                kcap, offs, ENC_CORE_MAX_SFB_FULL,
                ENC_CORE_MAX_SFB_LFE, 1, &qs[ch]);
        else
            rc = enc_core_quantize_full(
                Xs[ch], 2048, ENC_CORE_DIG_GAIN, shift,
                kcap, floor_adaptive_mains, &qs[ch]);
        if (rc) {
            free(qs);
            return -1;
        }
        if (qs[ch].n > 0)
            any = 1;
    }
    if (!any)
        goto done;
    bw_init(&w);
    /* WAV order L R C LFE Ls Rs -> (lfe, l, r, ls, rs, c) */
    if (audio_51_w(&w, &qs[3], &qs[0], &qs[1], &qs[4],
                   &qs[5], &qs[2])) {
        bw_free(&w);
        goto done;
    }
    n_audio = w_drain(&w, &audio);
    bw_free(&w);
    if (n_audio < 0)
        goto done;
    n = enc_core_assemble_51(i, audio, n_audio, raw_size,
                             iframe, stamp51, 0, drc, dmx,
                             out, outcap);
done:
    free(qs);
    free(audio);
    return n;
}

static int
fit_51(int i, const double *const *Xs, int raw_size,
       int iframe, const enc_core_stamp_t *stamp51,
       const enc_core_drc_t *drc, const int *dmx,
       int floor_adaptive_mains, uint8_t *out, size_t outcap)
{
    static const int kcaps[6] = { 63, 31, 15, 7, 3, 1 };
    for (int k = 0; k < 6; k++) {
        int shift = 0;
        for (;;) {
            int n = try_fit_51(i, Xs, shift, kcaps[k],
                               raw_size, iframe, stamp51,
                               drc, dmx, floor_adaptive_mains,
                               out, outcap);
            if (n >= 0)
                return n;
            shift += ENC_CORE_SHIFT_STEP;
            if (shift > ENC_CORE_SHIFT_MAX)
                break;
        }
    }
    return -1;
}

static int
try_fit_51_mixed(int i, const double *const *Xs9,
                 const double *XsLFE, int orient, int shift,
                 int kcap, int raw_size, int iframe,
                 const enc_core_stamp_t *stamp51,
                 const enc_core_drc_t *drc, const int *dmx,
                 uint8_t *out, size_t outcap)
{
    const int *offs = sfb_offsets48(2048);
    enc_core_leg_t lfe;
    enc_core_mixed_leg_t *mixed;
    bitwriter_t w;
    uint8_t *audio = NULL;
    int refs[5];
    int n_audio, n = -1, any = 0;
    if (!Xs9 || !XsLFE || !offs)
        return -1;
    mixed = malloc(5 * sizeof *mixed);
    if (!mixed)
        return -1;
    if (enc_core_quantize_bands(XsLFE, 2048, ENC_CORE_DIG_GAIN,
                                shift, kcap, offs,
                                ENC_CORE_MAX_SFB_FULL,
                                ENC_CORE_MAX_SFB_LFE, 1,
                                &lfe)) {
        free(mixed);
        return -1;
    }
    for (int c = 0; c < 5; c++) {
        if (enc_core_quantize_mixed(Xs9 + c * 9, orient,
                                    ENC_CORE_DIG_GAIN, shift,
                                    kcap, &mixed[c], NULL)) {
            free(mixed);
            return -1;
        }
        if (mixed_any(&mixed[c]))
            any = 1;
    }
    if (!any && lfe.n == 0)
        goto done;
    for (int c = 0; c < 5; c++) {
        refs[c] = 100;
        if (mixed_any(&mixed[c]) &&
            leg_ref_first(&mixed[c], &refs[c]))
            goto done;
    }
    bw_init(&w);
    if (audio_51_mixed_w(&w, &lfe, &mixed[0], refs[0],
                         &mixed[1], refs[1], &mixed[3],
                         refs[3], &mixed[4], refs[4],
                         &mixed[2], refs[2], orient)) {
        bw_free(&w);
        goto done;
    }
    n_audio = w_drain(&w, &audio);
    bw_free(&w);
    if (n_audio < 0)
        goto done;
    n = enc_core_assemble_51(i, audio, n_audio, raw_size,
                             iframe, stamp51, 0, drc, dmx,
                             out, outcap);
done:
    free(mixed);
    free(audio);
    return n;
}

static int
fit_51_mixed(int i, const double *const *Xs9,
             const double *XsLFE, int orient, int raw_size,
             int iframe, const enc_core_stamp_t *stamp51,
             const enc_core_drc_t *drc, const int *dmx,
             uint8_t *out, size_t outcap)
{
    static const int kcaps[5] = { 12, 7, 4, 2, 1 };
    int shift = 0;
    for (;;) {
        int n = try_fit_51_mixed(i, Xs9, XsLFE, orient, shift,
                                 63, raw_size, iframe,
                                 stamp51, drc, dmx, out,
                                 outcap);
        if (n >= 0)
            return n;
        shift += ENC_CORE_SHIFT_STEP;
        if (shift > ENC_CORE_SHIFT_MAX)
            break;
    }
    shift = 0;
    for (;;) {
        for (int k = 0; k < 5; k++) {
            int n = try_fit_51_mixed(i, Xs9, XsLFE, orient,
                                     shift, kcaps[k], raw_size,
                                     iframe, stamp51, drc,
                                     dmx, out, outcap);
            if (n >= 0)
                return n;
        }
        shift += ENC_CORE_SHIFT_STEP;
        if (shift > ENC_CORE_SHIFT_MAX)
            return -1;
    }
}

/* ---- full-stream drivers ---- */

/* Half-even round to int with overflow -> -1 (bake path). */
static int
bake_round(double v, int *out)
{
    long long ll;
    if (pycompat_round_half_even_ll(v, &ll))
        return -1;
    if (ll < -2147483647L - 1 || ll > 2147483647L)
        return -1;
    *out = (int)ll;
    return 0;
}

/* int PCM -> exact doubles. */
static double *
pcm_to_double(const int *pcm, int n)
{
    double *d = malloc((size_t)(n > 0 ? n : 1) * sizeof *d);
    if (!d)
        return NULL;
    for (int k = 0; k < n; k++)
        d[k] = (double)pcm[k];
    return d;
}

/* Stereo content frames from double PCM (post-bake baked
 * ints as doubles, or raw rows at target 0). Shared by the int
 * and float entries; bytes or -1. */
static int encode_pcm_frames(const double *dL, int nL,
                             const double *dR, int nR, int rate_kbps,
                             const char *layout, const char *interval,
                             const char *trailer,
                             const double *window, int ns2_tone,
                             int shorts, int joint_shorts,
                             const int *forced, int nforced,
                             const enc_core_drc_t *drc, const int *dmx,
                             int priming, int iframe_base,
                             int iframe_total,
                             const enc_core_stamp_t *stamp,
                             int flat_file, uint8_t *out,
                             size_t outcap);

int
enc_core_encode_pcm(const int *pcmL, int nL, const int *pcmR,
                    int nR, int rate_kbps, const char *layout,
                    const char *interval, const char *trailer,
                    const double *window, int ns2_tone,
                    int shorts, double target_db, int has_bake,
                    double bake_db, int joint_shorts,
                    const int *forced, int nforced,
                    const char *preset, const enc_core_drc_t *drc,
                    const int *dmx, int priming, int iframe_base,
                    int iframe_total, uint8_t *out,
                    size_t outcap)
{
    core_clear_error();
    int *bL = NULL, *bR = NULL;
    double *dL = NULL, *dR = NULL;
    uint8_t p6[6], i12[12], s4[4], fr0[12];
    enc_core_stamp_t stampbuf;
    const enc_core_stamp_t *stamp = NULL;
    int flat_file = 0;
    int n = -1;
    if (!pcmL || !pcmR || nL < 0 || nR < 0 || !layout ||
        !trailer || !out)
        return -1;
    if (strcmp(layout, "stereo"))
        return -1;
    if (strcmp(trailer, "crc") && strcmp(trailer, "zero"))
        return -1;
    if (!(target_db == 0.0 ||
          (-31.0 <= target_db && target_db <= -10.0)))
        return -1;
    dL = pcm_to_double(pcmL, nL);
    dR = pcm_to_double(pcmR, nR);
    if (!dL || !dR)
        goto done;
    if (target_db != 0.0) {
        double m, f, tf, gain, bake;
        int nb = 0, ng = 0, in_peak = 0, peak = 0, sby;
        int sparse, c16, t16, pid, np6, ni12, nfr0;
        loud_integrated(dL, nL, dR, nR, 48000, &m, &nb,
                        &ng);
        loud_flatness(dL, nL, dR, nR, 2048, &f, &tf);
        for (int k = 0; k < nL; k++)
            if (enc_core_abs_sat(pcmL[k]) > in_peak)
                in_peak = enc_core_abs_sat(pcmL[k]);
        for (int k = 0; k < nR; k++)
            if (enc_core_abs_sat(pcmR[k]) > in_peak)
                in_peak = enc_core_abs_sat(pcmR[k]);
        if (has_bake)
            gain = bake_db;
        else if (isinf(m) && m < 0)
            gain = 0.0;
        else
            gain = loud_bake_gain(m, f, tf, target_db);
        bake = pow(10.0, gain / 20.0);
        bL = malloc((size_t)(nL > 0 ? nL : 1) * sizeof *bL);
        bR = malloc((size_t)(nR > 0 ? nR : 1) * sizeof *bR);
        if (!bL || !bR)
            goto done;
        for (int k = 0; k < nL; k++)
            if (bake_round((double)pcmL[k] * bake, &bL[k]))
                goto done;
        for (int k = 0; k < nR; k++)
            if (bake_round((double)pcmR[k] * bake, &bR[k]))
                goto done;
        for (int k = 0; k < nL; k++)
            if (enc_core_abs_sat(bL[k]) > peak)
                peak = enc_core_abs_sat(bL[k]);
        for (int k = 0; k < nR; k++)
            if (enc_core_abs_sat(bR[k]) > peak)
                peak = enc_core_abs_sat(bR[k]);
        if (!(peak < 32767))
            goto done;
        free(dL);
        free(dR);
        dL = pcm_to_double(bL, nL);
        dR = pcm_to_double(bR, nR);
        if (!dL || !dR)
            goto done;
        sby = pycompat_round_half_even(-4.0 * target_db);
        sparse = tf < 0.6 && f > 0.5;
        c16 = enc_core_content16(m, in_peak, f, target_db);
        t16 = enc_core_tail16(sparse, tf);
        pid = enc_core_preset_id(preset ? preset : "manual");
        if (pid < 0)
            goto done;
        np6 = enc_core_compose_sub0(0, sparse, sby, 0, NULL,
                                   pid, p6);
        ni12 = enc_core_compose_sub0(
            1, 0, sby, t16, ENC_CORE_I12_MID_TAIL, pid,
            i12);
        nfr0 = enc_core_compose_sub0(1, sparse, sby, c16,
                                     NULL, pid, fr0);
        if (np6 != 6 || ni12 != 12 ||
            nfr0 != (sparse ? 11 : 12)) {
            stamp_fail("enc_core_encode_pcm", sparse, np6, ni12, nfr0,
                       12, sparse ? 11 : 12);
            goto done;
        }
        memcpy(s4, ENC_CORE_SUB2_DEE, 4);
        stampbuf.p6 = p6;
        stampbuf.n_p6 = 6;
        stampbuf.i12 = i12;
        stampbuf.n_i12 = 12;
        stampbuf.s4 = s4;
        stampbuf.n_s4 = 4;
        stampbuf.fr0 = fr0;
        stampbuf.n_fr0 = nfr0;
        stamp = &stampbuf;
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
    } else {
        double f, tf;
        loud_flatness(dL, nL, dR, nR, 2048, &f, &tf);
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
    }
    n = encode_pcm_frames(dL, nL, dR, nR, rate_kbps, layout,
                          interval, trailer, window, ns2_tone,
                          shorts, joint_shorts, forced, nforced,
                          drc, dmx, priming, iframe_base,
                          iframe_total, stamp, flat_file, out,
                          outcap);
done:
    free(bL);
    free(bR);
    free(dL);
    free(dR);
    return n;
}

static int
encode_pcm_frames(const double *dL, int nL, const double *dR,
                  int nR, int rate_kbps, const char *layout,
                  const char *interval, const char *trailer,
                  const double *window, int ns2_tone, int shorts,
                  int joint_shorts, const int *forced, int nforced,
                  const enc_core_drc_t *drc, const int *dmx,
                  int priming, int iframe_base, int iframe_total,
                  const enc_core_stamp_t *stamp, int flat_file,
                  uint8_t *out, size_t outcap)
{
    /* r2: thin wrapper over the incremental engine (feed all,
     * drain with sync wrap). Byte-identical to the former
     * whole-stream body by construction (same per-frame code). */
    enc_step_ctx_t *st = NULL;
    enc_step_cfg_t cfg;
    const double *rows[2];
    int ns[2];
    int lines[7], nlines, nsamples, nframes;
    uint8_t *raw = NULL;
    int n = -1, pos = 0;
    double freq;
    if (!dL || !dR || nL < 0 || nR < 0 || !layout ||
        !trailer || !out)
        return -1;
    nsamples = nL > nR ? nL : nR;
    if (priming)
        nframes = enc_core_nframes_for_samples(nsamples);
    else if (nsamples <= 0)
        nframes = 0;
    else
        nframes = (nsamples + ENC_CORE_FRAME_SAMPLES - 1) /
            ENC_CORE_FRAME_SAMPLES;
    freq = mdct_estimate_freq(dL, nL, 50.0, 20000.0);
    nlines = mdct_lines_for_freq(freq, 3, lines);
    if (nlines < 0)
        return -1;
    memset(&cfg, 0, sizeof cfg);
    cfg.nch = 2;
    cfg.rate_kbps = rate_kbps;
    cfg.layout = layout;
    cfg.interval = interval;
    cfg.window = window;
    cfg.ns2_tone = ns2_tone;
    cfg.shorts = shorts;
    cfg.joint_shorts = joint_shorts;
    cfg.forced = forced;
    cfg.nforced = nforced;
    cfg.drc = drc;
    cfg.dmx = dmx;
    cfg.priming = priming;
    cfg.iframe_base = iframe_base;
    cfg.iframe_total =
        (!priming && iframe_total < 0) ? nframes
                                       : iframe_total;
    cfg.stamp = stamp;
    cfg.flat_file = flat_file;
    cfg.nlines = nlines;
    memcpy(cfg.lines, lines, sizeof lines);
    if (enc_step_open(&st, &cfg))
        return -1;
    rows[0] = dL;
    rows[1] = dR;
    ns[0] = nL;
    ns[1] = nR;
    if (enc_step_feed(st, rows, ns) || enc_step_end(st))
        goto done;
    raw = malloc(65536);
    if (!raw)
        goto done;
    for (;;) {
        size_t olen = 0;
        int is_if = 0, rc, n_sync;
        rc = enc_step_ready(st);
        if (rc < 0)
            goto done;
        if (rc == 0)
            break;
        rc = enc_step_emit(st, raw, 65536, &olen, &is_if);
        if (rc != 1 || olen > 65536)
            goto done;
        n_sync = enc_core_write_sync_frame(
            raw, (int)olen, trailer, out + pos,
            outcap - (size_t)pos);
        if (n_sync < 0)
            goto done;
        pos += n_sync;
    }
    n = pos;
done:
    free(raw);
    enc_step_close(st);
    return n;
}

/* 5.1 content frames from double PCM (post-bake baked ints
 * as doubles, or raw rows at target 0). Shared by the int and
 * float entries; bytes or -1. */
static int encode_pcm_mc_frames(const double *const *dp,
                                const int *lens, int rate_kbps,
                                const char *interval,
                                const char *trailer,
                                const double *window, int shorts,
                                const int *forced, int nforced,
                                const enc_core_drc_t *drc,
                                const int *dmx,
                                const enc_core_stamp_t *stamp51,
                                int flat_file, uint8_t *out,
                                size_t outcap);

int
enc_core_encode_pcm_mc(const int *const *chs, const int *lens,
                       int rate_kbps, const char *interval,
                       const char *trailer,
                       const double *window, double target_db,
                       int has_bake, double bake_db, int shorts,
                       const int *const *meter_chs,
                       const int *meter_lens,
                       const int *forced, int nforced,
                       const char *preset,
                       const enc_core_drc_t *drc, const int *dmx,
                       uint8_t *out, size_t outcap)
{
    core_clear_error();
    int *pcm[6];
    double *dp[6];
    uint8_t p6[6], i15[15], s4[4], fr0[15];
    enc_core_stamp_t stampbuf;
    const enc_core_stamp_t *stamp51 = NULL;
    int n6 = 0;
    int flat_file = 0;
    int n = -1;
    if (!chs || !lens || !trailer || !out)
        return -1;
    if (strcmp(trailer, "crc") && strcmp(trailer, "zero"))
        return -1;
    if (!(target_db == 0.0 ||
          (-31.0 <= target_db && target_db <= -10.0)))
        return -1;
    for (int c = 0; c < 6; c++) {
        pcm[c] = NULL;
        dp[c] = NULL;
        if (!chs[c] || lens[c] < 0)
            goto done;
        if (lens[c] > n6)
            n6 = lens[c];
    }
    for (int c = 0; c < 6; c++) {
        pcm[c] = malloc(
            (size_t)(lens[c] > 0 ? lens[c] : 1) *
            sizeof *pcm[c]);
        if (!pcm[c])
            goto done;
        memcpy(pcm[c], chs[c],
               (size_t)lens[c] * sizeof *pcm[c]);
        dp[c] = pcm_to_double(pcm[c], lens[c]);
        if (!dp[c])
            goto done;
    }
    if (target_db != 0.0) {
        const int *met[6];
        int metlens[6];
        double *dm[6];
        double *mono6 = NULL;
        double m, f, tf, gain, bake, lfe_bake, lin[6];
        int nb = 0, ng = 0, peak = 0, sby, c16, pid;
        int hits51, np6, ni15, nfr0, nm = 0;
        for (int c = 0; c < 6; c++) {
            dm[c] = NULL;
            if (meter_chs) {
                if (!meter_chs[c] || !meter_lens ||
                    meter_lens[c] < 0)
                    goto done;
                met[c] = meter_chs[c];
                metlens[c] = meter_lens[c];
            } else {
                met[c] = pcm[c];
                metlens[c] = lens[c];
            }
            if (metlens[c] > nm)
                nm = metlens[c];
        }
        for (int c = 0; c < 6; c++) {
            dm[c] = pcm_to_double(met[c], metlens[c]);
            if (!dm[c]) {
                for (int k = 0; k < c; k++)
                    free(dm[k]);
                goto done;
            }
        }
        {
            const double *dmp[6];
            for (int c = 0; c < 6; c++)
                dmp[c] = dm[c];
            loud_integrated_mc(dmp, metlens, 48000, &m,
                               &nb, &ng);
        }
        mono6 = malloc((size_t)(nm > 0 ? nm : 1) *
                       sizeof *mono6);
        if (!mono6) {
            for (int c = 0; c < 6; c++)
                free(dm[c]);
            goto done;
        }
        for (int k = 0; k < nm; k++) {
            long long s = 0;
            for (int c = 0; c < 6; c++)
                s += k < metlens[c] ? met[c][k] : 0;
            mono6[k] = (double)s / 6.0;
        }
        loud_flatness(mono6, nm, mono6, nm, 2048, &f, &tf);
        free(mono6);
        for (int c = 0; c < 6; c++) {
            for (int k = 0; k < metlens[c]; k++)
                if (enc_core_abs_sat(met[c][k]) > peak)
                    peak = enc_core_abs_sat(met[c][k]);
            free(dm[c]);
        }
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
        hits51 = tf < 0.6 && f > 0.5;
        if (has_bake)
            gain = bake_db;
        else if (isinf(m) && m < 0)
            gain = 0.0;
        else if (hits51)
            gain = ENC_CORE_T_HIT_51_DB -
                (m > -23.0 ? m : -23.0) +
                (target_db + 24.0);
        else
            gain = target_db - m;
        bake = pow(10.0, gain / 20.0);
        for (int c = 0; c < 6; c++)
            lin[c] = pow(
                10.0, ENC_CORE_LEG_COMP_51_DB[c] / 20.0);
        lfe_bake = hits51
            ? pow(10.0, ENC_CORE_LFE_HITS_BAKE_DB / 20.0)
            : bake;
        for (int c = 0; c < 6; c++) {
            double bc = c == 3 ? lfe_bake : bake;
            for (int k = 0; k < lens[c]; k++) {
                double v =
                    (double)pcm[c][k] * bc * lin[c];
                if (bake_round(v, &pcm[c][k]))
                    goto done;
            }
        }
        {
            int pk = 0;
            for (int c = 0; c < 6; c++)
                for (int k = 0; k < lens[c]; k++)
                    if (enc_core_abs_sat(pcm[c][k]) > pk)
                        pk = enc_core_abs_sat(pcm[c][k]);
            if (!(pk < 32767))
                goto done;
        }
        for (int c = 0; c < 6; c++) {
            free(dp[c]);
            dp[c] = pcm_to_double(pcm[c], lens[c]);
            if (!dp[c])
                goto done;
        }
        sby = pycompat_round_half_even(-4.0 * target_db);
        c16 = enc_core_content16(m, peak, f, target_db);
        pid = enc_core_preset_id(preset ? preset : "manual");
        if (pid < 0)
            goto done;
        nfr0 = enc_core_compose_sub0_51(sby, c16, NULL, pid,
                                        dmx, fr0);
        ni15 = enc_core_compose_sub0_51(
            sby, ENC_CORE_TAIL16_DENSE,
            ENC_CORE_I12_MID_TAIL, pid, dmx, i15);
        np6 = enc_core_compose_sub0(0, 0, sby, 0, NULL, pid,
                                   p6);
        if (nfr0 != 15 || ni15 != 15 || np6 != 6) {
            stamp_fail("enc_core_encode_pcm_mc", 0, np6, ni15, nfr0, 15,
                       15);
            goto done;
        }
        memcpy(s4, ENC_CORE_SUB2_DEE, 4);
        stampbuf.p6 = p6;
        stampbuf.n_p6 = 6;
        stampbuf.i12 = i15;
        stampbuf.n_i12 = 15;
        stampbuf.s4 = s4;
        stampbuf.n_s4 = 4;
        stampbuf.fr0 = fr0;
        stampbuf.n_fr0 = 15;
        stamp51 = &stampbuf;
    } else {
        double *mono6 = malloc(
            (size_t)(n6 > 0 ? n6 : 1) * sizeof *mono6);
        double f, tf;
        if (!mono6)
            goto done;
        for (int k = 0; k < n6; k++) {
            long long s = 0;
            for (int c = 0; c < 6; c++)
                s += k < lens[c] ? pcm[c][k] : 0;
            mono6[k] = (double)s / 6.0;
        }
        loud_flatness(mono6, n6, mono6, n6, 2048, &f, &tf);
        free(mono6);
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
    }
    {
        const double *dpp[6];
        for (int c = 0; c < 6; c++)
            dpp[c] = dp[c];
        n = encode_pcm_mc_frames(dpp, lens, rate_kbps, interval,
                                 trailer, window, shorts, forced,
                                 nforced, drc, dmx, stamp51,
                                 flat_file, out, outcap);
    }
done:
    for (int c = 0; c < 6; c++) {
        free(pcm[c]);
        free(dp[c]);
    }
    return n;
}

static int
encode_pcm_mc_frames(const double *const *dp, const int *lens,
                     int rate_kbps, const char *interval,
                     const char *trailer, const double *window,
                     int shorts, const int *forced, int nforced,
                     const enc_core_drc_t *drc, const int *dmx,
                     const enc_core_stamp_t *stamp51,
                     int flat_file, uint8_t *out,
                     size_t outcap)
{
    /* r2: thin wrapper over the incremental engine (feed all,
     * drain with sync wrap). Byte-identical to the former
     * whole-stream body by construction (same per-frame code). */
    enc_step_ctx_t *st = NULL;
    enc_step_cfg_t cfg;
    uint8_t *raw = NULL;
    int n = -1, pos = 0;
    if (!dp || !lens || !trailer || !out)
        return -1;
    for (int ch = 0; ch < 6; ch++)
        if (!dp[ch] || lens[ch] < 0)
            return -1;
    memset(&cfg, 0, sizeof cfg);
    cfg.nch = 6;
    cfg.rate_kbps = rate_kbps;
    cfg.interval = interval;
    cfg.window = window;
    cfg.shorts = shorts;
    cfg.forced = forced;
    cfg.nforced = nforced;
    cfg.drc = drc;
    cfg.dmx = dmx;
    cfg.stamp = stamp51;
    cfg.flat_file = flat_file;
    if (enc_step_open(&st, &cfg))
        return -1;
    if (enc_step_feed(st, dp, lens) || enc_step_end(st))
        goto done;
    raw = malloc(65536);
    if (!raw)
        goto done;
    for (;;) {
        size_t olen = 0;
        int is_if = 0, rc, n_sync;
        rc = enc_step_ready(st);
        if (rc < 0)
            goto done;
        if (rc == 0)
            break;
        rc = enc_step_emit(st, raw, 65536, &olen, &is_if);
        if (rc != 1 || olen > 65536)
            goto done;
        n_sync = enc_core_write_sync_frame(
            raw, (int)olen, trailer, out + pos,
            outcap - (size_t)pos);
        if (n_sync < 0)
            goto done;
        pos += n_sync;
    }
    n = pos;
done:
    free(raw);
    enc_step_close(st);
    return n;
}

/* Copy caller double rows (post-fold PCM). */
static double *
dbl_copy(const double *pcm, int n)
{
    double *d = malloc((size_t)(n > 0 ? n : 1) * sizeof *d);
    if (!d)
        return NULL;
    if (n > 0)
        memcpy(d, pcm, (size_t)n * sizeof *d);
    return d;
}

int
enc_core_encode_pcm_f(const double *pcmL, int nL,
                      const double *pcmR, int nR, int rate_kbps,
                      const char *layout, const char *interval,
                      const char *trailer,
                      const double *window, int ns2_tone,
                      int shorts, double target_db, int has_bake,
                      double bake_db, int joint_shorts,
                      const int *forced, int nforced,
                      const char *preset, const enc_core_drc_t *drc,
                      const int *dmx, int priming, int iframe_base,
                      int iframe_total, uint8_t *out,
                      size_t outcap)
{
    core_clear_error();
    int *bL = NULL, *bR = NULL;
    double *dL = NULL, *dR = NULL;
    uint8_t p6[6], i12[12], s4[4], fr0[12];
    enc_core_stamp_t stampbuf;
    const enc_core_stamp_t *stamp = NULL;
    int flat_file = 0;
    int n = -1;
    if (!pcmL || !pcmR || nL < 0 || nR < 0 || !layout ||
        !trailer || !out)
        return -1;
    if (strcmp(layout, "stereo"))
        return -1;
    if (strcmp(trailer, "crc") && strcmp(trailer, "zero"))
        return -1;
    if (!(target_db == 0.0 ||
          (-31.0 <= target_db && target_db <= -10.0)))
        return -1;
    dL = dbl_copy(pcmL, nL);
    dR = dbl_copy(pcmR, nR);
    if (!dL || !dR)
        goto done;
    if (target_db != 0.0) {
        double m, f, tf, gain, bake, in_peak = 0.0;
        int nb = 0, ng = 0, peak = 0, sby;
        int sparse, c16, t16, pid, np6, ni12, nfr0;
        loud_integrated(dL, nL, dR, nR, 48000, &m, &nb,
                        &ng);
        loud_flatness(dL, nL, dR, nR, 2048, &f, &tf);
        for (int k = 0; k < nL; k++)
            if (fabs(dL[k]) > in_peak)
                in_peak = fabs(dL[k]);
        for (int k = 0; k < nR; k++)
            if (fabs(dR[k]) > in_peak)
                in_peak = fabs(dR[k]);
        if (has_bake)
            gain = bake_db;
        else if (isinf(m) && m < 0)
            gain = 0.0;
        else
            gain = loud_bake_gain(m, f, tf, target_db);
        bake = pow(10.0, gain / 20.0);
        bL = malloc((size_t)(nL > 0 ? nL : 1) * sizeof *bL);
        bR = malloc((size_t)(nR > 0 ? nR : 1) * sizeof *bR);
        if (!bL || !bR)
            goto done;
        for (int k = 0; k < nL; k++)
            if (bake_round(dL[k] * bake, &bL[k]))
                goto done;
        for (int k = 0; k < nR; k++)
            if (bake_round(dR[k] * bake, &bR[k]))
                goto done;
        for (int k = 0; k < nL; k++)
            if (enc_core_abs_sat(bL[k]) > peak)
                peak = enc_core_abs_sat(bL[k]);
        for (int k = 0; k < nR; k++)
            if (enc_core_abs_sat(bR[k]) > peak)
                peak = enc_core_abs_sat(bR[k]);
        if (!(peak < 32767))
            goto done;
        free(dL);
        free(dR);
        dL = pcm_to_double(bL, nL);
        dR = pcm_to_double(bR, nR);
        if (!dL || !dR)
            goto done;
        sby = pycompat_round_half_even(-4.0 * target_db);
        sparse = tf < 0.6 && f > 0.5;
        c16 = enc_core_content16_f(m, in_peak, f, target_db);
        t16 = enc_core_tail16(sparse, tf);
        pid = enc_core_preset_id(preset ? preset : "manual");
        if (pid < 0)
            goto done;
        np6 = enc_core_compose_sub0(0, sparse, sby, 0, NULL,
                                   pid, p6);
        ni12 = enc_core_compose_sub0(
            1, 0, sby, t16, ENC_CORE_I12_MID_TAIL, pid,
            i12);
        nfr0 = enc_core_compose_sub0(1, sparse, sby, c16,
                                     NULL, pid, fr0);
        if (np6 != 6 || ni12 != 12 ||
            nfr0 != (sparse ? 11 : 12)) {
            stamp_fail("enc_core_encode_pcm_f", sparse, np6, ni12, nfr0,
                       12, sparse ? 11 : 12);
            goto done;
        }
        memcpy(s4, ENC_CORE_SUB2_DEE, 4);
        stampbuf.p6 = p6;
        stampbuf.n_p6 = 6;
        stampbuf.i12 = i12;
        stampbuf.n_i12 = 12;
        stampbuf.s4 = s4;
        stampbuf.n_s4 = 4;
        stampbuf.fr0 = fr0;
        stampbuf.n_fr0 = nfr0;
        stamp = &stampbuf;
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
    } else {
        double f, tf;
        loud_flatness(dL, nL, dR, nR, 2048, &f, &tf);
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
    }
    n = encode_pcm_frames(dL, nL, dR, nR, rate_kbps, layout,
                          interval, trailer, window, ns2_tone,
                          shorts, joint_shorts, forced, nforced,
                          drc, dmx, priming, iframe_base,
                          iframe_total, stamp, flat_file, out,
                          outcap);
done:
    free(bL);
    free(bR);
    free(dL);
    free(dR);
    return n;
}

int
enc_core_encode_pcm_mc_f(const double *const *chs,
                         const int *lens, int rate_kbps,
                         const char *interval, const char *trailer,
                         const double *window, double target_db,
                         int has_bake, double bake_db, int shorts,
                         const double *const *meter_chs,
                         const int *meter_lens, const int *forced,
                         int nforced, const char *preset,
                         const enc_core_drc_t *drc, const int *dmx,
                         uint8_t *out, size_t outcap)
{
    core_clear_error();
    double *fc[6];
    int *pcm[6];
    double *dp[6];
    uint8_t p6[6], i15[15], s4[4], fr0[15];
    enc_core_stamp_t stampbuf;
    const enc_core_stamp_t *stamp51 = NULL;
    int n6 = 0;
    int flat_file = 0;
    int n = -1;
    if (!chs || !lens || !trailer || !out)
        return -1;
    if (strcmp(trailer, "crc") && strcmp(trailer, "zero"))
        return -1;
    if (!(target_db == 0.0 ||
          (-31.0 <= target_db && target_db <= -10.0)))
        return -1;
    for (int c = 0; c < 6; c++) {
        fc[c] = NULL;
        pcm[c] = NULL;
        dp[c] = NULL;
        if (!chs[c] || lens[c] < 0)
            goto done;
        if (lens[c] > n6)
            n6 = lens[c];
    }
    for (int c = 0; c < 6; c++) {
        fc[c] = dbl_copy(chs[c], lens[c]);
        if (!fc[c])
            goto done;
    }
    if (target_db != 0.0) {
        const double *met[6];
        int metlens[6];
        double *mono6 = NULL;
        double m, f, tf, gain, bake, lfe_bake, lin[6];
        double peak = 0.0;
        int nb = 0, ng = 0, sby, c16, pid;
        int hits51, np6, ni15, nfr0, nm = 0;
        for (int c = 0; c < 6; c++) {
            if (meter_chs) {
                if (!meter_chs[c] || !meter_lens ||
                    meter_lens[c] < 0)
                    goto done;
                met[c] = meter_chs[c];
                metlens[c] = meter_lens[c];
            } else {
                met[c] = fc[c];
                metlens[c] = lens[c];
            }
            if (metlens[c] > nm)
                nm = metlens[c];
        }
        loud_integrated_mc(met, metlens, 48000, &m, &nb,
                           &ng);
        mono6 = malloc((size_t)(nm > 0 ? nm : 1) *
                       sizeof *mono6);
        if (!mono6)
            goto done;
        for (int k = 0; k < nm; k++) {
            double v6[6];
            for (int c = 0; c < 6; c++)
                v6[c] = k < metlens[c] ? met[c][k] : 0.0;
            mono6[k] = pycompat_sum(v6, 6) / 6.0;
        }
        loud_flatness(mono6, nm, mono6, nm, 2048, &f, &tf);
        free(mono6);
        for (int c = 0; c < 6; c++)
            for (int k = 0; k < metlens[c]; k++)
                if (fabs(met[c][k]) > peak)
                    peak = fabs(met[c][k]);
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
        hits51 = tf < 0.6 && f > 0.5;
        if (has_bake)
            gain = bake_db;
        else if (isinf(m) && m < 0)
            gain = 0.0;
        else if (hits51)
            gain = ENC_CORE_T_HIT_51_DB -
                (m > -23.0 ? m : -23.0) +
                (target_db + 24.0);
        else
            gain = target_db - m;
        bake = pow(10.0, gain / 20.0);
        for (int c = 0; c < 6; c++)
            lin[c] = pow(
                10.0, ENC_CORE_LEG_COMP_51_DB[c] / 20.0);
        lfe_bake = hits51
            ? pow(10.0, ENC_CORE_LFE_HITS_BAKE_DB / 20.0)
            : bake;
        for (int c = 0; c < 6; c++) {
            double bc = c == 3 ? lfe_bake : bake;
            pcm[c] = malloc((size_t)(lens[c] > 0 ? lens[c]
                                                 : 1) *
                            sizeof *pcm[c]);
            if (!pcm[c])
                goto done;
            for (int k = 0; k < lens[c]; k++) {
                double v = fc[c][k] * bc * lin[c];
                if (bake_round(v, &pcm[c][k]))
                    goto done;
            }
        }
        {
            int pk = 0;
            for (int c = 0; c < 6; c++)
                for (int k = 0; k < lens[c]; k++)
                    if (enc_core_abs_sat(pcm[c][k]) > pk)
                        pk = enc_core_abs_sat(pcm[c][k]);
            if (!(pk < 32767))
                goto done;
        }
        for (int c = 0; c < 6; c++) {
            dp[c] = pcm_to_double(pcm[c], lens[c]);
            if (!dp[c])
                goto done;
        }
        sby = pycompat_round_half_even(-4.0 * target_db);
        c16 = enc_core_content16_f(m, peak, f, target_db);
        pid = enc_core_preset_id(preset ? preset : "manual");
        if (pid < 0)
            goto done;
        nfr0 = enc_core_compose_sub0_51(sby, c16, NULL, pid,
                                        dmx, fr0);
        ni15 = enc_core_compose_sub0_51(
            sby, ENC_CORE_TAIL16_DENSE,
            ENC_CORE_I12_MID_TAIL, pid, dmx, i15);
        np6 = enc_core_compose_sub0(0, 0, sby, 0, NULL, pid,
                                   p6);
        if (nfr0 != 15 || ni15 != 15 || np6 != 6) {
            stamp_fail("enc_core_encode_pcm_mc_f", 0, np6, ni15, nfr0, 15,
                       15);
            goto done;
        }
        memcpy(s4, ENC_CORE_SUB2_DEE, 4);
        stampbuf.p6 = p6;
        stampbuf.n_p6 = 6;
        stampbuf.i12 = i15;
        stampbuf.n_i12 = 15;
        stampbuf.s4 = s4;
        stampbuf.n_s4 = 4;
        stampbuf.fr0 = fr0;
        stampbuf.n_fr0 = 15;
        stamp51 = &stampbuf;
    } else {
        double *mono6 = malloc(
            (size_t)(n6 > 0 ? n6 : 1) * sizeof *mono6);
        double f, tf;
        if (!mono6)
            goto done;
        for (int k = 0; k < n6; k++) {
            double v6[6];
            for (int c = 0; c < 6; c++)
                v6[c] = k < lens[c] ? fc[c][k] : 0.0;
            mono6[k] = pycompat_sum(v6, 6) / 6.0;
        }
        loud_flatness(mono6, n6, mono6, n6, 2048, &f, &tf);
        free(mono6);
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
        for (int c = 0; c < 6; c++) {
            dp[c] = dbl_copy(fc[c], lens[c]);
            if (!dp[c])
                goto done;
        }
    }
    {
        const double *dpp[6];
        for (int c = 0; c < 6; c++)
            dpp[c] = dp[c];
        n = encode_pcm_mc_frames(dpp, lens, rate_kbps,
                                 interval, trailer, window,
                                 shorts, forced, nforced, drc,
                                 dmx, stamp51, flat_file, out,
                                 outcap);
    }
done:
    for (int c = 0; c < 6; c++) {
        free(fc[c]);
        free(pcm[c]);
        free(dp[c]);
    }
    return n;
}

/* ---- ac4enc-lib H1: caller-measured entries ----
 * Duplicates of the four drivers above with the pass-1
 * loudness measure supplied by the caller (ms) instead of
 * computed from the input. ms==NULL selects the NEUTRAL path
 * (no bake, no stamp, flat_file=0). Bake/stamp op order is
 * copied verbatim from the originals; when ms matches the
 * input's true measure the output is bit-identical to the
 * corresponding original driver. */

int
enc_core_encode_pcm_m(const int *pcmL, int nL, const int *pcmR,
                      int nR, int rate_kbps, const char *layout,
                      const char *interval, const char *trailer,
                      const double *window, int ns2_tone,
                      int shorts, double target_db, int has_bake,
                      double bake_db, int joint_shorts,
                      const int *forced, int nforced,
                      const char *preset,
                      const enc_core_drc_t *drc, const int *dmx,
                      int priming, int iframe_base,
                      int iframe_total,
                      const enc_core_measure_t *ms, uint8_t *out,
                      size_t outcap)
{
    core_clear_error();
    int *bL = NULL, *bR = NULL;
    double *dL = NULL, *dR = NULL;
    uint8_t p6[6], i12[12], s4[4], fr0[12];
    enc_core_stamp_t stampbuf;
    const enc_core_stamp_t *stamp = NULL;
    int flat_file = 0;
    int n = -1;
    if (!pcmL || !pcmR || nL < 0 || nR < 0 || !layout ||
        !trailer || !out)
        return -1;
    if (strcmp(layout, "stereo"))
        return -1;
    if (strcmp(trailer, "crc") && strcmp(trailer, "zero"))
        return -1;
    if (!(target_db == 0.0 ||
          (-31.0 <= target_db && target_db <= -10.0)))
        return -1;
    dL = pcm_to_double(pcmL, nL);
    dR = pcm_to_double(pcmR, nR);
    if (!dL || !dR)
        goto done;
    if (ms && target_db != 0.0) {
        double m = ms->m_db, f = ms->f_flat;
        double tf = ms->tf_tone, gain, bake;
        int in_peak = ms->in_peak, peak = 0, sby;
        int sparse, c16, t16, pid, np6, ni12, nfr0;
        if (has_bake)
            gain = bake_db;
        else if (isinf(m) && m < 0)
            gain = 0.0;
        else
            gain = loud_bake_gain(m, f, tf, target_db);
        bake = pow(10.0, gain / 20.0);
        bL = malloc((size_t)(nL > 0 ? nL : 1) * sizeof *bL);
        bR = malloc((size_t)(nR > 0 ? nR : 1) * sizeof *bR);
        if (!bL || !bR)
            goto done;
        for (int k = 0; k < nL; k++)
            if (bake_round((double)pcmL[k] * bake, &bL[k]))
                goto done;
        for (int k = 0; k < nR; k++)
            if (bake_round((double)pcmR[k] * bake, &bR[k]))
                goto done;
        for (int k = 0; k < nL; k++)
            if (enc_core_abs_sat(bL[k]) > peak)
                peak = enc_core_abs_sat(bL[k]);
        for (int k = 0; k < nR; k++)
            if (enc_core_abs_sat(bR[k]) > peak)
                peak = enc_core_abs_sat(bR[k]);
        if (!(peak < 32767))
            goto done;
        free(dL);
        free(dR);
        dL = pcm_to_double(bL, nL);
        dR = pcm_to_double(bR, nR);
        if (!dL || !dR)
            goto done;
        sby = pycompat_round_half_even(-4.0 * target_db);
        sparse = tf < 0.6 && f > 0.5;
        c16 = enc_core_content16(m, in_peak, f, target_db);
        t16 = enc_core_tail16(sparse, tf);
        pid = enc_core_preset_id(preset ? preset : "manual");
        if (pid < 0)
            goto done;
        np6 = enc_core_compose_sub0(0, sparse, sby, 0, NULL,
                                   pid, p6);
        ni12 = enc_core_compose_sub0(
            1, 0, sby, t16, ENC_CORE_I12_MID_TAIL, pid,
            i12);
        nfr0 = enc_core_compose_sub0(1, sparse, sby, c16,
                                     NULL, pid, fr0);
        if (np6 != 6 || ni12 != 12 ||
            nfr0 != (sparse ? 11 : 12)) {
            stamp_fail("enc_core_encode_pcm_m", sparse, np6, ni12, nfr0,
                       12, sparse ? 11 : 12);
            goto done;
        }
        memcpy(s4, ENC_CORE_SUB2_DEE, 4);
        stampbuf.p6 = p6;
        stampbuf.n_p6 = 6;
        stampbuf.i12 = i12;
        stampbuf.n_i12 = 12;
        stampbuf.s4 = s4;
        stampbuf.n_s4 = 4;
        stampbuf.fr0 = fr0;
        stampbuf.n_fr0 = nfr0;
        stamp = &stampbuf;
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
    } else if (ms) {
        flat_file = ms->f_flat > ENC_CORE_FLAT_F_THRESH;
    } else {
        flat_file = 0;
    }
    n = encode_pcm_frames(dL, nL, dR, nR, rate_kbps, layout,
                          interval, trailer, window, ns2_tone,
                          shorts, joint_shorts, forced, nforced,
                          drc, dmx, priming, iframe_base,
                          iframe_total, stamp, flat_file, out,
                          outcap);
done:
    free(bL);
    free(bR);
    free(dL);
    free(dR);
    return n;
}

int
enc_core_encode_pcm_mc_m(const int *const *chs, const int *lens,
                         int rate_kbps, const char *interval,
                         const char *trailer,
                         const double *window, double target_db,
                         int has_bake, double bake_db, int shorts,
                         const int *forced, int nforced,
                         const char *preset,
                         const enc_core_drc_t *drc, const int *dmx,
                         const enc_core_measure_t *ms, uint8_t *out,
                         size_t outcap)
{
    core_clear_error();
    int *pcm[6];
    double *dp[6];
    uint8_t p6[6], i15[15], s4[4], fr0[15];
    enc_core_stamp_t stampbuf;
    const enc_core_stamp_t *stamp51 = NULL;
    int flat_file = 0;
    int n = -1;
    if (!chs || !lens || !trailer || !out)
        return -1;
    if (strcmp(trailer, "crc") && strcmp(trailer, "zero"))
        return -1;
    if (!(target_db == 0.0 ||
          (-31.0 <= target_db && target_db <= -10.0)))
        return -1;
    for (int c = 0; c < 6; c++) {
        pcm[c] = NULL;
        dp[c] = NULL;
        if (!chs[c] || lens[c] < 0)
            goto done;
    }
    for (int c = 0; c < 6; c++) {
        pcm[c] = malloc(
            (size_t)(lens[c] > 0 ? lens[c] : 1) *
            sizeof *pcm[c]);
        if (!pcm[c])
            goto done;
        memcpy(pcm[c], chs[c],
               (size_t)lens[c] * sizeof *pcm[c]);
        dp[c] = pcm_to_double(pcm[c], lens[c]);
        if (!dp[c])
            goto done;
    }
    if (ms && target_db != 0.0) {
        double m = ms->m_db, f = ms->f_flat;
        double tf = ms->tf_tone;
        double gain, bake, lfe_bake, lin[6];
        int peak = ms->in_peak, sby, c16, pid;
        int hits51, np6, ni15, nfr0;
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
        hits51 = tf < 0.6 && f > 0.5;
        if (has_bake)
            gain = bake_db;
        else if (isinf(m) && m < 0)
            gain = 0.0;
        else if (hits51)
            gain = ENC_CORE_T_HIT_51_DB -
                (m > -23.0 ? m : -23.0) +
                (target_db + 24.0);
        else
            gain = target_db - m;
        bake = pow(10.0, gain / 20.0);
        for (int c = 0; c < 6; c++)
            lin[c] = pow(
                10.0, ENC_CORE_LEG_COMP_51_DB[c] / 20.0);
        lfe_bake = hits51
            ? pow(10.0, ENC_CORE_LFE_HITS_BAKE_DB / 20.0)
            : bake;
        for (int c = 0; c < 6; c++) {
            double bc = c == 3 ? lfe_bake : bake;
            for (int k = 0; k < lens[c]; k++) {
                double v =
                    (double)pcm[c][k] * bc * lin[c];
                if (bake_round(v, &pcm[c][k]))
                    goto done;
            }
        }
        {
            int pk = 0;
            for (int c = 0; c < 6; c++)
                for (int k = 0; k < lens[c]; k++)
                    if (enc_core_abs_sat(pcm[c][k]) > pk)
                        pk = enc_core_abs_sat(pcm[c][k]);
            if (!(pk < 32767))
                goto done;
        }
        for (int c = 0; c < 6; c++) {
            free(dp[c]);
            dp[c] = pcm_to_double(pcm[c], lens[c]);
            if (!dp[c])
                goto done;
        }
        sby = pycompat_round_half_even(-4.0 * target_db);
        c16 = enc_core_content16(m, peak, f, target_db);
        pid = enc_core_preset_id(preset ? preset : "manual");
        if (pid < 0)
            goto done;
        nfr0 = enc_core_compose_sub0_51(sby, c16, NULL, pid,
                                        dmx, fr0);
        ni15 = enc_core_compose_sub0_51(
            sby, ENC_CORE_TAIL16_DENSE,
            ENC_CORE_I12_MID_TAIL, pid, dmx, i15);
        np6 = enc_core_compose_sub0(0, 0, sby, 0, NULL, pid,
                                   p6);
        if (nfr0 != 15 || ni15 != 15 || np6 != 6) {
            stamp_fail("enc_core_encode_pcm_mc_m", 0, np6, ni15, nfr0, 15,
                       15);
            goto done;
        }
        memcpy(s4, ENC_CORE_SUB2_DEE, 4);
        stampbuf.p6 = p6;
        stampbuf.n_p6 = 6;
        stampbuf.i12 = i15;
        stampbuf.n_i12 = 15;
        stampbuf.s4 = s4;
        stampbuf.n_s4 = 4;
        stampbuf.fr0 = fr0;
        stampbuf.n_fr0 = 15;
        stamp51 = &stampbuf;
    } else if (ms) {
        flat_file = ms->f_flat > ENC_CORE_FLAT_F_THRESH;
    } else {
        flat_file = 0;
    }
    {
        const double *dpp[6];
        for (int c = 0; c < 6; c++)
            dpp[c] = dp[c];
        n = encode_pcm_mc_frames(dpp, lens, rate_kbps, interval,
                                 trailer, window, shorts, forced,
                                 nforced, drc, dmx, stamp51,
                                 flat_file, out, outcap);
    }
done:
    for (int c = 0; c < 6; c++) {
        free(pcm[c]);
        free(dp[c]);
    }
    return n;
}

int
enc_core_encode_pcm_f_m(const double *pcmL, int nL,
                        const double *pcmR, int nR, int rate_kbps,
                        const char *layout, const char *interval,
                        const char *trailer,
                        const double *window, int ns2_tone,
                        int shorts, double target_db, int has_bake,
                        double bake_db, int joint_shorts,
                        const int *forced, int nforced,
                        const char *preset,
                        const enc_core_drc_t *drc, const int *dmx,
                        int priming, int iframe_base,
                        int iframe_total,
                        const enc_core_measure_t *ms, uint8_t *out,
                        size_t outcap)
{
    core_clear_error();
    int *bL = NULL, *bR = NULL;
    double *dL = NULL, *dR = NULL;
    uint8_t p6[6], i12[12], s4[4], fr0[12];
    enc_core_stamp_t stampbuf;
    const enc_core_stamp_t *stamp = NULL;
    int flat_file = 0;
    int n = -1;
    if (!pcmL || !pcmR || nL < 0 || nR < 0 || !layout ||
        !trailer || !out)
        return -1;
    if (strcmp(layout, "stereo"))
        return -1;
    if (strcmp(trailer, "crc") && strcmp(trailer, "zero"))
        return -1;
    if (!(target_db == 0.0 ||
          (-31.0 <= target_db && target_db <= -10.0)))
        return -1;
    dL = dbl_copy(pcmL, nL);
    dR = dbl_copy(pcmR, nR);
    if (!dL || !dR)
        goto done;
    if (ms && target_db != 0.0) {
        double m = ms->m_db, f = ms->f_flat;
        double tf = ms->tf_tone, gain, bake;
        double in_peak = ms->has_d_peak ? ms->d_peak
                                        : (double)ms->in_peak;
        int peak = 0, sby;
        int sparse, c16, t16, pid, np6, ni12, nfr0;
        if (has_bake)
            gain = bake_db;
        else if (isinf(m) && m < 0)
            gain = 0.0;
        else
            gain = loud_bake_gain(m, f, tf, target_db);
        bake = pow(10.0, gain / 20.0);
        bL = malloc((size_t)(nL > 0 ? nL : 1) * sizeof *bL);
        bR = malloc((size_t)(nR > 0 ? nR : 1) * sizeof *bR);
        if (!bL || !bR)
            goto done;
        for (int k = 0; k < nL; k++)
            if (bake_round(dL[k] * bake, &bL[k]))
                goto done;
        for (int k = 0; k < nR; k++)
            if (bake_round(dR[k] * bake, &bR[k]))
                goto done;
        for (int k = 0; k < nL; k++)
            if (enc_core_abs_sat(bL[k]) > peak)
                peak = enc_core_abs_sat(bL[k]);
        for (int k = 0; k < nR; k++)
            if (enc_core_abs_sat(bR[k]) > peak)
                peak = enc_core_abs_sat(bR[k]);
        if (!(peak < 32767))
            goto done;
        free(dL);
        free(dR);
        dL = pcm_to_double(bL, nL);
        dR = pcm_to_double(bR, nR);
        if (!dL || !dR)
            goto done;
        sby = pycompat_round_half_even(-4.0 * target_db);
        sparse = tf < 0.6 && f > 0.5;
        c16 = enc_core_content16_f(m, in_peak, f, target_db);
        t16 = enc_core_tail16(sparse, tf);
        pid = enc_core_preset_id(preset ? preset : "manual");
        if (pid < 0)
            goto done;
        np6 = enc_core_compose_sub0(0, sparse, sby, 0, NULL,
                                   pid, p6);
        ni12 = enc_core_compose_sub0(
            1, 0, sby, t16, ENC_CORE_I12_MID_TAIL, pid,
            i12);
        nfr0 = enc_core_compose_sub0(1, sparse, sby, c16,
                                     NULL, pid, fr0);
        if (np6 != 6 || ni12 != 12 ||
            nfr0 != (sparse ? 11 : 12)) {
            stamp_fail("enc_core_encode_pcm_f_m", sparse, np6, ni12, nfr0,
                       12, sparse ? 11 : 12);
            goto done;
        }
        memcpy(s4, ENC_CORE_SUB2_DEE, 4);
        stampbuf.p6 = p6;
        stampbuf.n_p6 = 6;
        stampbuf.i12 = i12;
        stampbuf.n_i12 = 12;
        stampbuf.s4 = s4;
        stampbuf.n_s4 = 4;
        stampbuf.fr0 = fr0;
        stampbuf.n_fr0 = nfr0;
        stamp = &stampbuf;
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
    } else if (ms) {
        flat_file = ms->f_flat > ENC_CORE_FLAT_F_THRESH;
    } else {
        flat_file = 0;
    }
    n = encode_pcm_frames(dL, nL, dR, nR, rate_kbps, layout,
                          interval, trailer, window, ns2_tone,
                          shorts, joint_shorts, forced, nforced,
                          drc, dmx, priming, iframe_base,
                          iframe_total, stamp, flat_file, out,
                          outcap);
done:
    free(bL);
    free(bR);
    free(dL);
    free(dR);
    return n;
}

int
enc_core_encode_pcm_mc_f_m(const double *const *chs,
                           const int *lens, int rate_kbps,
                           const char *interval, const char *trailer,
                           const double *window, double target_db,
                           int has_bake, double bake_db, int shorts,
                           const int *forced, int nforced,
                           const char *preset,
                           const enc_core_drc_t *drc, const int *dmx,
                           const enc_core_measure_t *ms,
                           uint8_t *out, size_t outcap)
{
    core_clear_error();
    double *fc[6];
    int *pcm[6];
    double *dp[6];
    uint8_t p6[6], i15[15], s4[4], fr0[15];
    enc_core_stamp_t stampbuf;
    const enc_core_stamp_t *stamp51 = NULL;
    int flat_file = 0;
    int n = -1;
    if (!chs || !lens || !trailer || !out)
        return -1;
    if (strcmp(trailer, "crc") && strcmp(trailer, "zero"))
        return -1;
    if (!(target_db == 0.0 ||
          (-31.0 <= target_db && target_db <= -10.0)))
        return -1;
    for (int c = 0; c < 6; c++) {
        fc[c] = NULL;
        pcm[c] = NULL;
        dp[c] = NULL;
        if (!chs[c] || lens[c] < 0)
            goto done;
    }
    for (int c = 0; c < 6; c++) {
        fc[c] = dbl_copy(chs[c], lens[c]);
        if (!fc[c])
            goto done;
    }
    if (ms && target_db != 0.0) {
        double m = ms->m_db, f = ms->f_flat;
        double tf = ms->tf_tone;
        double gain, bake, lfe_bake, lin[6];
        double peak = ms->has_d_peak ? ms->d_peak
                                     : (double)ms->in_peak;
        int sby, c16, pid;
        int hits51, np6, ni15, nfr0;
        flat_file = f > ENC_CORE_FLAT_F_THRESH;
        hits51 = tf < 0.6 && f > 0.5;
        if (has_bake)
            gain = bake_db;
        else if (isinf(m) && m < 0)
            gain = 0.0;
        else if (hits51)
            gain = ENC_CORE_T_HIT_51_DB -
                (m > -23.0 ? m : -23.0) +
                (target_db + 24.0);
        else
            gain = target_db - m;
        bake = pow(10.0, gain / 20.0);
        for (int c = 0; c < 6; c++)
            lin[c] = pow(
                10.0, ENC_CORE_LEG_COMP_51_DB[c] / 20.0);
        lfe_bake = hits51
            ? pow(10.0, ENC_CORE_LFE_HITS_BAKE_DB / 20.0)
            : bake;
        for (int c = 0; c < 6; c++) {
            double bc = c == 3 ? lfe_bake : bake;
            pcm[c] = malloc((size_t)(lens[c] > 0 ? lens[c]
                                                 : 1) *
                            sizeof *pcm[c]);
            if (!pcm[c])
                goto done;
            for (int k = 0; k < lens[c]; k++) {
                double v = fc[c][k] * bc * lin[c];
                if (bake_round(v, &pcm[c][k]))
                    goto done;
            }
        }
        {
            int pk = 0;
            for (int c = 0; c < 6; c++)
                for (int k = 0; k < lens[c]; k++)
                    if (enc_core_abs_sat(pcm[c][k]) > pk)
                        pk = enc_core_abs_sat(pcm[c][k]);
            if (!(pk < 32767))
                goto done;
        }
        for (int c = 0; c < 6; c++) {
            dp[c] = pcm_to_double(pcm[c], lens[c]);
            if (!dp[c])
                goto done;
        }
        sby = pycompat_round_half_even(-4.0 * target_db);
        c16 = enc_core_content16_f(m, peak, f, target_db);
        pid = enc_core_preset_id(preset ? preset : "manual");
        if (pid < 0)
            goto done;
        nfr0 = enc_core_compose_sub0_51(sby, c16, NULL, pid,
                                        dmx, fr0);
        ni15 = enc_core_compose_sub0_51(
            sby, ENC_CORE_TAIL16_DENSE,
            ENC_CORE_I12_MID_TAIL, pid, dmx, i15);
        np6 = enc_core_compose_sub0(0, 0, sby, 0, NULL, pid,
                                   p6);
        if (nfr0 != 15 || ni15 != 15 || np6 != 6) {
            stamp_fail("enc_core_encode_pcm_mc_f_m", 0, np6, ni15, nfr0, 15,
                       15);
            goto done;
        }
        memcpy(s4, ENC_CORE_SUB2_DEE, 4);
        stampbuf.p6 = p6;
        stampbuf.n_p6 = 6;
        stampbuf.i12 = i15;
        stampbuf.n_i12 = 15;
        stampbuf.s4 = s4;
        stampbuf.n_s4 = 4;
        stampbuf.fr0 = fr0;
        stampbuf.n_fr0 = 15;
        stamp51 = &stampbuf;
    } else if (ms) {
        flat_file = ms->f_flat > ENC_CORE_FLAT_F_THRESH;
        for (int c = 0; c < 6; c++) {
            dp[c] = dbl_copy(fc[c], lens[c]);
            if (!dp[c])
                goto done;
        }
    } else {
        flat_file = 0;
        for (int c = 0; c < 6; c++) {
            dp[c] = dbl_copy(fc[c], lens[c]);
            if (!dp[c])
                goto done;
        }
    }
    {
        const double *dpp[6];
        for (int c = 0; c < 6; c++)
            dpp[c] = dp[c];
        n = encode_pcm_mc_frames(dpp, lens, rate_kbps,
                                 interval, trailer, window,
                                 shorts, forced, nforced, drc,
                                 dmx, stamp51, flat_file, out,
                                 outcap);
    }
done:
    for (int c = 0; c < 6; c++) {
        free(fc[c]);
        free(pcm[c]);
        free(dp[c]);
    }
    return n;
}

/* ---- ac4enc-lib r2: incremental engine ----
 *
 * Single frame-stepping implementation behind the whole-stream
 * drivers (thin wrappers below... above) and the incremental
 * ac4enc_lib ctx. Loop bodies are moved verbatim from
 * encode_pcm_frames / encode_pcm_mc_frames with mechanical
 * adaptations only:
 *   sizes[j] -> running CBR step (positional, exact);
 *   is_iframe  -> rule predicate (exact; joint open-ended edge
 *     documented in enc_step.h);
 *   orient[] whole array -> compute-once ring (exact: masks use
 *     fed-so-far, which provably equals the true total wherever
 *     the mask is read pre-end);
 *   whole-input PCM -> sliding window (all MDCT reads provably
 *     stay inside fed data pre-end; end() freezes the total);
 *   function-static scratch -> heap ctx (B1);
 *   per-frame mallocs -> reusable ctx buffers (H11);
 *   sync wrap -> caller (lib takes bare frames; wrappers wrap).
 * lines[] (H6) arrive via cfg from the caller's pass 1.
 */

struct enc_step_ctx {
    int nch;
    int rate_kbps;
    const char *layout;
    const char *interval;
    int ns2_tone;
    int shorts;
    int joint_shorts;
    int priming;
    int iframe_base;
    int iframe_total;
    const int *forced;
    int nforced;
    const enc_core_drc_t *drc;
    const int *dmx;
    const enc_core_stamp_t *stamp;
    int flat_file;
    int do_bake;
    int do_peak;
    int bake_two;
    double bake0[6];
    double bake1[6];
    int nlines;
    int lines[7];
    double win[4096];
    long long cbr_num;
    long long cbr_acc;
    int if_rule;
    long long if_step;
    long long if_n;
    long long if_j;
    int or_base;
    int or_have;
    int or_ring[8];
    double *rows[8];
    int fed[8];
    int wcap;
    int wbase;
    int fed_max;
    int ended;
    int end_frames;
    int ns_total;
    int emitted;
    int baked_peak;
    int fail_rc;
    int detail;
    union {
        struct {
            double sL[16][128];
            double sR[16][128];
            double s9L[9][1024];
            double s9R[9][1024];
            double sM[16][128];
            double sS[16][128];
            double XL[2048];
            double XR[2048];
        } st;
        struct {
            double sp[6][16][128];
            double x9[5][9][1024];
            double xlfe[2048];
            double xs[6][2048];
        } mc;
    } u;
    uint8_t *b_full;
    uint8_t *b_audio;
    uint8_t *b_body;
};

static int
step_fail(enc_step_ctx_t *c, int detail, int rc)
{
    c->detail = detail;
    if (detail != ENC_STEP_ESHORT && !c->fail_rc)
        c->fail_rc = rc;
    return rc;
}

int
enc_step_max_cbr(int rate_kbps)
{
    long long num, acc;
    int best = 0;
    if (rate_kbps <= 0 || rate_kbps > 1000000)
        return -1;
    num = (long long)rate_kbps * 16;
    acc = ((-num) % 3 + 3) % 3;
    for (int i = 0; i < 6; i++) {
        int q;
        acc += num;
        q = (int)(acc / 3);
        acc = acc % 3;
        if (q > best)
            best = q;
    }
    return best;
}

static int
cbr_peek(const enc_step_ctx_t *c)
{
    return (int)((c->cbr_acc + c->cbr_num) / 3);
}

static void
cbr_advance(enc_step_ctx_t *c)
{
    c->cbr_acc = (c->cbr_acc + c->cbr_num) % 3;
}

static void
if_advance(enc_step_ctx_t *c)
{
    if (c->if_rule == 0) {
        c->if_n++;
        c->if_j = 1 + (c->if_n * 48000) / 2048;
    } else if (c->if_j > LLONG_MAX - c->if_step) {
        c->if_j = LLONG_MAX;
    } else {
        c->if_j += c->if_step;
    }
}

/* Priming/5.1 rule: {0,1} + cadence + forced (positional, exact). */
static int
iframe_pred51(enc_step_ctx_t *c, int j)
{
    if (j == 0 || j == 1)
        return 1;
    while (c->if_j < j)
        if_advance(c);
    if (c->if_j == j)
        return 1;
    if (c->forced) {
        for (int k = 0; k < c->nforced; k++)
            if (c->forced[k] == j)
                return 1;
    }
    return 0;
}

/* Joint rule over global indices (exact for explicit total and
 * for base==0 open-ended; see enc_step.h). */
static int
iframe_pred_joint(enc_step_ctx_t *c, int j)
{
    long long g = (long long)c->iframe_base + j;
    long long total = c->iframe_total < 0 ? LLONG_MAX
                                          : c->iframe_total;
    if (j == 0)
        return 1;
    while (c->if_j < g)
        if_advance(c);
    if (c->if_j == g && g < total)
        return 1;
    if (c->forced) {
        for (int k = 0; k < c->nforced; k++)
            if ((long long)c->forced[k] == g)
                return 1;
    }
    return 0;
}

static int
is01(int v)
{
    return v == 0 || v == 1;
}

int
enc_step_open(enc_step_ctx_t **cp, const enc_step_cfg_t *cfg)
{
    enc_step_ctx_t *c;
    long long step = 0;
    int istep, nrows;
    if (!cp || !cfg)
        return -EINVAL;
    if (cfg->nch != 2 && cfg->nch != 6)
        return -EINVAL;
    if (cfg->rate_kbps <= 0 || cfg->rate_kbps > 1000000)
        return -EINVAL;
    if (cfg->nch == 2 && (!cfg->layout ||
                          strcmp(cfg->layout, "stereo")))
        return -EINVAL;
    if (!is01(cfg->ns2_tone) || !is01(cfg->shorts) ||
        !is01(cfg->joint_shorts) || !is01(cfg->priming) ||
        !is01(cfg->do_bake) || !is01(cfg->do_peak) ||
        !is01(cfg->bake_two))
        return -EINVAL;
    if (cfg->nch == 6 && cfg->joint_shorts)
        return -EINVAL;
    if (cfg->nforced < 0 ||
        (cfg->nforced > 0 && !cfg->forced))
        return -EINVAL;
    if (cfg->nlines < 0 || cfg->nlines > 7)
        return -EINVAL;
    for (int k = 0; k < cfg->nlines; k++)
        if (cfg->lines[k] < 0 ||
            cfg->lines[k] >= MDCT_NCOEFF)
            return -EINVAL;
    istep = parse_interval(cfg->interval, &step);
    if (istep < 0)
        return -EINVAL;
    if (cfg->drc) {
        /* Dry-run the real metadata writer over both iframe
         * values: validates nmodes/mid/kind/eac3/gain/curve. */
        uint8_t tmp[1024];
        if (enc_core_write_metadata(cfg->nch == 2 ? "stereo"
                                                  : "5.1",
                                    0, cfg->drc, tmp,
                                    sizeof tmp) < 0 ||
            enc_core_write_metadata(cfg->nch == 2 ? "stereo"
                                                  : "5.1",
                                    1, cfg->drc, tmp,
                                    sizeof tmp) < 0)
            return -EINVAL;
    }
    c = calloc(1, sizeof *c);
    if (!c)
        return -ENOMEM;
    c->detail = ENC_STEP_OK;
    c->nch = cfg->nch;
    c->rate_kbps = cfg->rate_kbps;
    c->layout = cfg->layout;
    c->interval = cfg->interval;
    c->ns2_tone = cfg->ns2_tone;
    c->shorts = cfg->shorts;
    c->joint_shorts = cfg->joint_shorts;
    c->priming = cfg->priming;
    c->iframe_base = cfg->iframe_base;
    c->iframe_total = cfg->iframe_total;
    c->forced = cfg->forced;
    c->nforced = cfg->nforced;
    c->drc = cfg->drc;
    c->dmx = cfg->dmx;
    c->stamp = cfg->stamp;
    c->flat_file = cfg->flat_file;
    c->do_bake = cfg->do_bake;
    c->do_peak = cfg->do_peak;
    c->bake_two = cfg->bake_two;
    memcpy(c->bake0, cfg->bake0, sizeof c->bake0);
    memcpy(c->bake1, cfg->bake1, sizeof c->bake1);
    c->nlines = cfg->nlines;
    memcpy(c->lines, cfg->lines, sizeof c->lines);
    if (cfg->window)
        memcpy(c->win, cfg->window, sizeof c->win);
    else
        mdct_kbd_window(4096, 3.0, c->win);
    c->cbr_num = (long long)cfg->rate_kbps * 16;
    c->cbr_acc =
        ((-c->cbr_num) % 3 + 3) % 3;
    c->if_rule = istep;
    c->if_step = step;
    if (istep == 0) {
        c->if_n = 1;
        c->if_j = 1 + (1 * 48000) / 2048;
    } else {
        c->if_n = 0;
        c->if_j = 1 + step;
    }
    nrows = cfg->nch +
        (cfg->nch == 2 && cfg->joint_shorts ? 2 : 0);
    c->wcap = 8192;
    for (int r = 0; r < nrows; r++) {
        c->rows[r] =
            malloc((size_t)c->wcap * sizeof *c->rows[r]);
        if (!c->rows[r]) {
            enc_step_close(c);
            return -ENOMEM;
        }
    }
    *cp = c;
    return 0;
}

void
enc_step_close(enc_step_ctx_t *c)
{
    int nrows;
    if (!c)
        return;
    nrows = c->nch +
        (c->nch == 2 && c->joint_shorts ? 2 : 0);
    for (int r = 0; r < nrows; r++)
        free(c->rows[r]);
    free(c->b_full);
    free(c->b_audio);
    free(c->b_body);
    free(c);
}

int
enc_step_emitted(const enc_step_ctx_t *c)
{
    return c ? c->emitted : 0;
}

int
enc_step_detail(const enc_step_ctx_t *c)
{
    return c ? c->detail : ENC_STEP_OK;
}

static int
step_coff(const enc_step_ctx_t *c)
{
    return (c->nch == 2 && !c->priming) ? 2 : 0;
}

/* Start sample of absolute orient index k (k>=2). */
static int
or_start(int k)
{
    return (k - 2) * ENC_CORE_FRAME_SAMPLES;
}

/* Ensure room for n more fed samples; slide to zero first. */
static int
win_grow(enc_step_ctx_t *c, int n)
{
    int nrows =
        c->nch + (c->nch == 2 && c->joint_shorts ? 2 : 0);
    if (n < 0)
        return -1;
    if (c->fed_max + n <= c->wbase + c->wcap)
        return 0;
    if (c->wbase > 0) {
        for (int r = 0; r < nrows; r++) {
            int keep = c->fed[r] - c->wbase;
            if (keep > 0)
                memmove(c->rows[r], c->rows[r] + c->wbase,
                        (size_t)keep * sizeof *c->rows[r]);
            c->fed[r] = keep > 0 ? keep : 0;
        }
        c->fed_max -= c->wbase;
        if (c->fed_max < 0)
            c->fed_max = 0;
        c->wbase = 0;
        if (c->fed_max + n <= c->wcap)
            return 0;
    }
    {
        int want = c->wcap > 0 ? c->wcap : 2048;
        while (want < c->fed_max + n - c->wbase) {
            if (want > INT_MAX / 2)
                return -1;
            want *= 2;
        }
        for (int r = 0; r < nrows; r++) {
            double *nr = realloc(c->rows[r],
                                 (size_t)want *
                                     sizeof *nr);
            if (!nr)
                return -1;
            c->rows[r] = nr;
        }
        c->wcap = want;
    }
    return 0;
}

/* Ingest one already-ordered double sample (post int-cast). */
static int
ingest_d(enc_step_ctx_t *c, int ch, double x)
{
    double v = x;
    if (c->do_bake) {
        int bi;
        if (c->bake_two)
            v = (x * c->bake0[ch]) * c->bake1[ch];
        else
            v = x * c->bake0[ch];
        if (bake_round(v, &bi))
            return -1;
        /* INT_MIN-safe magnitudes (m1), same scans as the core. */
        if (c->do_peak && enc_core_abs_sat(bi) > c->baked_peak)
            c->baked_peak = enc_core_abs_sat(bi);
        v = (double)bi;
    }
    c->rows[ch][c->fed[ch] - c->wbase] = v;
    return 0;
}

static int
feed_common(enc_step_ctx_t *c, const double *const *df,
            const int *const *di, const int *ns)
{
    int add = 0;
    if (!c || !ns)
        return -EINVAL;
    if (c->ended || c->fail_rc)
        return step_fail(c, ENC_STEP_EPROTO, -EINVAL);
    if ((!df && !di) || (df && di))
        return step_fail(c, ENC_STEP_EPROTO, -EINVAL);
    for (int ch = 0; ch < c->nch; ch++) {
        if (ns[ch] < 0)
            return step_fail(c, ENC_STEP_EPROTO, -EINVAL);
        if (ns[ch] > 0 && !(df ? df[ch] : (const void *)di[ch]))
            return step_fail(c, ENC_STEP_EPROTO, -EINVAL);
        if (ns[ch] > add)
            add = ns[ch];
    }
    if (add == 0)
        return 0;
    if (win_grow(c, add))
        return step_fail(c, ENC_STEP_ENOMEM, -ENOMEM);
    for (int ch = 0; ch < c->nch; ch++) {
        for (int k = 0; k < ns[ch]; k++) {
            double x = df ? df[ch][k]
                          : (double)di[ch][k];
            if (ingest_d(c, ch, x))
                return step_fail(c, ENC_STEP_EBAKE, -EIO);
            c->fed[ch]++;
        }
    }
    {
        int mx = 0;
        for (int ch = 0; ch < c->nch; ch++)
            if (c->fed[ch] > mx)
                mx = c->fed[ch];
        /* M/S rows over the newly fed span (core pcmM/pcmS
         * order: zero-fill past each row's end). */
        if (c->nch == 2 && c->joint_shorts) {
            int old = c->fed[2];
            int nl = c->fed[0], nr = c->fed[1];
            for (int g = old; g < mx; g++) {
                double a = g < nl
                    ? c->rows[0][g - c->wbase]
                    : 0.0;
                double b = g < nr
                    ? c->rows[1][g - c->wbase]
                    : 0.0;
                c->rows[2][g - c->wbase] = (a + b) / 2.0;
                c->rows[3][g - c->wbase] = (a - b) / 2.0;
            }
            c->fed[2] = c->fed[3] = mx;
        }
        c->fed_max = mx;
    }
    return 0;
}

int
enc_step_feed(enc_step_ctx_t *c, const double *const *rows,
              const int *ns)
{
    return feed_common(c, rows, NULL, ns);
}

int
enc_step_feed_int(enc_step_ctx_t *c, const int *const *rows,
                  const int *ns)
{
    return feed_common(c, NULL, rows, ns);
}

int
enc_step_end(enc_step_ctx_t *c)
{
    if (!c)
        return -EINVAL;
    if (c->ended || c->fail_rc)
        return step_fail(c, ENC_STEP_EPROTO, -EINVAL);
    c->ended = 1;
    c->ns_total = c->fed_max;
    if (c->nch == 6 || c->priming)
        c->end_frames =
            enc_core_nframes_for_samples(c->ns_total);
    else if (c->ns_total <= 0)
        c->end_frames = 0;
    else
        c->end_frames =
            (c->ns_total + ENC_CORE_FRAME_SAMPLES - 1) /
            ENC_CORE_FRAME_SAMPLES;
    if (c->do_peak && !(c->baked_peak < 32767))
        return step_fail(c, ENC_STEP_EPEAK, -EIO);
    return 0;
}

/* Data needed above a frame start: 4160 with the orient
 * lookahead (shorts), else 3072 (MDCT span). */
static int
emit_ahead(const enc_step_ctx_t *c)
{
    return c->shorts ? 4160 : 3072;
}

int
enc_step_ready(enc_step_ctx_t *c)
{
    int j, i, start;
    if (!c)
        return -EINVAL;
    if (c->fail_rc)
        return c->fail_rc;
    j = c->emitted;
    if (c->ended)
        return j < c->end_frames ? 1 : 0;
    i = j + step_coff(c);
    if (i < 2)
        return c->fed_max > 0 ? 1 : 0;
    start = or_start(i);
    return c->fed_max >= start + emit_ahead(c) ? 1 : 0;
}

int
enc_step_ready_after(const enc_step_ctx_t *c, int n)
{
    int j, i, start;
    if (!c || n < 0)
        return 0;
    if (c->fail_rc)
        return 0;
    j = c->emitted;
    if (c->ended)
        return j < c->end_frames ? 1 : 0;
    i = j + step_coff(c);
    if (i < 2)
        return c->fed_max + n > 0 ? 1 : 0;
    start = or_start(i);
    return c->fed_max + n >= start + emit_ahead(c) ? 1
                                                  : 0;
}

int
enc_step_set_stamp(enc_step_ctx_t *c,
                   const enc_core_stamp_t *stamp)
{
    if (!c)
        return -EINVAL;
    if (c->emitted != 0 || c->fail_rc)
        return step_fail(c, ENC_STEP_EPROTO, -EINVAL);
    c->stamp = stamp;
    return 0;
}

int
enc_step_next_size(enc_step_ctx_t *c)
{
    if (!c)
        return -1;
    if (c->fail_rc || (c->ended && c->emitted >= c->end_frames)) {
        c->detail = ENC_STEP_EPROTO;
        return -1;
    }
    return cbr_peek(c);
}
/* Orient ring: compute-once absolute indices, shared by the
 * pre-scan twin and emission. Values match the whole-stream
 * pre-scan exactly (mask total = fed-so-far pre-end, which
 * provably reads identically wherever consulted). */
static int
or_compute(enc_step_ctx_t *c, int k)
{
    int start = or_start(k);
    int total = c->ended ? c->ns_total : c->fed_max;
    int o0 = 0, o1 = 0, rc;
    if (c->nch == 2) {
        const double *sp[2];
        int wlen0 = c->fed[0] - c->wbase;
        int wlen1 = c->fed[1] - c->wbase;
        if (wlen0 < 0)
            wlen0 = 0;
        if (wlen1 < 0)
            wlen1 = 0;
        tile_specs_128(c->rows[0], wlen0, start - c->wbase,
                       c->u.st.sL);
        tile_specs_128(c->rows[1], wlen1, start - c->wbase,
                       c->u.st.sR);
        sp[0] = &c->u.st.sL[0][0];
        sp[1] = &c->u.st.sR[0][0];
        rc = tile_orient(sp, 2, start, total, &o0, &o1, 0);
    } else {
        const double *sl[6];
        for (int ch = 0; ch < 6; ch++) {
            int wlen = c->fed[ch] - c->wbase;
            if (wlen < 0)
                wlen = 0;
            tile_specs_128(c->rows[ch], wlen,
                           start - c->wbase, c->u.mc.sp[ch]);
            sl[ch] = &c->u.mc.sp[ch][0][0];
        }
        rc = tile_orient(sl, 6, start, total, &o0, &o1, 1);
    }
    if (rc < 0)
        return -1;
    if (rc > 0)
        return o0 == ENC_CORE_TL_1024
            ? ENC_CORE_ORIENT_1024_FIRST
            : ENC_CORE_ORIENT_128_FIRST;
    return ENC_CORE_ORIENT_LONG;
}

static int
or_need(enc_step_ctx_t *c, int i, int *out)
{
    if (!c->shorts || i < 2) {
        *out = ENC_CORE_ORIENT_LONG;
        return 0;
    }
    while (c->or_base + c->or_have <= i) {
        int k = c->or_base + c->or_have;
        int v;
        if (!c->ended &&
            c->fed_max < or_start(k) + 2112)
            return -1;
        v = or_compute(c, k);
        if (v < 0)
            return -1;
        if (c->or_have == 8) {
            c->or_base++;
            c->or_have--;
        }
        c->or_ring[(c->or_base + c->or_have) % 8] = v;
        c->or_have++;
    }
    *out = c->or_ring[i % 8];
    return 0;
}

/* Drop window history before start_next-1024 (no future read
 * reaches below it). */
static void
win_compact(enc_step_ctx_t *c, int start_next)
{
    int nrows =
        c->nch + (c->nch == 2 && c->joint_shorts ? 2 : 0);
    int want = start_next - 1024;
    int shift;
    if (want <= c->wbase)
        return;
    if (want > c->fed_max)
        want = c->fed_max;
    shift = want - c->wbase;
    for (int r = 0; r < nrows; r++) {
        int keep = c->fed[r] - want;
        if (keep > 0)
            memmove(c->rows[r], c->rows[r] + shift,
                    (size_t)keep * sizeof *c->rows[r]);
    }
    c->wbase = want;
}

static int
emit_bufs(enc_step_ctx_t *c)
{
    if (!c->b_full) {
        c->b_full = malloc(65536);
        c->b_audio = malloc(65536);
        c->b_body = malloc(65536);
        if (!c->b_full || !c->b_audio || !c->b_body)
            return -1;
    }
    return 0;
}

/* Stereo frame step: moved encode_pcm_frames loop body. */
static int
st_emit(enc_step_ctx_t *c, int j, uint8_t *out, size_t outcap,
        size_t *out_len, int *is_iframe)
{
    int coff = step_coff(c);
    int i = j + coff;
    int iframe = c->priming ? iframe_pred51(c, j)
                            : iframe_pred_joint(c, j);
    int size = cbr_peek(c);
    int oi_prev = ENC_CORE_ORIENT_LONG;
    int oi = ENC_CORE_ORIENT_LONG;
    int oi_next = ENC_CORE_ORIENT_LONG;
    uint8_t *raw = NULL;
    int n_raw = -1, have_full = 0, have_audio = 0;
    int n_audio = 0;
    int wlen0 = c->fed[0] - c->wbase;
    int wlen1 = c->fed[1] - c->wbase;
    if (wlen0 < 0)
        wlen0 = 0;
    if (wlen1 < 0)
        wlen1 = 0;
    if (emit_bufs(c))
        return step_fail(c, ENC_STEP_ENOMEM, -ENOMEM);
    /* Fresh-zero each frame: the moved bodies replaced per-frame
     * 64K mallocs (fresh zero pages in practice); writers may
     * leave gaps that must read back as zero, not stale. */
    memset(c->b_full, 0, 65536);
    memset(c->b_audio, 0, 65536);
    memset(c->b_body, 0, 65536);
    if (or_need(c, i - 1, &oi_prev) || or_need(c, i, &oi))
        return step_fail(c, ENC_STEP_EEMIT, -EIO);
    if (c->ended && i + 1 >= c->end_frames + coff)
        oi_next = ENC_CORE_ORIENT_LONG;
    else if (or_need(c, i + 1, &oi_next))
        return step_fail(c, ENC_STEP_EEMIT, -EIO);
    if (i >= 2) {
        int start = or_start(i);
        int ts = start - c->wbase;
        if (oi != ENC_CORE_ORIENT_LONG) {
            const double *rL[9], *rR[9];
            int pf, pl, nf, nl;
            frame_edge_lens(oi_prev, &pf, &pl);
            frame_edge_lens(oi_next, &nf, &nl);
            tile_specs_128(c->rows[0], wlen0, ts,
                           c->u.st.sL);
            tile_specs_128(c->rows[1], wlen1, ts,
                           c->u.st.sR);
            if (c->joint_shorts) {
                int wlenM = c->fed[2] - c->wbase;
                int wlenS = c->fed[3] - c->wbase;
                if (wlenM < 0)
                    wlenM = 0;
                if (wlenS < 0)
                    wlenS = 0;
                for (int a = 0; a < 16; a++) {
                    for (int b = 0; b < 128; b++) {
                        c->u.st.sM[a][b] =
                            (c->u.st.sL[a][b] +
                             c->u.st.sR[a][b]) /
                            2.0;
                        c->u.st.sS[a][b] =
                            (c->u.st.sL[a][b] -
                             c->u.st.sR[a][b]) /
                            2.0;
                    }
                }
                if (enc_core_mixed_specs_for_frame(
                        c->rows[2], wlenM, ts, oi,
                        &c->u.st.sM[0][0], pl, nf,
                        c->u.st.s9L) ||
                    enc_core_mixed_specs_for_frame(
                        c->rows[3], wlenS, ts, oi,
                        &c->u.st.sS[0][0], pl, nf,
                        c->u.st.s9R))
                    return step_fail(c, ENC_STEP_EEMIT,
                                     -EIO);
            } else {
                if (enc_core_mixed_specs_for_frame(
                        c->rows[0], wlen0, ts, oi,
                        &c->u.st.sL[0][0], pl, nf,
                        c->u.st.s9L) ||
                    enc_core_mixed_specs_for_frame(
                        c->rows[1], wlen1, ts, oi,
                        &c->u.st.sR[0][0], pl, nf,
                        c->u.st.s9R))
                    return step_fail(c, ENC_STEP_EEMIT,
                                     -EIO);
            }
            for (int g = 0; g < 9; g++) {
                rL[g] = c->u.st.s9L[g];
                rR[g] = c->u.st.s9R[g];
            }
            {
                int n_full;
                if (c->joint_shorts)
                    n_full =
                        enc_core_encode_joint_mixed_frame(
                            j, rL, rR, oi, size,
                            c->layout, iframe, c->stamp,
                            c->drc, c->dmx, c->b_full,
                            65536);
                else
                    n_full = enc_core_encode_mixed_frame(
                        j, rL, rR, oi, size, c->layout,
                        iframe, c->stamp, c->drc, c->dmx,
                        c->b_full, 65536);
                if (n_full >= 0) {
                    raw = c->b_full;
                    n_raw = n_full;
                    have_full = 1;
                }
            }
        } else {
            double wspan[4096];
            const double *ww = c->win;
            int pf, pl, nf, nl, bridged;
            int tl = 0, tr = 0;
            frame_edge_lens(oi_prev, &pf, &pl);
            frame_edge_lens(oi_next, &nf, &nl);
            if (pl < 2048)
                tl = pl;
            if (nf < 2048)
                tr = nf;
            bridged = tl || tr;
            if (bridged) {
                mdct_bridged_window(2048, tl, tr, wspan);
                ww = wspan;
            }
            mdct_forward_full(c->rows[0], wlen0, ts, ww,
                              c->u.st.XL);
            mdct_forward_full(c->rows[1], wlen1, ts, ww,
                              c->u.st.XR);
            if (c->nlines > 0 && !bridged &&
                enc_core_tile_pure(c->u.st.XL,
                                   c->u.st.XR, 2048,
                                   c->lines,
                                   c->nlines) > 0) {
                enc_core_leg_t cL, cR;
                if (enc_core_quantize_frame(
                        c->rows[0], wlen0, ts, c->lines,
                        c->nlines, c->win,
                        ENC_CORE_DIG_GAIN, &cL) ||
                    enc_core_quantize_frame(
                        c->rows[1], wlen1, ts, c->lines,
                        c->nlines, c->win,
                        ENC_CORE_DIG_GAIN, &cR))
                    return step_fail(c, ENC_STEP_EEMIT,
                                     -EIO);
                if (cL.n > 0 || cR.n > 0) {
                    n_audio =
                        enc_core_write_audio_spectral(
                            cL.lines, cL.qs, cL.n, cL.ref,
                            cR.lines, cR.qs, cR.n, cR.ref,
                            c->b_audio, 65536);
                    if (n_audio < 0)
                        return step_fail(c, ENC_STEP_EEMIT,
                                         -EIO);
                    have_audio = 1;
                }
            } else {
                int n_full;
                n_full = enc_core_encode_fullband_frame(
                    j, c->u.st.XL, c->u.st.XR, size,
                    c->layout, iframe, 1, c->stamp, c->drc,
                    c->dmx, !c->flat_file, c->b_full,
                    65536);
                if (n_full >= 0) {
                    raw = c->b_full;
                    n_raw = n_full;
                    have_full = 1;
                }
            }
        }
    }
    if (!have_full) {
        if (!have_audio) {
            int n_body;
            if (c->stamp && j == 0) {
                bytebuf_t s1 = { NULL, 0, 0 };
                const uint8_t *subs[3];
                int nlens[3];
                if (!iframe) {
                    bb_free(&s1);
                    return step_fail(c, ENC_STEP_EEMIT,
                                     -EIO);
                }
                if (priming_sub1_bb(&s1, c->drc, c->dmx)) {
                    bb_free(&s1);
                    return step_fail(c, ENC_STEP_EEMIT,
                                     -EIO);
                }
                subs[0] = c->stamp->fr0;
                nlens[0] = c->stamp->n_fr0;
                subs[1] = s1.p;
                nlens[1] = (int)s1.n;
                subs[2] = c->stamp->s4;
                nlens[2] = c->stamp->n_s4;
                n_body = assemble_raw(j, 1, c->layout, 0,
                                      subs, nlens, 3, size,
                                      c->b_body, 65536);
                bb_free(&s1);
            } else {
                n_body = enc_core_write_raw_frame(
                    j, size, c->layout, iframe, c->drc,
                    c->dmx, c->b_body, 65536);
            }
            if (n_body < 0)
                return step_fail(c, ENC_STEP_EEMIT, -EIO);
            raw = c->b_body;
            n_raw = n_body;
        } else {
            int n_body;
            n_body = enc_core_assemble_content_frame(
                j, c->b_audio, n_audio, size, c->layout,
                iframe, c->ns2_tone, c->stamp, c->drc,
                c->dmx, c->b_body, 65536);
            if (n_body < 0)
                return step_fail(c, ENC_STEP_EEMIT, -EIO);
            raw = c->b_body;
            n_raw = n_body;
        }
    }
    if (n_raw < 0 || n_raw > 65536)
        return step_fail(c, ENC_STEP_EEMIT, -EIO);
    *out_len = (size_t)n_raw;
    if ((size_t)n_raw > outcap || !out)
        return step_fail(c, ENC_STEP_ESHORT, -EINVAL);
    memcpy(out, raw, (size_t)n_raw);
    if (is_iframe)
        *is_iframe = iframe;
    cbr_advance(c);
    c->emitted++;
    win_compact(c, or_start(i + 1));
    return 1;
}
/* 5.1 frame step: moved encode_pcm_mc_frames loop body. */
static int
mc_emit(enc_step_ctx_t *c, int j, uint8_t *out, size_t outcap,
        size_t *out_len, int *is_iframe)
{
    int iframe = iframe_pred51(c, j);
    int size = cbr_peek(c);
    int oi_prev = ENC_CORE_ORIENT_LONG;
    int oi = ENC_CORE_ORIENT_LONG;
    int oi_next = ENC_CORE_ORIENT_LONG;
    uint8_t *raw = NULL;
    int n_raw = -1;
    if (emit_bufs(c))
        return step_fail(c, ENC_STEP_ENOMEM, -ENOMEM);
    /* Fresh-zero each frame: the moved bodies replaced per-frame
     * 64K mallocs (fresh zero pages in practice); writers may
     * leave gaps that must read back as zero, not stale. */
    memset(c->b_full, 0, 65536);
    memset(c->b_audio, 0, 65536);
    memset(c->b_body, 0, 65536);
    if (or_need(c, j - 1, &oi_prev) || or_need(c, j, &oi))
        return step_fail(c, ENC_STEP_EEMIT, -EIO);
    if (c->ended && j + 1 >= c->end_frames)
        oi_next = ENC_CORE_ORIENT_LONG;
    else if (or_need(c, j + 1, &oi_next))
        return step_fail(c, ENC_STEP_EEMIT, -EIO);
    if (j >= 2) {
        int start = or_start(j);
        int ts = start - c->wbase;
        if (oi != ENC_CORE_ORIENT_LONG) {
            const double *rows[45];
            int pf, pl, nf, nl, any = 0;
            int chmap[5] = { 0, 1, 2, 4, 5 };
            int wlen3 = c->fed[3] - c->wbase;
            frame_edge_lens(oi_prev, &pf, &pl);
            frame_edge_lens(oi_next, &nf, &nl);
            if (wlen3 < 0)
                wlen3 = 0;
            for (int ch = 0; ch < 6; ch++) {
                int wlen = c->fed[ch] - c->wbase;
                if (wlen < 0)
                    wlen = 0;
                tile_specs_128(c->rows[ch], wlen, ts,
                               c->u.mc.sp[ch]);
            }
            for (int k = 0; k < 5; k++) {
                int wlen = c->fed[chmap[k]] - c->wbase;
                if (wlen < 0)
                    wlen = 0;
                if (enc_core_mixed_specs_for_frame(
                        c->rows[chmap[k]], wlen, ts, oi,
                        &c->u.mc.sp[chmap[k]][0][0], pl,
                        nf, c->u.mc.x9[k]))
                    return step_fail(c, ENC_STEP_EEMIT,
                                     -EIO);
                for (int g = 0; g < 9; g++)
                    rows[k * 9 + g] = c->u.mc.x9[k][g];
            }
            mdct_forward_full(c->rows[3], wlen3, ts, c->win,
                              c->u.mc.xlfe);
            for (int k = 0; k < 5 && !any; k++)
                for (int g = 0; g < 9 && !any; g++)
                    for (int v = 0; v < 1024; v++)
                        if (fabs(c->u.mc.x9[k][g][v]) >
                            0.0) {
                            any = 1;
                            break;
                        }
            for (int v = 0; v < 2048 && !any; v++)
                if (fabs(c->u.mc.xlfe[v]) > 0.0)
                    any = 1;
            if (any) {
                int n_body;
                n_body = fit_51_mixed(j, rows, c->u.mc.xlfe,
                                      oi, size, iframe,
                                      c->stamp, c->drc,
                                      c->dmx, c->b_body,
                                      65536);
                if (n_body >= 0) {
                    raw = c->b_body;
                    n_raw = n_body;
                }
            }
        } else {
            const double *rows[6];
            int pf, pl, nf, nl, tl = 0, tr = 0;
            int any = 0;
            double wspan[4096];
            const double *ww = c->win;
            frame_edge_lens(oi_prev, &pf, &pl);
            frame_edge_lens(oi_next, &nf, &nl);
            if (pl < 2048)
                tl = pl;
            if (nf < 2048)
                tr = nf;
            if (tl || tr) {
                mdct_bridged_window(2048, tl, tr, wspan);
                ww = wspan;
            }
            for (int ch = 0; ch < 6; ch++) {
                int wlen = c->fed[ch] - c->wbase;
                if (wlen < 0)
                    wlen = 0;
                mdct_forward_full(c->rows[ch], wlen, ts,
                                  ch == 3 ? c->win : ww,
                                  c->u.mc.xs[ch]);
                rows[ch] = c->u.mc.xs[ch];
            }
            for (int ch = 0; ch < 6 && !any; ch++)
                for (int v = 0; v < 2048; v++)
                    if (c->u.mc.xs[ch][v] != 0.0) {
                        any = 1;
                        break;
                    }
            if (any) {
                int n_body;
                n_body = fit_51(j, rows, size, iframe,
                                c->stamp, c->drc, c->dmx,
                                !c->flat_file, c->b_body,
                                65536);
                if (n_body >= 0) {
                    raw = c->b_body;
                    n_raw = n_body;
                }
            }
        }
    }
    if (!raw) {
        int n_body;
        if (c->stamp && j == 0) {
            bytebuf_t s1 = { NULL, 0, 0 };
            const uint8_t *subs[3];
            int nlens[3], n_aud;
            if (!iframe) {
                bb_free(&s1);
                return step_fail(c, ENC_STEP_EEMIT, -EIO);
            }
            n_aud = enc_core_write_audio_data(
                "5.1", 1, c->b_audio, 65536);
            if (n_aud < 0 ||
                sub_audio_bb(&s1, c->b_audio, n_aud,
                             "5.1", 1, c->drc)) {
                bb_free(&s1);
                return step_fail(c, ENC_STEP_EEMIT, -EIO);
            }
            subs[0] = c->stamp->fr0;
            nlens[0] = c->stamp->n_fr0;
            subs[1] = s1.p;
            nlens[1] = (int)s1.n;
            subs[2] = c->stamp->s4;
            nlens[2] = c->stamp->n_s4;
            n_body = assemble_raw(j, 1, "5.1", 0, subs,
                                  nlens, 3, size,
                                  c->b_body, 65536);
            bb_free(&s1);
        } else {
            n_body = enc_core_write_raw_frame(
                j, size, "5.1", iframe, c->drc, c->dmx,
                c->b_body, 65536);
        }
        if (n_body < 0)
            return step_fail(c, ENC_STEP_EEMIT, -EIO);
        raw = c->b_body;
        n_raw = n_body;
    }
    if (n_raw < 0 || n_raw > 65536)
        return step_fail(c, ENC_STEP_EEMIT, -EIO);
    *out_len = (size_t)n_raw;
    if ((size_t)n_raw > outcap || !out)
        return step_fail(c, ENC_STEP_ESHORT, -EINVAL);
    memcpy(out, raw, (size_t)n_raw);
    if (is_iframe)
        *is_iframe = iframe;
    cbr_advance(c);
    c->emitted++;
    win_compact(c, or_start(j + 1));
    return 1;
}

int
enc_step_emit(enc_step_ctx_t *c, uint8_t *out, size_t outcap,
              size_t *out_len, int *is_iframe)
{
    int j;
    if (!c || !out_len)
        return -EINVAL;
    *out_len = 0;
    if (c->fail_rc)
        return c->fail_rc;
    j = c->emitted;
    if (c->ended && j >= c->end_frames)
        return step_fail(c, ENC_STEP_EPROTO, -EINVAL);
    if (!c->ended) {
        int i = j + step_coff(c);
        if (i >= 2 &&
            c->fed_max < or_start(i) + emit_ahead(c))
            return step_fail(c, ENC_STEP_EPROTO, -EINVAL);
        if (i < 2 && c->fed_max <= 0)
            return step_fail(c, ENC_STEP_EPROTO, -EINVAL);
    }
    if (c->nch == 2)
        return st_emit(c, j, out, outcap, out_len,
                       is_iframe);
    return mc_emit(c, j, out, outcap, out_len, is_iframe);
}
