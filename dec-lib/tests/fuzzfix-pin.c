/* fuzzfix-pin: single-packet behavior pin for FUZZ-DEC-1.
 * Feeds one packet file through default-opts ac4d open/feed/close.
 * Exit 0 iff feed returns the pinned loud-reject code (-3, bad packet);
 * any sanitizer abort fails loud via the signal. Usage: pin <packet.bin>
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ac4d.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: pin <packet.bin>\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) {
        fprintf(stderr, "pin: empty input\n");
        fclose(f);
        return 2;
    }
    uint8_t *d = malloc((size_t)n);
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(d);
        return 2;
    }
    fclose(f);
    ac4d_ctx *c = NULL;
    ac4d_opts o;
    /* default opts must match fuzzer iters (pres_sel=-1, bits=16) */
    for (size_t i = 0; i < sizeof o; i++)
        ((unsigned char *)&o)[i] = 0;
    o.pres_sel = -1;
    o.bits = 16;
    if (ac4d_open(&c, &o) != 0) {
        printf("open failed\n");
        free(d);
        return 1;
    }
    int rc = ac4d_feed(c, d, (size_t)n);
    printf("feed rc=%d (%s) note=%s\n", rc, ac4d_strerror(rc),
           ac4d_gap_note(c));
    ac4d_close(c);
    free(d);
    /* pinned: cfg6-only TOC rejects loud with AC4D_BAD_PACKET (-3) */
    return rc == -3 ? 0 : 1;
}
