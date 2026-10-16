/**
 * @file ac4dsi_priv.h
 * @brief Shared internals for the two DSI emitters (DSI-11).
 *
 * The hand-rolled emitter (ac4dsi.c) and the PutBitContext emitter
 * (ac4dsi_putbits.c) share validation and wire constants through this
 * header so the two can never disagree on what is legal or on buffer
 * bounds. Only the bit Emission differs, and the putbits suite pins
 * the two byte-identical.
 *
 * Private: not installed, not part of the public API.
 */
#ifndef AC4DSI_PRIV_H
#define AC4DSI_PRIV_H

#include "ac4dsi.h"

/* Fixed v1 synthesis constants for the observed stereo/5.1 shape. */
#define AC4DSI_V1_CFG_SINGLE_GROUP 0x1Fu
/* Worst case: 114 fixed bits + 63 tag bytes + align + 8 tail = 632 bits
 * = 79 B, so 80 leaves 1 B margin. (Each tag byte adds a full byte.) */
#define AC4DSI_V1_TMP_SIZE 80u

/**
 * strlen that tolerates NULL (NULL -> 0). Never reads past NUL.
 */
size_t ac4dsi_v1_taglen(const char *s);

/**
 * Validate the stream header + presentation count. AC4DSI_OK or EINVAL.
 */
int ac4dsi_stream_check(const ac4dsi_stream_t *s, size_t n_pres);

/**
 * Validate one v1 presentation's synthesis parameters. AC4DSI_OK/EINVAL.
 */
int ac4dsi_v1_check(const ac4dsi_pres_t *p);

#endif /* AC4DSI_PRIV_H */
