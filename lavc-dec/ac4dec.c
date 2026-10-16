/*
 * AC-4 audio decoder (thin wrapper over the dec-lib streaming API)
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

/**
 * @file
 * Native AC-4 decoder. Thin lavc wrapper: re-frames demuxer payloads to
 * AC40 sync frames, feeds the dec-lib streaming core (ac4d_*), drains
 * every feed to NEED_MORE, and emits one AVFrame per ready lib frame.
 * No DSP lives here.
 *
 * Lib contract honored (REVIEW-DEC-LIB APPROVE-WRAP + WRAP-DRIFTCHECK):
 * - drain-per-feed: get_frame is called until AC4D_NEED_MORE after every
 *   accepted feed; ready frames wait in a wrapper FIFO, never in the lib.
 * - flush loops ac4d_flush until NEED_MORE (never flush-once); the lib
 *   emits whole tail frames only, total emitted == total fed.
 * - seek is close + reopen: feed-after-flush is AC4D_ERR, there is no
 *   reset API, so .flush drops all state and re-opens the lib.
 * - all lib calls are serialized process-wide (the header overclaims
 *   thread safety: _ac4d_self + shared statics make concurrent use of
 *   even distinct contexts unsafe; REVIEW MAJOR-3).
 * - error table: BAD_PACKET skips loud with a consecutive cap (a bad
 *   pres_sel fails every feed identically, and with no parser there is
 *   no resync); BITSTREAM/CONFIG_CHANGE/ERR are fatal and sticky;
 *   UNSUPPORTED at init maps DRC/DE fail-open. Truncation mirrors the
 *   shim's sync-chain rule: a lone clean framing (one unflagged
 *   packet) followed by skipped packets is unanchored sync, so the
 *   lone prefix is discarded and EOF fails loud (INVALIDDATA,
 *   no-sync diagnostic, zero output); two chained framings (or a
 *   lone framing at clean EOF) decode normally. First-frame emit
 *   waits for the shape decision (second clean framing or EOF).
 * - stream params latch from the first emitted frame's info only;
 *   channels == 0 (no-bed/exotic class) fails loud.
 *
 * Sample formats: S16 only in v1. The lib's bits=32 output is
 * requantized int (s16/32768, saturated, no headroom), and bits=24 is
 * s16LE + 0x00 pad (3-byte LE, pinned but carrying no true 24-bit
 * path), so neither is advertised until native plumbing lands.
 * Requesting anything but S16 fails init honestly.
 *
 * Delay: avctx->delay stays 0 (Q2 open). The lib exposes no priming
 * count at this revision, so no trim is applied; output is the lib's
 * raw frame stream. When dec-lib carries priming, set delay + seed
 * skip_samples at the latch site.
 *
 * Rate labels come straight from the lib (N5: base_fs x sf_mult +
 * s5.11 converter output counts on every fri row). Per-frame spf
 * varies on fractional rows (Table-70 pattern, bounded by the
 * latched max); the wrapper passes labels and counts through;
 * see lavc-dec/README.md.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/error.h"
#include "libavutil/frame.h"
#include "libavutil/log.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/thread.h"

#include "libavcodec/avcodec.h"
#include "libavcodec/codec_internal.h"
#include "libavcodec/decode.h"
#include "libavcodec/internal.h"
#include "libavcodec/packet.h"

#include "ac4d.h"

/* Consecutive BAD_PACKET feeds before the decode aborts. Real corpus
 * shows zero in-lib rejects; a stuck pres_sel (or unresyncable
 * garbage with no parser) fails every feed identically, so a tight
 * cap fails fast instead of spinning through the file. */
#define AC4_MAX_CONSEC_BAD 16

/* Largest plausible S16 lib frame (8ch x 2048 x 2B). Sizing is
 * re-checked against latched info after the first feed. */
#define AC4_MAX_FRAME_BYTES (8 * 2048 * 2)

typedef struct AC4QueuedFrame {
    uint8_t *pcm;          /* owned interleaved S16, len bytes */
    size_t len;
    ac4d_info info;        /* per-frame info from get_frame/flush */
    int is_gap;
    int64_t pts;           /* source packet pts (pop per emitted frame) */
    char note[64];         /* gap_note copy ("" for real frames) */
} AC4QueuedFrame;

