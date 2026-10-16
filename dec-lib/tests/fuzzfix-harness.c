/* fuzzfix-harness: FUZZ-DEC fuzzer, vendored for the regression gate.
 * Provenance: copy of ff-fuzz-dec scratch fzharness.c (2026-10-16);
 * corpus-format compatible. Only this header comment differs. */
/* fzharness: deterministic dec-lib fuzzer (ASan+UBSan).
 *
 * Usage:
 *   fzharness <target> <seed> <iters> <corpusdir...> [--repro N] [--dump dir]
 *   target: feed | dnsplice | opt
 *
 * Deterministic: iteration i derives all randomness from hash(seed,i).
 * Reproducer for a crash at iteration i: same cmd with --repro i.
 * With --dump dir, each feed payload is written to dir/last-<pid>.bin
 * BEFORE the call (post-mortem input recovery).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "ac4d.h"

/* ---------- rng (xorshift64*) ---------- */
typedef struct { uint64_t s; } rng_t;
static uint64_t r64(rng_t *r) {
    uint64_t x = r->s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->s = x;
    return x * 0x2545F4914F6CDD1DULL;
}
static uint64_t rbelow(rng_t *r, uint64_t n) {
    return n == 0 ? 0 : r64(r) % n;
}
static uint64_t mix64(uint64_t a, uint64_t b) {
    uint64_t h = a ^ 0x9E3779B97F4A7C15ULL;
    h ^= b + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 32;
    return h ? h : 0x12345;
}

/* ---------- corpus ---------- */
typedef struct {
    uint8_t *d;
    size_t n;
    char name[64]; /* basename stem for stream grouping */
} frame_t;
static frame_t *Gf = NULL;
static size_t Gn = 0;

static void stem_of(const char *path, char *out, size_t cap) {
    const char *b = strrchr(path, '/');
    b = b ? b + 1 : path;
    size_t i = 0;
    while (b[i] && i + 1 < cap) {
        /* cut at -NNNNN.frm suffix */
        if (b[i] == '-' && b[i+1] >= '0' && b[i+1] <= '9' &&
            strstr(b + i, ".frm"))
            break;
        out[i] = b[i];
        i++;
    }
    out[i] = 0;
}

static void load_dir(const char *dir) {
    DIR *dp = opendir(dir);
    if (!dp)
        return;
    struct dirent *e;
    while ((e = readdir(dp)) != NULL) {
        size_t L = strlen(e->d_name);
        if (L < 5 || strcmp(e->d_name + L - 4, ".frm") != 0)
            continue;
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        FILE *f = fopen(p, "rb");
        if (!f)
            continue;
        fseek(f, 0, SEEK_END);
        long s = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (s <= 0 || s > 300000) {
            fclose(f);
            continue;
        }
        uint8_t *b = malloc((size_t)s);
        if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) {
            free(b);
            fclose(f);
            continue;
        }
        fclose(f);
        Gf = realloc(Gf, (Gn + 1) * sizeof *Gf);
        Gf[Gn].d = b;
        Gf[Gn].n = (size_t)s;
        stem_of(e->d_name, Gf[Gn].name, sizeof Gf[Gn].name);
        Gn++;
    }
    closedir(dp);
}

/* ---------- dump-before-call (post-mortem input) ---------- */
static char Gdump[1024];
static unsigned long Gfeed_ctr;
static void dump_feed(const uint8_t *p, size_t n) {
    if (!Gdump[0])
        return;
    Gfeed_ctr++;
    char path[1152];
    snprintf(path, sizeof path, "%s/last-input.bin", Gdump);
    FILE *f = fopen(path, "wb");
    if (f) {
        if (p && n)
            fwrite(p, 1, n, f);
        fclose(f);
    }
    snprintf(path, sizeof path, "%s/feed-%05lu.bin", Gdump,
            Gfeed_ctr);
    f = fopen(path, "wb");
    if (f) {
        if (p && n)
            fwrite(p, 1, n, f);
        fclose(f);
    }
    fprintf(stderr, "FEED#%lu n=%zu b0=%02x%02x%02x%02x\n", Gfeed_ctr, n,
            n > 0 ? p[0] : 0, n > 1 ? p[1] : 0, n > 2 ? p[2] : 0,
            n > 3 ? p[3] : 0);
}

