/*
 * sfb_offsets.h -- 48k scale-factor-band offset ladders.
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

/* sfb_offsets.h -- 48k scale-factor-band offset ladders.
 * Machine-generated value tables; DO NOT HAND-EDIT (regenerate from
 * the ETSI source instead). Values transcribed from ETSI P1 Annex B
 * (Table B.7 band edges, Table B.1 prefix); layout is our own, zero
 * ETSI prose copied. */
#ifndef AVCODEC_SFB_OFFSETS_H
#define AVCODEC_SFB_OFFSETS_H

/* 48k band count (Table B.1) + offsets (Annex B column prefix).
 * off[nbands] == length. Returns 0/NULL on unknown length. */
int sfb_nbands48(int length);
const int *sfb_offsets48(int length);

#endif /* AVCODEC_SFB_OFFSETS_H */