typedef struct AC4DecodeContext {
    const AVClass *class;
    ac4d_ctx *lib;

    int pres_sel;
    int drc_on;
    int drc_out_ref;
    int drc_boost_pct;
    int drc_cut_pct;
    double de_level;

    AVPacket *pkt;         /* borrows avctx->internal->in_pkt */
    uint8_t *frame_buf;    /* re-frame scratch (AC40 header + payload) */
    unsigned frame_cap;
    uint8_t *pcm_buf;      /* get_frame/flush drain scratch */
    unsigned pcm_cap;

    AC4QueuedFrame *q;     /* ready frames, qh..qn-1 valid */
    int qn, qh, qa;
    int64_t *pend_pts;     /* pushed per accepted feed, popped per drain */
    int pn, ph, pa;
    int64_t last_pts;

    int stream_set;        /* avctx sr/layout latched */
    int latched_spf;       /* latched MAX spf (get_info sizing bound) */
    int latched_ch;
    int draining;          /* input EOF seen, flush loop running */
    int flush_done;
    int fatal;             /* latched AVERROR, sticks (0 = none) */
    int bad_streak;
    int clean_framings;  /* size-valid unflagged packets (demuxer sync
                          * framings; the shim's chain-count mirror) */
    int trail_skips;     /* skipped packets since the last clean framing
                          * (any skip path; unanchored-tail witness) */
    int emit_ok;         /* shape decided-good: held prefix may emit */
    long pkt_idx;
    long fed_count;
    long gap_frames;
    char last_gap[64];     /* once-per-change gap logging */
    int pts_warned;
} AC4DecodeContext;

/* Process-wide lib serialization (REVIEW MAJOR-3: _ac4d_self and the
 * imdct/qmf/acpl/entropy statics are shared mutable; concurrent use
 * of even distinct contexts is unsafe until the lib is fixed). Every
 * ac4d_* call below runs under this lock. */
static AVMutex ac4d_mutex = AV_MUTEX_INITIALIZER;

static void ac4_lib_log(int lvl, const char *msg, void *u)
{
    AVCodecContext *avctx = u;
    /* The lib routes all diagnostics at a single level; they are
     * supplementary (gap notes and strerror are logged at their own
     * sites), so DEBUG keeps default output clean. */
    (void) lvl;
    av_log(avctx, AV_LOG_DEBUG, "lib: %s", msg);
}

static int ac4_lib_open(AC4DecodeContext *s, AVCodecContext *avctx)
{
    ac4d_opts opts;
    int rc;

    memset(&opts, 0, sizeof(opts));
    opts.pres_sel      = s->pres_sel;
    opts.drc_on        = s->drc_on;
    opts.drc_out_ref   = s->drc_out_ref;
    opts.drc_ref_given = 1;
    opts.drc_boost_pct = s->drc_boost_pct;
    opts.drc_cut_pct   = s->drc_cut_pct;
    opts.de_given      = 1;
    opts.de_level      = s->de_level;
    opts.bits          = 16;
    opts.log           = ac4_lib_log;
    opts.log_u         = avctx;

    ff_mutex_lock(&ac4d_mutex);
    rc = ac4d_open(&s->lib, &opts);
    ff_mutex_unlock(&ac4d_mutex);

    if (rc == AC4D_OK)
        return 0;
    s->lib = NULL;
    if (rc == AC4D_NO_MEM)
        return AVERROR(ENOMEM);
    if (rc == AC4D_UNSUPPORTED) {
        av_log(avctx, AV_LOG_ERROR,
               "unsupported option: DRC and dialog enhancement need "
               "whole-stream legs the streaming core lacks; keep drc off "
               "and de_level at 0\n");
        return AVERROR(EINVAL);
    }
    av_log(avctx, AV_LOG_ERROR, "ac4d_open failed: %s\n",
           ac4d_strerror(rc));
    return AVERROR(EINVAL);
}

/* Map a sticky lib failure to an AVERROR. The caller latches it in
 * s->fatal so the error sticks across subsequent receive_frame calls. */
