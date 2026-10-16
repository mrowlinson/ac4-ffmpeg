/**
 * @file ac4dsi_putbits.h
 * @brief PutBitContext-based AC-4 DSI serializer (DSI-11).
 *
 * Same wire format and API contract as ac4dsi_serialize(), but the bit
 * emission is written against libavcodec's PutBitContext instead of the
 * hand-rolled bitwriter. This is the lavc-submit-shaped emitter: the
 * field sequence maps 1:1 onto put_bits()/put_bits32() calls, while the
 * hand-rolled writer stays as the byte-exact oracle (both share
 * validation via ac4dsi_priv.h, and the putbits suite pins them
 * byte-identical on goldens plus fuzzed shapes).
 *
 * Only the .c file needs lavc headers; this header is standalone. The
 * C-encoder wrapper adoption is a separate follow-up patch (combo owns
 * those tips); this file is lib-level only.
 */
#ifndef AC4DSI_PUTBITS_H
#define AC4DSI_PUTBITS_H

#include "ac4dsi.h"

/**
 * Serialize one ac4_dsi_v1 payload with PutBitContext emission.
 *
 * Parameters, return codes, and failure semantics are identical to
 * ac4dsi_serialize() (EINVAL leaves out and *out_len untouched, ENOSPC
 * leaves the zero-padded partial payload and reports the full need).
 * Output is byte-identical to ac4dsi_serialize() for every input,
 * including short-buffer partials.
 */
int ac4dsi_serialize_putbits(const ac4dsi_stream_t *s,
                             const ac4dsi_pres_t *pres, size_t n_pres,
                             uint8_t *out, size_t out_cap, size_t *out_len);

#endif /* AC4DSI_PUTBITS_H */
