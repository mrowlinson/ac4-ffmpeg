/* asf.h: ASF spectral writer for the clean AC-4 encoder.
 *
 * Reuses the committed C tables (huffman_tables.h codewords,
 * sfb_offsets.h ladders — verified identical to the
 * BOOKS/OFFS inputs). Bit-parity gate:
 * tests/test_asf_parity.py. C11, zero deps.
 */
#ifndef AC4_ASF_H
#define AC4_ASF_H

#include "bits.h"

#define ASF_MAX_Q 8191

/* Band b with offs[b] <= line < offs[b+1]; offs NULL = 2048 ladder. */
int asf_band_of(int line, const int *offs, int nsfb);

int asf_book_for_peak(int p);

void asf_emit_pair(bitwriter_t *w, int book, int a, int b);

void asf_put_section(bitwriter_t *w, int book, int length, int nbits,
                     int esc);

/*
 * One sf_data leg. lines[i]/qs[i] are the nonzero coeffs (n of
 * them); offs NULL selects the 2048 ladder. refs_by_band (size
 * max_sfb) carries per-band refs when has_refs != 0, else the v1
 * shared-ref path runs.
 */
void asf_write_sf_data(bitwriter_t *w, const int *lines, const int *qs,
                       int n, int ref, int max_sfb,
                       const int *refs_by_band, int has_refs,
                       const int *offs, int sect_bits);

typedef struct {
    const int *lines;
    const int *wins; /* NULL = every coeff is window 0 */
    const int *qs;
    int n;
    const int *offs;
    int max_sfb;
    int sect_bits;
    int nwins;
    const int *refs_by_band; /* size max_sfb */
} asf_group_t;

void asf_write_mixed_leg(bitwriter_t *w, const asf_group_t *groups,
                         int ngroups, int ref);

#endif