static int ac4_lib_fatal(AC4DecodeContext *s, AVCodecContext *avctx,
                         int rc, const char *where)
{
    const char *note;

    ff_mutex_lock(&ac4d_mutex);
    note = ac4d_gap_note(s->lib);
    ff_mutex_unlock(&ac4d_mutex);

    switch (rc) {
    case AC4D_NO_MEM:
        av_log(avctx, AV_LOG_ERROR, "%s: out of memory\n", where);
        return AVERROR(ENOMEM);
    case AC4D_BITSTREAM:
        /* Strict-path bit error: fatal, never skip (driftcheck 17). */
        av_log(avctx, AV_LOG_ERROR, "%s: bitstream error (%s)\n",
               where, note);
        return AVERROR_INVALIDDATA;
    case AC4D_CONFIG_CHANGE:
        /* v1 fatal (D6): the core is one-shot-init, so a mid-stream
         * bed/rate/pres change cannot be absorbed. Drain-and-reopen
         * is the upgrade path if real streams need it (Q3). */
        av_log(avctx, AV_LOG_ERROR,
               "%s: mid-stream config change (%s); v1 treats this as "
               "fatal\n", where, note);
        return AVERROR_INVALIDDATA;
    case AC4D_ERR:
        av_log(avctx, AV_LOG_ERROR, "%s: decode failure (%s)\n",
               where, note);
        return AVERROR(EINVAL);
    default:
        av_log(avctx, AV_LOG_ERROR, "%s: %s (%s)\n", where,
               ac4d_strerror(rc), note);
        return AVERROR(EINVAL);
    }
}

static void ac4_queue_reset(AC4DecodeContext *s)
{
    int i;

    for (i = s->qh; i < s->qn; i++)
        av_freep(&s->q[i].pcm);
    s->qn = s->qh = 0;
    s->pn = s->ph = 0;
}

static int ac4_pts_push(AC4DecodeContext *s, int64_t pts)
{
    if (s->pn >= s->pa) {
        int64_t *np;
        int na = s->pa ? s->pa * 2 : 8;

        np = av_realloc_array(s->pend_pts, na, sizeof(*np));
        if (!np)
            return AVERROR(ENOMEM);
        s->pend_pts = np;
        s->pa = na;
    }
    s->pend_pts[s->pn++] = pts;
    return 0;
}

/* Pop the pts of the next emitted frame. Totals match (emitted ==
 * fed), so underflow means a lib invariant broke; fall back to
 * last+spf rather than mislabeling. */
static int64_t ac4_pts_pop(AC4DecodeContext *s, AVCodecContext *avctx,
                           int spf)
{
    int64_t pts;

    if (s->ph < s->pn) {
        pts = s->pend_pts[s->ph++];
    } else {
        if (!s->pts_warned) {
            av_log(avctx, AV_LOG_WARNING,
                   "lib emitted more frames than fed; pts estimated\n");
            s->pts_warned = 1;
        }
        pts = s->last_pts == AV_NOPTS_VALUE ? AV_NOPTS_VALUE
                                            : s->last_pts + spf;
    }
    if (s->ph == s->pn)
        s->ph = s->pn = 0;
    s->last_pts = pts;
    return pts;
}

static int ac4_queue_push(AC4DecodeContext *s, AVCodecContext *avctx,
                          const uint8_t *pcm, size_t len,
                          const ac4d_info *fi, int is_gap)
{
    AC4QueuedFrame *f;
    const char *note = "";

    if (s->qn >= s->qa) {
        AC4QueuedFrame *nq;
        int na = s->qa ? s->qa * 2 : 8;

        nq = av_realloc_array(s->q, na, sizeof(*nq));
        if (!nq)
            return AVERROR(ENOMEM);
        s->q = nq;
        s->qa = na;
    }
    if (is_gap) {
        ff_mutex_lock(&ac4d_mutex);
        note = ac4d_gap_note(s->lib);
        if (!note)
            note = "gap";
        ff_mutex_unlock(&ac4d_mutex);
    }
    f = &s->q[s->qn];
    f->pcm = av_malloc(len ? len : 1);
    if (!f->pcm)
        return AVERROR(ENOMEM);
    memcpy(f->pcm, pcm, len);
    f->len    = len;
    f->info   = *fi;
    f->is_gap = is_gap;
    f->pts    = ac4_pts_pop(s, avctx, fi->spf);
    snprintf(f->note, sizeof(f->note), "%s", note);
    s->qn++;
    return 0;
}

/* Size the drain scratch from latched lib info (needed before the
 * first get_frame, whose cap must fit channels*spf*s16). */
