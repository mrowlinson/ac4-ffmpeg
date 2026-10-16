/* bits.c: MSB-first bit writer + CRC-16. See bits.h. */
#include "bits.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define CRC_POLY 0x8005u


uint16_t
ac4_crc16(const uint8_t *data, size_t len)
{
    uint32_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i] << 8;
        for (int k = 0; k < 8; k++) {
            if (crc & 0x8000u)
                crc = ((crc << 1) ^ CRC_POLY) & 0xFFFFu;
            else
                crc = (crc << 1) & 0xFFFFu;
        }
    }
    return (uint16_t)crc;
}


void
bw_init(bitwriter_t *w)
{
    w->buf = NULL;
    w->nbits = 0;
    w->cap_bits = 0;
}


void
bw_free(bitwriter_t *w)
{
    free(w->buf);
    w->buf = NULL;
    w->nbits = 0;
    w->cap_bits = 0;
}


static void
bw_reserve(bitwriter_t *w, size_t extra)
{
    size_t need = w->nbits + extra;
    if (need <= w->cap_bits)
        return;
    size_t cap = w->cap_bits ? w->cap_bits : 64;
    while (cap < need)
        cap *= 2;
    uint8_t *nb = realloc(w->buf, (cap + 7) / 8);
    assert(nb != NULL);
    memset(nb + (w->cap_bits + 7) / 8, 0,
           (cap + 7) / 8 - (w->cap_bits + 7) / 8);
    w->buf = nb;
    w->cap_bits = cap;
}


static void
bw_push_bit(bitwriter_t *w, int bit)
{
    bw_reserve(w, 1);
    if (bit)
        w->buf[w->nbits / 8] |= (uint8_t)(0x80u >> (w->nbits % 8));
    w->nbits++;
}


void
bw_put(bitwriter_t *w, uint64_t value, int nbits)
{
    assert(nbits >= 0 && nbits <= 64);
    assert(nbits == 0 ? value == 0
                      : nbits == 64 || (value >> nbits) == 0);
    for (int i = nbits - 1; i >= 0; i--)
        bw_push_bit(w, (int)((value >> i) & 1u));
}


void
bw_putbits(bitwriter_t *w, const char *bits)
{
    for (const char *p = bits; *p; p++) {
        assert(*p == '0' || *p == '1');
        bw_push_bit(w, *p - '0');
    }
}


int
bw_align(bitwriter_t *w, int fill)
{
    int pad = (int)((8 - w->nbits % 8) % 8);
    for (int i = 0; i < pad; i++)
        bw_push_bit(w, fill);
    return pad;
}


void
bw_varbits(bitwriter_t *w, int nbits, uint64_t value)
{
    assert(nbits > 0 && nbits < 64 && (value >> nbits) == 0);
    bw_put(w, value, nbits);
    bw_put(w, 0, 1);
}


void
bw_varbits_long(bitwriter_t *w, int nbits, uint64_t value)
{
    uint64_t base = 1u << nbits;
    uint64_t off = 0, span = base;
    int groups = 1;
    while (value >= off + span) {
        off += span;
        span *= base;
        groups++;
    }
    uint64_t rem = value - off;
    for (int j = 0; j < groups; j++) {
        int shift = nbits * (groups - 1 - j);
        bw_put(w, (rem >> shift) & (base - 1), nbits);
        bw_put(w, j < groups - 1 ? 1u : 0u, 1);
    }
}


size_t
bw_nbits(const bitwriter_t *w)
{
    return w->nbits;
}


size_t
bw_bytes(const bitwriter_t *w, uint8_t *out, size_t outcap)
{
    if (w->nbits % 8 != 0)
        return 0;
    if (outcap < w->nbits / 8)
        return 0;
    memcpy(out, w->buf, w->nbits / 8);
    return w->nbits / 8;
}