/* ---------- pcm sink ---------- */
static uint8_t Gpcm[12 * 4096 * 4 + 64];
static long Gfeeds, Gdrains, Gflushes;
/* return-code census (FZ_CENSUS=1): hist[rc+8]; notes sampled */
static long Ghist[16];
static char Gnotes[32][64];
static int Gnnotes;
static int Gcensus;

static void note_sample(ac4d_ctx *c) {
    const char *nt = ac4d_gap_note(c);
    if (!nt)
        return;
    for (int i = 0; i < Gnnotes; i++)
        if (strcmp(Gnotes[i], nt) == 0)
            return;
    if (Gnnotes < 32) {
        snprintf(Gnotes[Gnnotes], sizeof Gnotes[0], "%s", nt);
        Gnnotes++;
    }
}

static int feed1(ac4d_ctx *c, const uint8_t *p, size_t n) {
    dump_feed(p, n);
    Gfeeds++;
    int rc = ac4d_feed(c, p, n);
    if (Gcensus) {
        if (rc >= -8 && rc < 8)
            Ghist[rc + 8]++;
        if (rc != 0)
            note_sample(c);
    }
    return rc;
}

/* drain with rotating cap shapes; returns frames consumed */
static int drain(ac4d_ctx *c, rng_t *r) {
    int got = 0;
    for (int k = 0; k < 8; k++) {
        uint64_t w = rbelow(r, 10);
        void *pp = Gpcm;
        size_t cap = sizeof Gpcm;
        ac4d_info fi, *fip = NULL;
        if (w == 0) {
            pp = NULL;
            cap = 0; /* poll */
        } else if (w == 1) {
            fip = &fi; /* fi probe, full cap */
        } else if (w == 2) {
            cap = rbelow(r, 64); /* under-cap */
        } else if (w == 3) {
            cap = sizeof Gpcm - rbelow(r, 7); /* ragged cap */
        }
        int rc = ac4d_get_frame(c, pp, cap, fip);
        Gdrains++;
        if (rc == 1)
            break;
        got++;
    }
    return got;
}

static void flush_storm(ac4d_ctx *c, rng_t *r, int n) {
    for (int k = 0; k < n; k++) {
        uint64_t w = rbelow(r, 6);
        void *pp = Gpcm;
        size_t cap = sizeof Gpcm;
        if (w == 0) {
            pp = NULL;
            cap = 0;
        } else if (w == 1) {
            cap = rbelow(r, 100);
        }
        int rc = ac4d_flush(c, pp, cap);
        Gflushes++;
        if (rc == 1 && rbelow(r, 2))
            break; /* sometimes keep storming past NEED_MORE */
    }
}