static int ac4_size_pcm(AC4DecodeContext *s, AVCodecContext *avctx,
                        size_t *framelen)
{
    ac4d_info li;
    int rc;
    size_t need;

    ff_mutex_lock(&ac4d_mutex);
    rc = ac4d_get_info(s->lib, &li);
    ff_mutex_unlock(&ac4d_mutex);
    if (rc == AC4D_NEED_MORE)
        return 0; /* unlatched (virgin drain); caller decides */
    if (rc != AC4D_OK)
        return ac4_lib_fatal(s, avctx, rc, "get_info");
    if (li.channels <= 0 || li.spf <= 0)
        return 0; /* unlatched or no-bed; caller decides */
    need = (size_t)li.channels * (size_t)li.spf * 2;
    if (need > s->pcm_cap) {
        av_fast_malloc(&s->pcm_buf, &s->pcm_cap, need);
        if (!s->pcm_buf)
            return AVERROR(ENOMEM);
    }
    *framelen = need;
    return 0;
}

/* Drain-per-feed: get_frame until NEED_MORE, queueing every ready
 * frame. Returns 0 with progress queued (or nothing ready), or a
 * latched fatal AVERROR. */
static int ac4_drain(AC4DecodeContext *s, AVCodecContext *avctx,
                     size_t framelen)
{
    for (;;) {
        ac4d_info fi;
        int grc, ret;

        memset(&fi, 0, sizeof(fi));
        ff_mutex_lock(&ac4d_mutex);
        grc = ac4d_get_frame(s->lib, s->pcm_buf, framelen, &fi);
        ff_mutex_unlock(&ac4d_mutex);
        if (grc == AC4D_NEED_MORE)
            return 0;
        if (grc != AC4D_OK && grc != AC4D_GAP)
            return ac4_lib_fatal(s, avctx, grc, "get_frame");
        /* Copy exactly what the lib wrote (equals framelen except
         * for no-bed GAPs, which carry zero channels); never copy
         * past the drain cap. */
        {
            size_t n = (size_t)FFMAX(fi.channels, 0) *
                       (size_t)FFMAX(fi.spf, 0) * 2;
            ret = ac4_queue_push(s, avctx, s->pcm_buf,
                                 FFMIN(n, framelen), &fi,
                                 grc == AC4D_GAP);
        }
        if (ret < 0)
            return ret;
    }
}

/* EOF drain: ac4d_flush in a loop until NEED_MORE (never flush-once).
 * Same queueing and fatal mapping as the feed drain. */
static int ac4_drain_flush(AC4DecodeContext *s, AVCodecContext *avctx)
{
    size_t framelen = 0;
    int ret;

    ret = ac4_size_pcm(s, avctx, &framelen);
    if (ret < 0)
        return ret;
    if (!framelen)
        framelen = 1024;
    for (;;) {
        /* Flush reports the drained frame's actual count in fi (N5
         * 4-arg flush); shape verified against the latch at emit. */
        ac4d_info fi;
        int grc;

        memset(&fi, 0, sizeof(fi));
        ff_mutex_lock(&ac4d_mutex);
        grc = ac4d_flush(s->lib, s->pcm_buf, framelen, &fi);
        ff_mutex_unlock(&ac4d_mutex);
        if (grc == AC4D_NEED_MORE)
            return 0;
        if (grc != AC4D_OK && grc != AC4D_GAP)
            return ac4_lib_fatal(s, avctx, grc, "flush");
        {
            size_t n = (size_t)FFMAX(fi.channels, 0) *
                       (size_t)FFMAX(fi.spf, 0) * 2;
            ret = ac4_queue_push(s, avctx, s->pcm_buf,
                                 FFMIN(n, framelen), &fi,
                                 grc == AC4D_GAP);
        }
        if (ret < 0)
            return ret;
    }
}

/* Latch avctx stream params from the first emitted frame's info.
 * Gap-shaped params are usable (presented shape); a zero-channel
 * latch (no-bed/exotic class) fails loud. */
