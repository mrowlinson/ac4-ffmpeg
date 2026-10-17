/*
 * SNF spectral-noise-fill table, ETSI transcription.
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

/* SNF spectral-noise-fill table, ETSI transcription.
 * Source: ETSI TS 103 190-1 V1.4.1 (ts_10319001v010401p0.zip,
 * sha 8c827f84...e3ea4), Annex C.11 RANDOM_NOISE_TABLE, binary32.
 * Layout below is own; numeric values transcribed by machine;
 * zero ETSI prose or code copied. DO NOT HAND-EDIT.
 */
#ifndef AVCODEC_SNF_TAB_H
#define AVCODEC_SNF_TAB_H

#define SNF_RANDOM_NOISE_N 256
extern const float SNF_RANDOM_NOISE[SNF_RANDOM_NOISE_N];

#endif /* AVCODEC_SNF_TAB_H */