/* ---------- mutations ---------- */
static size_t mut_buf(const frame_t *f, rng_t *r, uint8_t *out, size_t outcap,
                      int hdr_bias) {
    size_t n = f->n;
    if (n > outcap)
        n = outcap;
    memcpy(out, f->d, n);
    uint64_t kind = rbelow(r, hdr_bias ? 12 : 10);
    switch (kind) {
    case 0: { /* bitflips 1..32 */
        uint64_t nb = 1 + rbelow(r, 32);
        for (uint64_t i = 0; i < nb; i++) {
            size_t o = (size_t)rbelow(r, n ? n : 1);
            out[o] ^= (uint8_t)(1u << rbelow(r, 8));
        }
        break;
    }
    case 1: { /* randomize a span */
        size_t o = (size_t)rbelow(r, n ? n : 1);
        size_t L = 1 + (size_t)rbelow(r, n - o > 256 ? 256 : (n - o ? n - o : 1));
        for (size_t i = 0; i < L && o + i < n; i++)
            out[o + i] = (uint8_t)rbelow(r, 256);
        break;
    }
    case 2: { /* truncate */
        static const size_t cuts[] = {0, 1, 2, 3, 4, 5, 7, 13, 16, 64, 256};
        size_t c = cuts[rbelow(r, 11)];
        if (c < n)
            n = c ? c : (rbelow(r, 2) ? 0 : n);
        else if (n > 1)
            n = (size_t)rbelow(r, n);
        break;
    }
    case 3: { /* extend with random tail */
        size_t x = 1 + (size_t)rbelow(r, 64);
        if (n + x <= outcap) {
            for (size_t i = 0; i < x; i++)
                out[n + i] = (uint8_t)rbelow(r, 256);
            n += x;
        }
        break;
    }
    case 4: /* sync-word smash */
        if (n >= 2) {
            uint64_t v = rbelow(r, 6);
            uint16_t w = v == 0 ? 0xAC40 : v == 1 ? 0xAC41 : v == 2 ? 0xAC42
                         : v == 3 ? 0x0000 : v == 4 ? 0xFFFF
                                                   : (uint16_t)rbelow(r, 65536);
            out[0] = (uint8_t)(w >> 8);
            out[1] = (uint8_t)w;
        }
        break;
    case 5: /* size-field tweak */
        if (n >= 4) {
            uint64_t v = rbelow(r, 8);
            uint16_t s = (uint16_t)((out[2] << 8) | out[3]);
            if (v == 0)
                s = 0xFFFF;
            else if (v == 1)
                s = 0;
            else if (v == 2)
                s++;
            else if (v == 3)
                s--;
            else if (v == 4)
                s = (uint16_t)(n > 4 ? n - 4 : 0);
            else if (v == 5)
                s = (uint16_t)(n + rbelow(r, 10));
            else
                s = (uint16_t)rbelow(r, 65536);
            out[2] = (uint8_t)(s >> 8);
            out[3] = (uint8_t)s;
        }
        break;
    case 6: { /* swap two chunks */
        if (n >= 32) {
            size_t a = (size_t)rbelow(r, n - 16), b = (size_t)rbelow(r, n - 16);
            size_t L = 1 + (size_t)rbelow(r, 16);
            if (a + L <= n && b + L <= n) {
                for (size_t i = 0; i < L; i++) {
                    uint8_t t = out[a + i];
                    out[a + i] = out[b + i];
                    out[b + i] = t;
                }
            }
        }
        break;
    }
    case 7: /* whole-frame random */
        for (size_t i = 0; i < n; i++)
            out[i] = (uint8_t)rbelow(r, 256);
        break;
    case 8: /* single-byte values sweep */
        if (n >= 1) {
            size_t o = (size_t)rbelow(r, n);
            static const uint8_t vals[] = {0, 1, 0x7F, 0x80, 0xFE, 0xFF};
            out[o] = vals[rbelow(r, 6)];
        }
        break;
    case 9: /* duplicate/shift: memmove overlap */
        if (n >= 16) {
            size_t dst = (size_t)rbelow(r, n - 8);
            size_t src = (size_t)rbelow(r, n - 8);
            size_t L = 1 + (size_t)rbelow(r, 8);
            if (dst + L <= n && src + L <= n)
                memmove(out + dst, out + src, L);
        }
        break;
    case 10: /* TOC-region bitflips (early payload bytes) */
    case 11: {
        size_t base = n > 7 ? 7 : 0;
        size_t end = base + 64 < n ? base + 64 : n;
        if (end > base) {
            uint64_t nb = 1 + rbelow(r, 8);
            for (uint64_t i = 0; i < nb; i++) {
                size_t o = base + (size_t)rbelow(r, end - base);
                out[o] ^= (uint8_t)(1u << rbelow(r, 8));
            }
        }
        break;
    }
    }
    return n;
}

/* ---------- targets ---------- */
static uint8_t Gwork[400000];

