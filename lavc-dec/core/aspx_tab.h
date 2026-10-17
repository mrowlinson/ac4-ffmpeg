/*
 * ASPX (A-SPX) codebook tables, spec-next transcription (wave9).
 * Copyright (c) 2026 Michael Rowlinson
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* ASPX (A-SPX) codebook tables, spec-next transcription (wave9).
 * Authorized: ANSWER-spec-part2-2 (ZIP-TRANSCRIBE, 18 books).
 * Sources (public ETSI deliverables, fetched 2026-10-02):
 *   P1 V1.4.1 ts_10319001v010401p0.zip sha 8c827f84...e3ea4
 *   URLs + full shas: tmp/wave05/WEB-e0.md. Generator:
 *   tools/aspx_gen.py (e0 gate family: prefix-free + Kraft).
 * Layout below is own; numeric values transcribed by the
 * generator; zero ETSI prose or code copied. DO NOT HAND-EDIT:
 * regenerate with tools/aspx_gen.py.
 * Order: fam ENV_LEVEL_15/ENV_LEVEL_30/ENV_BALANCE_15/
 * ENV_BALANCE_30/NOISE_LEVEL/NOISE_BALANCE x F0/DF/DT.
 * Codeword convention: low LEN bits, MSB first on the wire.
 */
#ifndef AVCODEC_ASPX_TAB_H
#define AVCODEC_ASPX_TAB_H

#include <stdint.h>

typedef struct {
    const char *name;
    const uint8_t *len;
    const uint32_t *cw;
    int entries;
    int off;
    int maxlen;
} aspx_book_t;

extern const uint8_t ASPX_HCB_ENV_LEVEL_15_F0_LEN[71];
extern const uint32_t ASPX_HCB_ENV_LEVEL_15_F0_CW[71];
extern const uint8_t ASPX_HCB_ENV_LEVEL_15_DF_LEN[141];
extern const uint32_t ASPX_HCB_ENV_LEVEL_15_DF_CW[141];
extern const uint8_t ASPX_HCB_ENV_LEVEL_15_DT_LEN[141];
extern const uint32_t ASPX_HCB_ENV_LEVEL_15_DT_CW[141];
extern const uint8_t ASPX_HCB_ENV_LEVEL_30_F0_LEN[36];
extern const uint32_t ASPX_HCB_ENV_LEVEL_30_F0_CW[36];
extern const uint8_t ASPX_HCB_ENV_LEVEL_30_DF_LEN[71];
extern const uint32_t ASPX_HCB_ENV_LEVEL_30_DF_CW[71];
extern const uint8_t ASPX_HCB_ENV_LEVEL_30_DT_LEN[71];
extern const uint32_t ASPX_HCB_ENV_LEVEL_30_DT_CW[71];
extern const uint8_t ASPX_HCB_ENV_BALANCE_15_F0_LEN[25];
extern const uint32_t ASPX_HCB_ENV_BALANCE_15_F0_CW[25];
extern const uint8_t ASPX_HCB_ENV_BALANCE_15_DF_LEN[49];
extern const uint32_t ASPX_HCB_ENV_BALANCE_15_DF_CW[49];
extern const uint8_t ASPX_HCB_ENV_BALANCE_15_DT_LEN[49];
extern const uint32_t ASPX_HCB_ENV_BALANCE_15_DT_CW[49];
extern const uint8_t ASPX_HCB_ENV_BALANCE_30_F0_LEN[13];
extern const uint32_t ASPX_HCB_ENV_BALANCE_30_F0_CW[13];
extern const uint8_t ASPX_HCB_ENV_BALANCE_30_DF_LEN[25];
extern const uint32_t ASPX_HCB_ENV_BALANCE_30_DF_CW[25];
extern const uint8_t ASPX_HCB_ENV_BALANCE_30_DT_LEN[25];
extern const uint32_t ASPX_HCB_ENV_BALANCE_30_DT_CW[25];
extern const uint8_t ASPX_HCB_NOISE_LEVEL_F0_LEN[30];
extern const uint32_t ASPX_HCB_NOISE_LEVEL_F0_CW[30];
extern const uint8_t ASPX_HCB_NOISE_LEVEL_DF_LEN[59];
extern const uint32_t ASPX_HCB_NOISE_LEVEL_DF_CW[59];
extern const uint8_t ASPX_HCB_NOISE_LEVEL_DT_LEN[59];
extern const uint32_t ASPX_HCB_NOISE_LEVEL_DT_CW[59];
extern const uint8_t ASPX_HCB_NOISE_BALANCE_F0_LEN[13];
extern const uint32_t ASPX_HCB_NOISE_BALANCE_F0_CW[13];
extern const uint8_t ASPX_HCB_NOISE_BALANCE_DF_LEN[25];
extern const uint32_t ASPX_HCB_NOISE_BALANCE_DF_CW[25];
extern const uint8_t ASPX_HCB_NOISE_BALANCE_DT_LEN[25];
extern const uint32_t ASPX_HCB_NOISE_BALANCE_DT_CW[25];

extern const aspx_book_t aspx_books[18];
#define ASPX_NBOOKS 18
static inline int aspx_book(int fam, int ht) {
    return (fam) * 3 + (ht);
}
/* Returns entry index or -1. getbit yields next wire bit. */
int aspx_decode(const aspx_book_t *b,
                int (*getbit)(void *ctx), void *ctx);

#endif /* AVCODEC_ASPX_TAB_H */