static int ac4_latch(AC4DecodeContext *s, AVCodecContext *avctx,
                     const ac4d_info *fi, int is_gap)
{
    AVChannelLayout layout = { 0 };
    int ret;

    if (fi->channels <= 0 || fi->ch_layout_mask == 0) {
        av_log(avctx, AV_LOG_ERROR,
               "undecodable stream class: lib latched channels=%d "
               "mask=0x%llx (%s); no PCM bed to emit\n",
               fi->channels, (unsigned long long)fi->ch_layout_mask,
               is_gap ? s->q[s->qh].note : "real");
        return AVERROR(EINVAL);
    }
    ret = av_channel_layout_from_mask(&layout,
                                      (uint64_t)fi->ch_layout_mask);
    if (ret < 0 || layout.nb_channels != fi->channels) {
        av_log(avctx, AV_LOG_ERROR,
               "lib layout mask 0x%llx does not match channels=%d\n",
               (unsigned long long)fi->ch_layout_mask, fi->channels);
        av_channel_layout_uninit(&layout);
        return AVERROR(EINVAL);
    }
    if (fi->sample_rate <= 0 || fi->spf <= 0) {
        av_log(avctx, AV_LOG_ERROR,
               "lib latched bogus rate/spf %d/%d\n",
               fi->sample_rate, fi->spf);
        av_channel_layout_uninit(&layout);
        return AVERROR(EINVAL);
    }
    avctx->sample_rate = fi->sample_rate;
    av_channel_layout_uninit(&avctx->ch_layout);
    ret = av_channel_layout_copy(&avctx->ch_layout, &layout);
    av_channel_layout_uninit(&layout);
    if (ret < 0)
        return ret;
    s->stream_set = 1;
    /* N5: per-frame spf varies on fractional rows, so the sizing
     * invariant is the latched max from get_info, not this frame. */
    {
        ac4d_info li;
        int lrc;

        ff_mutex_lock(&ac4d_mutex);
        lrc = ac4d_get_info(s->lib, &li);
        ff_mutex_unlock(&ac4d_mutex);
        s->latched_spf = (lrc == AC4D_OK && li.spf > 0) ? li.spf : fi->spf;
    }
    s->latched_ch = fi->channels;
    av_log(avctx, AV_LOG_INFO,
           "stream: %d Hz, %d channels, %d samples/frame (bed_cm %d)%s\n",
           fi->sample_rate, fi->channels, fi->spf, fi->bed_cm,
           is_gap ? " [params from stub-gap class]" : "");
    return 0;
}

/* Drop the queued head: it can never emit (a latch/shape failure is
 * deterministic per frame), and keeping it would fail identically on
 * every call without ever pulling more input or EOF. */
static void ac4_drop_head(AC4DecodeContext *s)
{
    av_freep(&s->q[s->qh].pcm);
    s->qh++;
    if (s->qh == s->qn)
        s->qh = s->qn = 0;
}

static int ac4_emit(AC4DecodeContext *s, AVCodecContext *avctx,
                    AVFrame *frame)
{
    AC4QueuedFrame *f = &s->q[s->qh];
    int ret;

    if (!s->stream_set) {
        ret = ac4_latch(s, avctx, &f->info, f->is_gap);
        if (ret < 0) {
            ac4_drop_head(s);
            return ret;
        }
    } else if (f->info.channels != s->latched_ch) {
        /* The lib latches channels once; a shift mid-stream is a
         * lib invariant break (CONFIG_CHANGE guards it). Per-frame
         * spf legitimately varies on fractional rows (bounded by
         * the latched max below). */
        av_log(avctx, AV_LOG_ERROR,
               "lib channel count changed mid-stream\n");
        ac4_drop_head(s);
        return AVERROR(EINVAL);
    }
    if (f->info.spf <= 0 || f->info.spf > s->latched_spf ||
        f->len != (size_t)f->info.channels * (size_t)f->info.spf * 2) {
        av_log(avctx, AV_LOG_ERROR,
               "lib frame byte count %zu disagrees with frame shape "
               "%dch x %d (latched max spf %d)\n",
               f->len, f->info.channels, f->info.spf, s->latched_spf);
        ac4_drop_head(s);
        return AVERROR(EINVAL);
    }
    /* frame_size is the latched max; per-frame nb_samples varies. */
    if (!avctx->frame_size)
        avctx->frame_size = s->latched_spf;

    frame->nb_samples = f->info.spf;
    ret = ff_get_buffer(avctx, frame, 0);
    if (ret < 0)
        return ret;
    memcpy(frame->data[0], f->pcm, f->len);
    frame->pts = f->pts;
    frame->pkt_dts = f->pts;
    if (f->is_gap) {
        frame->decode_error_flags |= FF_DECODE_ERROR_CONCEALMENT_ACTIVE;
        if (strcmp(f->note, s->last_gap) != 0) {
            av_log(avctx, AV_LOG_WARNING, "gap: %s (silence emitted)\n",
                   f->note);
            snprintf(s->last_gap, sizeof(s->last_gap), "%s", f->note);
        }
        s->gap_frames++;
    }
    av_freep(&f->pcm);
    s->qh++;
    if (s->qh == s->qn)
        s->qh = s->qn = 0;
    return 0;
}

/* Feed one demuxer payload: re-frame to AC40 (the raw demuxer strips
 * sync/size/CRC; the lib wants exactly one sync frame per feed) and
 * drain. Returns 0 (fed, maybe queued), a fatal AVERROR, or 1 meaning
 * "packet skipped, pull the next". */