static void run_feed(uint64_t seed, uint64_t it) {
    rng_t r;
    r.s = mix64(seed, it * 2 + 1);
    ac4d_opts o;
    memset(&o, 0, sizeof o);
    o.pres_sel = -1;
    static const int bitsv[] = {16, 24, 32, 16, 24, 32};
    o.bits = bitsv[rbelow(&r, 6)];
    if (rbelow(&r, 10) == 0)
        o.sr_override = rbelow(&r, 2) ? 48000 : 44100;
    if (rbelow(&r, 10) == 0)
        o.ch_override = 1 + (int)rbelow(&r, 8);
    ac4d_ctx *c = NULL;
    if (ac4d_open(&c, &o) != 0)
        return;
    int nops = 1 + (int)rbelow(&r, 12);
    int flushed = 0;
    for (int k = 0; k < nops; k++) {
        uint64_t w = rbelow(&r, 100);
        const frame_t *f = &Gf[rbelow(&r, Gn)];
        if (w < 40) {
            feed1(c, f->d, f->n); /* valid */
        } else if (w < 70) {
            size_t n = mut_buf(f, &r, Gwork, sizeof Gwork, 0);
            if (n == 0)
                feed1(c, Gwork, 0);
            else
                feed1(c, Gwork, n);
        } else if (w < 76) {
            /* tiny feeds: 1B / 13B / short header */
            static const size_t tl[] = {1, 2, 3, 13, 5, 7};
            size_t t = tl[rbelow(&r, 6)];
            if (t > f->n)
                t = f->n;
            feed1(c, f->d, t);
        } else if (w < 82) {
            /* concat two frames in one feed */
            const frame_t *g = &Gf[rbelow(&r, Gn)];
            size_t n = f->n + g->n;
            if (n <= sizeof Gwork) {
                memcpy(Gwork, f->d, f->n);
                memcpy(Gwork + f->n, g->d, g->n);
                feed1(c, Gwork, n);
            }
        } else if (w < 88) {
            drain(c, &r);
        } else if (w < 94) {
            flush_storm(c, &r, 1 + (int)rbelow(&r, 6));
            flushed = 1;
        } else {
            /* feed-after-flush + get_info/gap_note probes */
            ac4d_info ii;
            ac4d_get_info(c, &ii);
            (void)ac4d_gap_note(c);
            if (flushed && rbelow(&r, 2))
                feed1(c, f->d, f->n);
            else
                drain(c, &r);
        }
    }
    if (rbelow(&r, 2))
        flush_storm(c, &r, 1 + (int)rbelow(&r, 4));
    ac4d_close(c);
}

static int same_stream(size_t a, size_t b) {
    return strcmp(Gf[a].name, Gf[b].name) == 0;
}

static void run_dnsplice(uint64_t seed, uint64_t it) {
    rng_t r;
    r.s = mix64(seed, it * 2 + 101);
    ac4d_opts o;
    memset(&o, 0, sizeof o);
    o.pres_sel = -1;
    o.bits = 16;
    ac4d_ctx *c = NULL;
    if (ac4d_open(&c, &o) != 0)
        return;
    /* anchor stream: collect indices of one stem */
    size_t anchor = (size_t)rbelow(&r, Gn);
    size_t idx[4096];
    size_t ni = 0;
    for (size_t i = 0; i < Gn && ni < 4096; i++)
        if (same_stream(anchor, i))
            idx[ni++] = i;
    if (ni == 0) {
        ac4d_close(c);
        return;
    }
    /* sort idx by frame order (names share stem; corpus order ~ file order
     * is NOT guaranteed, so order by ordinal suffix) */
    for (size_t i = 1; i < ni; i++) {
        size_t j = i;
        while (j > 0 && idx[j] < idx[j - 1]) {
            size_t t = idx[j];
            idx[j] = idx[j - 1];
            idx[j - 1] = t;
            j--;
        }
    }
    int nops = 2 + (int)rbelow(&r, 10);
    size_t pos = 0;
    for (int k = 0; k < nops; k++) {
        uint64_t w = rbelow(&r, 100);
        if (w < 35) {
            /* in-order run (DN latch + steady state) */
            int run = 1 + (int)rbelow(&r, 4);
            for (int j = 0; j < run; j++) {
                size_t fi = idx[pos % ni];
                feed1(c, Gf[fi].d, Gf[fi].n);
                pos++;
            }
        } else if (w < 55) {
            /* DN-change attempt: TOC-region bitflips on in-order frame */
            size_t fi = idx[pos % ni];
            size_t n = mut_buf(&Gf[fi], &r, Gwork, sizeof Gwork, 1);
            if (n)
                feed1(c, Gwork, n);
            pos++;
        } else if (w < 75) {
            /* splice-epoch: jump/dup/rewind ordinals */
            uint64_t v = rbelow(&r, 4);
            if (v == 0)
                pos = (size_t)rbelow(&r, ni); /* jump */
            else if (v == 1 && pos > 0)
                pos--; /* rewind one */
            /* v==2: dup (no move); v==3: skip ahead */
            else if (v == 3)
                pos += 1 + (size_t)rbelow(&r, ni > 4 ? 4 : 1);
            size_t fi = idx[pos % ni];
            if (rbelow(&r, 3) == 0) {
                size_t n = mut_buf(&Gf[fi], &r, Gwork, sizeof Gwork, 0);
                if (n)
                    feed1(c, Gwork, n);
            } else {
                feed1(c, Gf[fi].d, Gf[fi].n);
            }
            pos++;
        } else if (w < 85) {
            /* gap/no-bed shapes: corrupt payload deep (substream-bounds,
             * bit errors) to drive tolerant sticky-gap legs */
            size_t fi = idx[pos % ni];
            size_t n = Gf[fi].n;
            if (n > sizeof Gwork)
                n = sizeof Gwork;
            memcpy(Gwork, Gf[fi].d, n);
            size_t base = n > 64 ? 64 : 0;
            uint64_t nb = 1 + rbelow(&r, 16);
            for (uint64_t i = 0; i < nb && base < n; i++) {
                size_t o = base + (size_t)rbelow(&r, n - base);
                Gwork[o] ^= (uint8_t)(1u << rbelow(&r, 8));
            }
            feed1(c, Gwork, n);
            pos++;
        } else {
            drain(c, &r);
        }
        if (rbelow(&r, 8) == 0)
            drain(c, &r);
    }
    if (rbelow(&r, 2))
        flush_storm(c, &r, 1 + (int)rbelow(&r, 4));
    ac4d_close(c);
}

