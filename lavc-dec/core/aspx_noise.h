/*
 * ASPX_NOISE[512][2] noise values table (D41 transcription).
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

/* ASPX_NOISE[512][2] noise values table (D41 transcription).
 * Authorized: owner D3a un-defer 2026-10-13 (reopen of
 * spec/aspx-noise-DEFERRED.md: full-decode noise synthesis).
 * Sources (public ETSI deliverables, fetched 2026-10-02):
 *   P1 V1.4.1 ts_10319001v010401p0.zip sha 8c827f84...e3ea4
 *   URLs + full shas: tmp/wave05/WEB-e0.md. Generator:
 *   tools/aspx_noise_gen.py (e0 gate family; C99 hex-float,
 *   exact float32). Layout below is own; numeric values
 *   transcribed by the generator; zero ETSI prose or code
 *   copied. DO NOT HAND-EDIT: regenerate with the tool.
 */
#ifndef AC4_ASPX_NOISE_H
#define AC4_ASPX_NOISE_H

extern const float ASPX_NOISE[512][2];
#define ASPX_NOISE_NROWS 512

#endif