static int ac4_feed_packet(AC4DecodeContext *s, AVCodecContext *avctx)
{
    size_t need, hlen;
    int rc, ret;
    size_t framelen = 0;

    if (s->pkt->size <= 0 || s->pkt->size > 0xFFFFFF) {
        av_log(avctx, AV_LOG_WARNING,
               "skipping packet #%ld: bad size %d\n",
               s->pkt_idx, s->pkt->size);
        s->trail_skips++;
        return 1;
    }
    if (!(s->pkt->flags & AV_PKT_FLAG_CORRUPT)) {
        /* Clean demuxer framing (sync-valid by construction): counts
         * toward the shim chain mirror even if the lib later rejects
         * its content, and anchors the tail witness. The second
         * chaining releases the held prefix. */
        s->clean_framings++;
        s->trail_skips = 0;
        if (s->clean_framings >= 2)
            s->emit_ok = 1;
    }
    hlen = s->pkt->size < 0xFFFF ? 4 : 7;
    need = hlen + (size_t)s->pkt->size;
    av_fast_malloc(&s->frame_buf, &s->frame_cap, need);
    if (!s->frame_buf)
        return AVERROR(ENOMEM);
    s->frame_buf[0] = 0xAC;
    s->frame_buf[1] = 0x40;
    if (hlen == 4) {
        s->frame_buf[2] = (uint8_t)((s->pkt->size >> 8) & 0xFF);
        s->frame_buf[3] = (uint8_t)(s->pkt->size & 0xFF);
    } else {
        s->frame_buf[2] = 0xFF;
        s->frame_buf[3] = 0xFF;
        s->frame_buf[4] = (uint8_t)((s->pkt->size >> 16) & 0xFF);
        s->frame_buf[5] = (uint8_t)((s->pkt->size >> 8) & 0xFF);
        s->frame_buf[6] = (uint8_t)(s->pkt->size & 0xFF);
    }
    memcpy(s->frame_buf + hlen, s->pkt->data, (size_t)s->pkt->size);

    ff_mutex_lock(&ac4d_mutex);
    rc = ac4d_feed(s->lib, s->frame_buf, need);
    ff_mutex_unlock(&ac4d_mutex);

    if (rc == AC4D_BAD_PACKET) {
        const char *note;

        ff_mutex_lock(&ac4d_mutex);
        note = ac4d_gap_note(s->lib);
        ff_mutex_unlock(&ac4d_mutex);
        s->bad_streak++;
        s->trail_skips++;
        av_log(avctx, AV_LOG_WARNING,
               "skipping bad packet #%ld (%d bytes): %s\n",
               s->pkt_idx, s->pkt->size,
               (note && strcmp(note, "ok") != 0)
                   ? note : ac4d_strerror(AC4D_BAD_PACKET));
        if (s->bad_streak > AC4_MAX_CONSEC_BAD) {
            av_log(avctx, AV_LOG_ERROR,
                   "%d consecutive bad packets; aborting (stuck "
                   "pres_sel or unresyncable input, no parser)\n",
                   s->bad_streak);
            return AVERROR_INVALIDDATA;
        }
        return 1;
    }
    if (rc != AC4D_OK)
        return ac4_lib_fatal(s, avctx, rc, "feed");

    s->bad_streak = 0;
    s->fed_count++;
    ret = ac4_pts_push(s, s->pkt->pts);
    if (ret < 0)
        return ret;

    /* Size the drain scratch from the latch (first feed) or reuse. */
    if (!s->pcm_buf) {
        av_fast_malloc(&s->pcm_buf, &s->pcm_cap, AC4_MAX_FRAME_BYTES);
        if (!s->pcm_buf)
            return AVERROR(ENOMEM);
    }
    ret = ac4_size_pcm(s, avctx, &framelen);
    if (ret < 0)
        return ret;
    if (!framelen) {
        /* No-bed latch (channels 0): still drain so the GAP path is
         * exercised; the latch site fails loud at emit. */
        framelen = 1024;
        if (framelen > s->pcm_cap) {
            av_fast_malloc(&s->pcm_buf, &s->pcm_cap, framelen);
            if (!s->pcm_buf)
                return AVERROR(ENOMEM);
        }
    }
    return ac4_drain(s, avctx, framelen);
}