static void run_opt(uint64_t seed, uint64_t it) {
    rng_t r;
    r.s = mix64(seed, it * 2 + 1001);
    /* env toggles (no file-writing dump paths) */
    if (rbelow(&r, 3) == 0)
        setenv("AC4DEC_DRC", rbelow(&r, 2) ? "1" : "0", 1);
    else
        unsetenv("AC4DEC_DRC");
    if (rbelow(&r, 3) == 0)
        setenv("AC4DEC_NOCMP", "1", 1);
    else
        unsetenv("AC4DEC_NOCMP");
    if (rbelow(&r, 3) == 0)
        setenv("AC4DEC_KEEP67", "1", 1);
    else
        unsetenv("AC4DEC_KEEP67");
    if (rbelow(&r, 4) == 0)
        setenv("AC4DEC_DRCDBG", "1", 1);
    else
        unsetenv("AC4DEC_DRCDBG");
    if (rbelow(&r, 4) == 0)
        setenv("AC4DEC_ACPLPROBE", "1", 1);
    else
        unsetenv("AC4DEC_ACPLPROBE");
    if (rbelow(&r, 4) == 0)
        setenv("AC4DEC_GAPBITS", "1", 1);
    else
        unsetenv("AC4DEC_GAPBITS");

    ac4d_opts o;
    memset(&o, 0, sizeof o);
    static const int prv[] = {-1, -1, 0, 1, 2, 3, 5, 99};
    o.pres_sel = prv[rbelow(&r, 8)];
    static const int btv[] = {16, 24, 32, 16, 24, 32, 0, 8, 64, -1};
    o.bits = btv[rbelow(&r, 10)];
    static const int srv[] = {0, 0, 44100, 48000, 8000, -1, 192000};
    o.sr_override = srv[rbelow(&r, 7)];
    static const int chv[] = {0, 0, 1, 2, 6, 8, 3, 12, -1};
    o.ch_override = chv[rbelow(&r, 9)];
    o.drc_on = rbelow(&r, 4) == 0 ? 1 : 0;
    static const int orv[] = {0, -16, -31, 5, 100, -100};
    o.drc_out_ref = orv[rbelow(&r, 6)];
    o.drc_ref_given = rbelow(&r, 2);
    static const int pcv[] = {0, 100, 0, 50, 200, -10, 1000};
    o.drc_boost_pct = pcv[rbelow(&r, 7)];
    o.drc_cut_pct = pcv[rbelow(&r, 7)];
    o.de_given = rbelow(&r, 3) == 0 ? 1 : 0;
    static const double dev[] = {0.0, 0.0, 1.0, -6.0, 32.5, -100.0};
    o.de_level = dev[rbelow(&r, 6)];
    o.ajoc_core = rbelow(&r, 4) == 0 ? 1 : 0;

    /* NULL-cp open probe (must fail loud, not crash) */
    if (rbelow(&r, 8) == 0)
        ac4d_open(NULL, &o);

    ac4d_ctx *c = NULL;
    int orc = ac4d_open(&c, &o);
    if (Gcensus && orc >= -8 && orc < 8)
        Ghist[orc + 8]++;
    if (orc != 0) {
        /* UNSUPPORTED / ERR: loud fail. Exercise accessors on NULL. */
        (void)ac4d_gap_note(NULL);
        ac4d_info ii;
        ac4d_get_info(NULL, &ii);
        ac4d_close(NULL);
        if (c)
            ac4d_close(c);
        return;
    }
    int nops = 1 + (int)rbelow(&r, 8);
    for (int k = 0; k < nops; k++) {
        uint64_t w = rbelow(&r, 100);
        const frame_t *f = &Gf[rbelow(&r, Gn)];
        if (w < 55) {
            feed1(c, f->d, f->n);
        } else if (w < 75) {
            size_t n = mut_buf(f, &r, Gwork, sizeof Gwork, 0);
            if (n)
                feed1(c, Gwork, n);
        } else if (w < 85) {
            drain(c, &r);
        } else {
            flush_storm(c, &r, 1 + (int)rbelow(&r, 3));
        }
    }
    ac4d_close(c);
    unsetenv("AC4DEC_DRC");
    unsetenv("AC4DEC_NOCMP");
    unsetenv("AC4DEC_KEEP67");
    unsetenv("AC4DEC_DRCDBG");
    unsetenv("AC4DEC_ACPLPROBE");
    unsetenv("AC4DEC_GAPBITS");
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: fzharness feed|dnsplice|opt seed iters "
                        "corpdir [corpdir...] [--repro N] [--dump dir]\n");
        return 2;
    }
    void (*run)(uint64_t, uint64_t) = NULL;
    if (strcmp(argv[1], "feed") == 0)
        run = run_feed;
    else if (strcmp(argv[1], "dnsplice") == 0)
        run = run_dnsplice;
    else if (strcmp(argv[1], "opt") == 0)
        run = run_opt;
    else
        return 2;
    uint64_t seed = strtoull(argv[2], NULL, 0);
    uint64_t iters = strtoull(argv[3], NULL, 0);
    long repro = -1;
    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "--repro") == 0 && i + 1 < argc)
            repro = atol(argv[++i]);
        else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc)
            snprintf(Gdump, sizeof Gdump, "%s", argv[++i]);
        else if (argv[i][0] != '-')
            load_dir(argv[i]);
    }
    if (Gn == 0) {
        fprintf(stderr, "no corpus frames\n");
        return 2;
    }
    fprintf(stderr, "corpus: %zu frames\n", Gn);
    if (repro >= 0) {
        run(seed, (uint64_t)repro);
        printf("repro %s seed=%llu iter=%ld feeds=%ld drains=%ld "
               "flushes=%ld DONE\n",
               argv[1], (unsigned long long)seed, repro, Gfeeds, Gdrains,
               Gflushes);
        return 0;
    }
    const char *trace = getenv("FZ_TRACE");
    Gcensus = getenv("FZ_CENSUS") != NULL;
    uint64_t from = 0;
    const char *fe = getenv("FZ_FROM");
    if (fe)
        from = strtoull(fe, NULL, 0);
    for (uint64_t i = from; i < iters; i++) {
        if (trace)
            fprintf(stderr, "TRACE iter %llu\n", (unsigned long long)i);
        run(seed, i);
        if ((i + 1) % 500 == 0)
            fprintf(stderr, "[%s] iter %llu/%llu feeds=%ld\n", argv[1],
                    (unsigned long long)(i + 1), (unsigned long long)iters,
                    Gfeeds);
    }
    printf("TARGET %s seed=%llu iters=%llu feeds=%ld drains=%ld "
           "flushes=%ld DONE\n",
           argv[1], (unsigned long long)seed, (unsigned long long)iters,
           Gfeeds, Gdrains, Gflushes);
    if (Gcensus) {
        printf("CENSUS");
        for (int i = 0; i < 16; i++)
            if (Ghist[i])
                printf(" rc%d=%ld", i - 8, Ghist[i]);
        printf("\nCENSUS-NOTES");
        for (int i = 0; i < Gnnotes; i++)
            printf(" [%s]", Gnotes[i]);
        printf("\n");
    }
    return 0;
}