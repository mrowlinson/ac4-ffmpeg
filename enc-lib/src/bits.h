/* bits.h: MSB-first bit writer + CRC-16 for the clean AC-4 encoder.
 *
 * C port of encoder/bits.py (C-port phase, module 1). Byte-identical
 * contract: the same op sequence produces the same bit stream as the
 * Python BitWriter; see tests/test_bits_parity.py. C11, zero deps.
 */
#ifndef AC4_BITS_H
#define AC4_BITS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *buf;     /* packed bits, MSB-first within each byte */
    size_t nbits;     /* live bit count */
    size_t cap_bits;  /* allocated capacity in bits */
} bitwriter_t;

void bw_init(bitwriter_t *w);
void bw_free(bitwriter_t *w);

/* Append the low nbits of value, most-significant bit first. */
void bw_put(bitwriter_t *w, uint64_t value, int nbits);

/* Append a "0101..." string. */
void bw_putbits(bitwriter_t *w, const char *bits);

/* Pad with fill bits to the next byte; returns the pad count. */
int bw_align(bitwriter_t *w, int fill);

/* variable_bits() writer, single group + stop bit. */
void bw_varbits(bitwriter_t *w, int nbits, uint64_t value);

/* variable_bits() writer, any value (multi-group + stop). */
void bw_varbits_long(bitwriter_t *w, int nbits, uint64_t value);

size_t bw_nbits(const bitwriter_t *w);

/*
 * Pack whole bytes MSB-first into out (outcap bytes). Returns the
 * byte count, or 0 when the writer is not byte-aligned or outcap
 * is short (mirrors bytes()' assert on alignment).
 */
size_t bw_bytes(const bitwriter_t *w, uint8_t *out, size_t outcap);

/* CRC-16: poly 0x8005, init 0, no reflect, no xorout. */
uint16_t ac4_crc16(const uint8_t *data, size_t len);

#endif