static int ac4_decode_frame(AVCodecContext *avctx, AVFrame *frame)
{
    AC4DecodeContext *s = avctx->priv_data;
    int ret;

    for (;;) {
        if (s->qh < s->qn && (s->emit_ok || s->fatal)) {
            /* Emit queued audio before surfacing a latched fatal. The
             * first prefix waits for the shape decision (emit_ok): a
             * lone unanchored framing is discarded at EOF, never
             * emitted. A latched fatal releases the hold (prefix
             * audio precedes the error, splice-style). Held frames
             * fall through to pull more input or EOF below. */
            ret = ac4_emit(s, avctx, frame);
            if (ret == AVERROR(EINVAL))
                s->fatal = ret;
            return ret;
        }
        if (s->flush_done) {
            /* Drain finished: report a latched fatal once, then EOF
             * so the scheduler terminates (a sticky error with no EOF
             * retries forever). */
            if (s->fatal) {
                ret = s->fatal;
                s->fatal = 0;
                return ret;
            }
            return AVERROR_EOF;
        }
        if (s->fatal) {
            /* Mid-stream stick: the error was already reported; eat
             * remaining input (one report per discarded packet, so
             * the failure stays loud but bounded) until EOF ends it. */
            ret = ff_decode_get_packet(avctx, s->pkt);
            av_packet_unref(s->pkt);
            if (ret == AVERROR_EOF) {
                s->flush_done = 1;
                continue;
            }
            if (ret < 0)
                return ret;
            return s->fatal;
        }
        if (s->draining) {
            if (!s->fed_count) {
                /* Nothing was ever fed (empty input or every
                 * packet skipped): nothing to drain. Skips were
                 * already logged loud; EOF ends the stream and
                 * the muxer reports the empty output. */
                s->flush_done = 1;
                continue;
            }
            if (s->clean_framings == 1 && s->trail_skips > 0) {
                /* Lone unanchored framing (F2 gap-trunc): the shim's
                 * chain rule rejects a single sync frame that is not
                 * EOF-flushed, so the lone prefix is sync-unproven.
                 * Discard it (never emit) and fail loud: zero output
                 * plus one fatal trips fftools' error-rate abort. */
                av_log(avctx, AV_LOG_ERROR,
                       "no-sync: lone frame followed by %d skipped "
                       "packet(s), input truncated\n",
                       s->trail_skips);
                ac4_queue_reset(s);
                s->fatal = AVERROR_INVALIDDATA;
                s->flush_done = 1;
                continue;
            }
            ret = ac4_drain_flush(s, avctx);
            s->flush_done = 1;
            /* EOF with a decided-good shape: release the held prefix. */
            s->emit_ok = 1;
            if (ret < 0)
                s->fatal = ret;
            continue;
        }
        ret = ff_decode_get_packet(avctx, s->pkt);
        if (ret == AVERROR_EOF) {
            s->draining = 1;
            continue;
        }
        if (ret < 0)
            return ret;
        s->pkt_idx++;
        ret = ac4_feed_packet(s, avctx);
        av_packet_unref(s->pkt);
        if (ret == 1)
            continue; /* skipped packet: pull the next */
        if (ret < 0) {
            s->fatal = ret;
            return ret;
        }
    }
}

static av_cold int ac4_decode_init(AVCodecContext *avctx)
{
    AC4DecodeContext *s = avctx->priv_data;
    int ret;

    if (avctx->request_sample_fmt != AV_SAMPLE_FMT_NONE &&
        avctx->request_sample_fmt != AV_SAMPLE_FMT_S16) {
        av_log(avctx, AV_LOG_ERROR,
               "only S16 output is supported in v1 (float needs "
               "native float-stage plumbing); requested %s\n",
               av_get_sample_fmt_name(avctx->request_sample_fmt));
        return AVERROR(EINVAL);
    }
    avctx->sample_fmt = AV_SAMPLE_FMT_S16;
    s->pkt = avctx->internal->in_pkt;
    s->last_pts = AV_NOPTS_VALUE;

    ret = ac4_lib_open(s, avctx);
    if (ret < 0)
        return ret;
    return 0;
}

/* Seek: close + reopen. Feed-after-flush is AC4D_ERR and the lib has
 * no reset, so every flush drops all queued audio and pts, clears
 * the sticky error, and re-opens; stream params re-latch from the
 * first post-seek frame. */
static void ac4_decode_flush(AVCodecContext *avctx)
{
    AC4DecodeContext *s = avctx->priv_data;

    ac4_queue_reset(s);
    av_packet_unref(s->pkt);
    s->draining = 0;
    s->flush_done = 0;
    s->fatal = 0;
    s->bad_streak = 0;
    s->clean_framings = 0;
    s->trail_skips = 0;
    s->emit_ok = 0;
    s->stream_set = 0;
    s->last_pts = AV_NOPTS_VALUE;
    s->last_gap[0] = '\0';
    avctx->frame_size = 0;
    ff_mutex_lock(&ac4d_mutex);
    ac4d_close(s->lib);
    ff_mutex_unlock(&ac4d_mutex);
    s->lib = NULL;
    if (ac4_lib_open(s, avctx) < 0)
        s->fatal = AVERROR(EINVAL);
}

static av_cold int ac4_decode_close(AVCodecContext *avctx)
{
    AC4DecodeContext *s = avctx->priv_data;

    if (s->gap_frames > 0) {
        av_log(avctx, AV_LOG_INFO,
               "emitted %ld gap-silence frames (concealment; see "
               "warnings above)\n", s->gap_frames);
    }
    ff_mutex_lock(&ac4d_mutex);
    ac4d_close(s->lib);
    ff_mutex_unlock(&ac4d_mutex);
    s->lib = NULL;
    ac4_queue_reset(s);
    av_freep(&s->q);
    av_freep(&s->pend_pts);
    av_freep(&s->frame_buf);
    av_freep(&s->pcm_buf);
    return 0;
}

#define AC4DEC_PARAM (AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_DECODING_PARAM)

static const AVOption ac4_options[] = {
    { "pres_sel", "presentation select (-1 = default)",
      offsetof(AC4DecodeContext, pres_sel),
      AV_OPT_TYPE_INT, { .i64 = -1 }, -1, 63, AC4DEC_PARAM },
    { "drc", "apply dynamic range control (unsupported: fails open)",
      offsetof(AC4DecodeContext, drc_on),
      AV_OPT_TYPE_BOOL, { .i64 = 0 }, 0, 1, AC4DEC_PARAM },
    { "drc_out_ref", "DRC output reference level",
      offsetof(AC4DecodeContext, drc_out_ref),
      AV_OPT_TYPE_INT, { .i64 = -31 }, -31, 0, AC4DEC_PARAM },
    { "drc_boost", "DRC boost factor percent",
      offsetof(AC4DecodeContext, drc_boost_pct),
      AV_OPT_TYPE_INT, { .i64 = 100 }, 0, 200, AC4DEC_PARAM },
    { "drc_cut", "DRC cut factor percent",
      offsetof(AC4DecodeContext, drc_cut_pct),
      AV_OPT_TYPE_INT, { .i64 = 100 }, 0, 200, AC4DEC_PARAM },
    { "de_level", "dialog enhancement level dB, 0 = off "
      "(nonzero unsupported: fails open)",
      offsetof(AC4DecodeContext, de_level),
      AV_OPT_TYPE_DOUBLE, { .dbl = 0.0 }, -12.0, 12.0, AC4DEC_PARAM },
    { NULL },
};

static const AVClass ac4_decoder_class = {
    .class_name = "AC-4 Decoder",
    .item_name  = av_default_item_name,
    .option     = ac4_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const FFCodec ff_ac4_decoder = {
    .p.name         = "ac4",
    CODEC_LONG_NAME("ATSC A/344 Dolby AC-4"),
    .p.type         = AVMEDIA_TYPE_AUDIO,
    .p.id           = AV_CODEC_ID_AC4,
    .p.capabilities = AV_CODEC_CAP_CHANNEL_CONF | AV_CODEC_CAP_DR1
                    | AV_CODEC_CAP_EXPERIMENTAL,
    .p.priv_class   = &ac4_decoder_class,
    .priv_data_size = sizeof(AC4DecodeContext),
    .init           = ac4_decode_init,
    .flush          = ac4_decode_flush,
    .close          = ac4_decode_close,
    FF_CODEC_RECEIVE_FRAME_CB(ac4_decode_frame),
    CODEC_CH_LAYOUTS(AV_CHANNEL_LAYOUT_MONO, AV_CHANNEL_LAYOUT_STEREO,
                     AV_CHANNEL_LAYOUT_5POINT1, AV_CHANNEL_LAYOUT_7POINT1),
    CODEC_SAMPLEFMTS(AV_SAMPLE_FMT_S16),
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP
                    | FF_CODEC_CAP_NOT_INIT_THREADSAFE,
};
