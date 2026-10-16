# DRAFT ONLY — do not copy into the ffmpeg tree yet. (v3, 2026-10-16)
# tests/fate/ac4.mak draft for the native AC-4 decoder + encoder.
# Sources:
#   decode plan: ff-dec-wrapdesign/DEC-WRAPDESIGN.md §6
#   samples/hashes: ff-fate-prep/FATE-PREP.md (2026-10-16)
#   encode plan: ff-enc-wrapdesign/ENC-WRAPDESIGN.md §4
#   conventions: ffmpeg tests/fate/ac3.mak (read-only) + tests/Makefile gates
#   A0 demux proof + 6 corrections: ff-a0-fate-local/A0-FATE-LOCAL.md
#   C encoder goldens: ff-C-fate-local/C-FATE-LOCAL.md (b9ee552)
#   span note: ff-span-triage/SPAN-TRIAGE.md
#   parser slots: ff-parser-torture/TORTURE.md (30 samples)
#   D4 fate decisions: ff-dec-wrapaddendum/WRAP-ADDENDUM.md §D4 (folded, v3)
#   mechanical check: ff-mak-check/MAK-CHECK.md F1-F7 + A1 (folded, v3)
#   crc/gap recipes: ff-fate-more/FATE-MORE.md §1-§2 (via MAK-CHECK F4/F7)
#   B2 negproof (31 runs, A0r2 2f7e33f): ff-B2-negproof/NEGPROOF.md
#     (folded, v3n — garbage pin, decode-phase markers, resync notes)
# VERIFIED-LOCAL = command + value proven against a real local in-tree
#   build (A0 demux or Series C encoder). Re-verify at implementation
#   time, but no longer cleanroom-derived.
# UNVERIFIED = value derived from cleanroom c-ac4enc/c-ac4dec, or not
#   yet written. Every golden/REF below must be re-recorded from the
#   real wrapper at implementation time and must never be committed
#   as-is. Each UNVERIFIED test names an owner track.
#
# --- v2 changelog (delta + source) ---
#  1. fate-ac4-encode: CMD gains -loudness_target 0; REF =
#     0f0e9f6092ea4d489f91a41acef75a59. VERIFIED-LOCAL. (C §T1)
#  2. fate-ac4-encode-51: REF = 53a44acaa21853f91cfef7dbdcccb67e,
#     no CMD change. VERIFIED-LOCAL. (C)
#  3. fate-ac4-framecrc: stdout golden recorded (13 lines, comment
#     below). VERIFIED-LOCAL. (C)
#  4. fate-ac4-tail: stdout golden recorded (13 lines, comment below);
#     ATRIM_FILTER gate confirmed required. VERIFIED-LOCAL. (C)
#  5. fate-ac4-encode: span-sensitivity note (58-pass/59-fail edge,
#     CLI parity, mak-only stands, R2 follow-up). (SPAN-TRIAGE.md)
#  6. NEW fate-ac4-copy-stereo + fate-ac4-copy-51: copy roundtrips
#     compare framecrc, never file bytes (muxer rewrites ac41->ac40,
#     1036 vs 1042 B). UNVERIFIED, owner demux/A0. (A0 corr 1)
#  7. fate-ac4-crc-bad: pinned decode-phase only; no demux-level
#     expected-failure test (demuxer passes payload through, exit 0).
#     UNVERIFIED, owner decoder-wrapper. (A0 corr 2)
#  8. fate-ac4-seek: -ss 0.02 -> -ss 0.05 (input seek); 2-packet
#     suffix shape proven at demux level, decode REF-slice still TBD.
#     UNVERIFIED, owner decoder-wrapper. (A0 corr 3)
#  9. 5.1(side) layout string confirmed; ims120/714 asserts expect
#     the (side)-suffixed naming family. (A0 corr 4)
# 10. NEW fate-ac4-seek-eof slot: past-EOF seek clamps to the last
#     packet (exit 0) — golden must not be empty. UNVERIFIED, owner
#     demux/A0. (A0 corr 5)
# 11. NEW fate-ac4-probe-garbage slot: garbage probe yields a zeroed
#     stub stream (assert sample_rate=0); real failure signal is
#     mux-header EINVAL, exit 234. UNVERIFIED, owner demux/A0.
#     (A0 corr 6)
# 12. NEW parser-torture slot block: 30 samples (t01-t29 + t14b)
#     with expected parser emit counts. UNVERIFIED, owner parser
#     track. (TORTURE.md)
#
# --- v3 changelog (delta + source) ---
# 13. fate-ac4-gap-fails REDEFINED per D4: CMD pcm -> framemd5, input
#     clip-51-aspx.ac4 -> clip-51-acpl4.ac4 (codec-4 5.1, T2 file);
#     pins deterministic-silence bytes via default diff vs in-tree
#     golden; comment cites expected gap_note + D1 flag rule. F1's
#     grep-composition suggestion DECLINED per D4/P14 (zero
#     stderr-grep precedent in tests/). SAMPLE-TBD + GOLDEN-TBD,
#     UNVERIFIED, owner decoder-wrapper. (D4; F1)
# 14. NEW fate-ac4-stereo51 (D4/T1): clip-stereo51.ac4, framemd5 pins
#     c0/c1 content + c2-c5 exact-zero + 6ch-presented assert
#     (ims120 pattern). SAMPLE-TBD + GOLDEN-TBD, UNVERIFIED, owner
#     decoder-wrapper. (D4)
# 15. NEW fate-ac4-delay (D4/T3): clip-impulse.ac4, framemd5 of
#     trimmed output; comment documents expected leading trim = spf.
#     SAMPLE-TBD + GOLDEN-TBD, UNVERIFIED, owner decoder-wrapper. (D4)
# 16. fate-ac4-51: D4 input note — prefer clip-chanbased.ac4 hot-LFE
#     (T4 file); CMD keeps prepared clip-51-real.ac4 until
#     equivalence is confirmed; golden pins saturated S16 bytes
#     as-is, never requantized float. UNVERIFIED. (D4)
# 17. DEFER slots (comment-only): fate-ac4-delay-1536 (needs
#     1536-spf impulse ref; falsifies D2's 1-frame rule) and
#     fate-ac4-float (needs float-stage plumbing, D3). (D4)
# 18. fate-ac4-seek: REF = full ref -> TBD seek-offset slice file
#     clip-stereo-seek-0.05.pcm (v2 REF guaranteed fail). UNVERIFIED,
#     owner decoder-wrapper. (F2)
# 19. fate-ac4-crc-bad reworked per FATE-MORE (slot FILLED): input
#     crc-bad.ac4 -> pack name clip-crc-bad.ac4; CMD pcm -> framemd5;
#     default diff vs in-tree golden (exit 0, 1 frame; decoded-bytes
#     anchor sha256 2ee1ade1... = 2nd half of clip-stereo-ref.pcm).
#     NEW companion fate-ac4-crc-bad-note (CMP=grep, REF=crc1) for
#     the stderr reject-count assert; "no stock assert" note
#     corrected (grep + `; test` is stock, astc.mak:142-152).
#     UNVERIFIED, owner decoder-wrapper. (F3, F4)
# 20. fate-ac4-probe-garbage: CMD gains `; test $$? -eq 234` (exit
#     demux-proven, A0 X3) + CMP=grep + REF=diagnostic (mux-header
#     EINVAL message TBD at implementation); ffprobe
#     sample_rate=0 assert noted as alternative. UNVERIFIED, owner
#     demux/A0. (F5)
# 21. Gate split (F6): pcm-CMD tests stay under $(call PCM,...);
#     framemd5-CMD tests move to FATE_AC4_MD5 under $(call
#     FRAMEMD5,...); -c copy/framecrc demux slots move to
#     FATE_AC4_REMUX under $(call REMUX,...) (no decoder dep).
#     Fixes --disable-everything breaks; replaces the L180 NOTE.
# 22. NEW fate-ac4-gap-trunc (F7): clip-gap-fail.ac4 (head -c 500,
#     sha256 e54faf83...) expects nonzero exit + no-sync grep
#     (astc idiom); no-output assert TBD at implementation.
#     UNVERIFIED, owner decoder-wrapper.
# 23. A1 advisory RECORDED, wiring unchanged: FATE_AC4_ENC stays in
#     FATE_SAMPLES_AVCONV (matches ac3 fixed-encode wart-for-wart);
#     FATE_FFMPEG move stays open for implementation. (A1)
#
# --- v3n changelog (B2 negproof fold; NEGPROOF.md cases 1-31, A0r2) ---
# 24. fate-ac4-probe-garbage: garbage-234 wording PINNED (was TBD):
#     `unspecified sample format` + `Output file does not contain any
#     stream`, exit 234 — DISTINCT from the unset-rate family
#     (`unspecified sample rate` + `sample rate not set`). REF set to
#     the pinned string; B-time must re-confirm against SUITE bytes
#     (negproof ran A0-local garbage 785b0751..., suite stages
#     mak-samples garbage b6c0fe39...). (NEGPROOF case 30)
# 25. Decode-phase markers EXTENDED: crc-bad passthrough re-confirmed
#     (cases 8-11, 3 pkts exit 0); mid-stream fri/rate 14/15 added as
#     decode-phase (cases 28-29, 3-pkt passthrough) — first-frame
#     fri14/15 IS demux-gated (234, cases 12-13). No demux-level
#     reject exists for either; stays out of demux FATE.
# 26. Resync rule PINNED: demuxer does NOT resync — leading noise
#     (t01) → 234 + 0 pkts, mid-stream noise (t02) → 1 pkt +
#     fail-stop `Error during demuxing...`. Resync belongs to the
#     parser lane; parser-block t02 "via resync" is PARSER behavior
#     (demux contrast annotated inline). (NEGPROOF cases 16-17)
# 27. fate-ac4-gap-trunc: demux shape RECORDED (case 5: pkt0 clean +
#     148 B F=0x3 tail, exit 0) — the nonzero/no-sync assert is
#     decode-phase on top of demux exit-0 passthrough.
# 28. First-frame 234 family DOCUMENTED (trunc-heads cases 2-4,
#     fri14/15 cases 12-13, bv1 case 14, multipres case 15, ext-huge
#     case 23): demux-citable but no FATE expected-failure mechanism
#     → local-proof only, cite NEGPROOF.md.
# 29. Single-frame-at-EOF (t22/t29) emit-1 CONFIRMED at demux
#     (cases 25/27); decoder-side no-sync gap stays flagged
#     per-layer. NOPTS-on-unvisited (case 31) has no FATE
#     mechanism; cite NEGPROOF.md.
#
# Gate note: `pcm`-CMD decode tests use $(call PCM,...) — tests/Makefile
#   documents PCM as the "Variant of DEMDEC for use with the pcm command"
#   (adds PCM_S16LE_MUXER + PIPE_PROTOCOL that bare DEMDEC lacks).
#   framemd5-CMD tests use $(call FRAMEMD5,...) (adds FRAMEMD5_MUXER),
#   demux-only -c copy/framecrc slots use $(call REMUX,...) (muxer +
#   demuxer + FRAMECRC_MUXER, no decoder dep). (F6)
# Wiring note: ENC-WRAPDESIGN §4 says "FATE_SAMPLES_FFMPEG wiring per ac3.mak
#   tail", but the ac3.mak tail actually wires these into FATE_SAMPLES_AVCONV
#   (line 216). This draft follows the ac3.mak tail literally (A1 keeps
#   this as the defensible default; FATE_FFMPEG move open).
# tests/Makefile include spot: between ac3.mak and adpcm.mak (~line 156).

# --- decode (pcm): stereo, byte-exact pcm vs suite ref (prepared sample) ---
FATE_AC4 += fate-ac4-stereo
fate-ac4-stereo: CMD = pcm -i $(TARGET_SAMPLES)/ac4/clip-stereo.ac4
fate-ac4-stereo: REF = $(SAMPLES)/ac4/clip-stereo-ref.pcm
# UNVERIFIED, owner decoder-wrapper: suite files clip-stereo.ac4 (1042 B,
# sha256 254c7ebc...) + clip-stereo-ref.pcm (16384 B s16le 48k 2ch,
# sha256 5fe8ef4c...) per FATE-PREP.md; ref recorded from cleanroom
# c-ac4dec, wrapper output must be confirmed byte-identical before
# upload/commit. Demux half proven (A0 D1: ac4/48k/stereo; F1: 3 pkts
# pts 0/2048/4096 dur 2048, crcs 0x23081287 0xbad70aae 0x27346f7e).
fate-ac4-stereo: CMP = oneoff

# --- decode (pcm): mono (sample NOT prepared) ---
FATE_AC4 += fate-ac4-mono
fate-ac4-mono: CMD = pcm -i $(TARGET_SAMPLES)/ac4/clip-mono.ac4
fate-ac4-mono: REF = $(SAMPLES)/ac4/clip-mono-ref.pcm
# SAMPLE-TBD + REF-TBD (UNVERIFIED, owner samples/encoder track): §6
# wants clip-mono.ac4; FATE-PREP.md did not build it (needs
# encoder-side shape beyond plain stereo/5.1).
fate-ac4-mono: CMP = oneoff

# --- decode (pcm): seek-back == straight decode (flush re-prime) ---
FATE_AC4 += fate-ac4-seek
fate-ac4-seek: CMD = pcm -ss 0.05 -i $(TARGET_SAMPLES)/ac4/clip-stereo.ac4
fate-ac4-seek: REF = $(SAMPLES)/ac4/clip-stereo-seek-0.05.pcm
# UNVERIFIED, owner decoder-wrapper: REF is the TBD seek-offset slice
# (2-packet suffix), NOT the full ref (F2; seek.mak idiom vs plain
# -ss TBD at implementation). -ss value concrete (A0 corr 3: input
# -ss 0.05 drops pkt 0, emits the 2-packet suffix; -ss 0.02 keeps
# all 3 rebased; output -ss 0.05 emits last pkt only). Decode-phase
# seek REF-slice decision still open; demux shape above is proven,
# not the decoded bytes.
fate-ac4-seek: CMP = oneoff

# --- decode (framemd5): 5.1 real (no-shape-gap), framemd5 (prepared sample) ---
FATE_AC4_MD5 += fate-ac4-51
fate-ac4-51: CMD = framemd5 -i $(TARGET_SAMPLES)/ac4/clip-51-real.ac4
# UNVERIFIED, owner decoder-wrapper: no framemd5 golden exists yet;
# stdout golden goes to tests/ref/fate/fate-ac4-51 at implementation
# time. Decode-shape hint from FATE-PREP.md: 2 frames x 2048 s16le 48k
# 6ch, sha256 354f6e0c... (cleanroom c-ac4dec, must be re-recorded
# from wrapper). Demux half proven (A0 D2: ac4/48k/6ch 5.1(side); F2:
# 3 pkts same pts shape, crcs 0x9c591a26 0x469a1126 0xada5fdc8).
# Layout string 5.1(side) confirmed for the channel_layout_name golden
# line (A0 corr 4).
# D4 INPUT NOTE (T4): prefer clip-chanbased.ac4 (T4 hot-LFE file) or
# equivalent hot-LFE clip; CMD keeps prepared clip-51-real.ac4 until
# equivalence is confirmed at implementation. Golden pins saturated
# S16 bytes as-is — no quantized-float golden is ever pinned (D3).

# --- decode (framemd5): cm120 immersive-stereo, framemd5 + 6ch-presented assert ---
FATE_AC4_MD5 += fate-ac4-ims120
fate-ac4-ims120: CMD = framemd5 -i $(TARGET_SAMPLES)/ac4/clip-ims120.ac4
# SAMPLE-TBD + GOLDEN-TBD (UNVERIFIED, owner decoder-wrapper): needs
# 6ch-presented assertion mechanism (ffprobe stream check or -af astats
# invariant — TBD at implementation; DEC-WRAPDESIGN Q4 may change
# presented layout). Expect the (side)-suffixed naming family per the
# confirmed 5.1(side) string (A0 corr 4).

# --- decode (framemd5): cm253/7.1, framemd5 + 8ch assert ---
FATE_AC4_MD5 += fate-ac4-714
fate-ac4-714: CMD = framemd5 -i $(TARGET_SAMPLES)/ac4/clip-714.ac4
# SAMPLE-TBD + GOLDEN-TBD (UNVERIFIED, owner decoder-wrapper): needs
# 8ch assertion mechanism (same TBD as ims120; same (side)-family
# layout naming per A0 corr 4).

# --- decode (framemd5): stub-gap clip pins silence bytes, never silent-pass ---
FATE_AC4_MD5 += fate-ac4-gap-fails
fate-ac4-gap-fails: CMD = framemd5 -i $(TARGET_SAMPLES)/ac4/clip-51-acpl4.ac4
# SAMPLE-TBD + GOLDEN-TBD (UNVERIFIED, owner decoder-wrapper): D4
# REDEFINE (was volumedetect+stderr-grep, both zero-precedent per
# P14 — F1's grep composition explicitly declined). Input is the T2
# codec-4 5.1 stub file (clip-51-acpl4.ac4; supersedes the
# FATE-MORE clip-51-aspx.ac4 name — confirm alias at implementation).
# Test pins deterministic-silence bytes via default diff vs
# tests/ref/fate/fate-ac4-gap-fails. Gap observability stays
# CLI-stderr (human) + decode_error_flags (API): wrapper must emit
# exact-zero PCM + WARNING with gap_note (once-per-change + trailing
# summary; expected gap_note string TBD) + set
# FF_DECODE_ERROR_CONCEALMENT_ACTIVE per gap frame, and must NOT
# latch sample_rate/ch_layout from a gap frame (D1). Port-verify:
# if gap PCM is not exact-zero, ref must pin actual bytes + note why.

# --- decode (framemd5): stereo program in 5.1 bed (T1-class) ---
FATE_AC4_MD5 += fate-ac4-stereo51
fate-ac4-stereo51: CMD = framemd5 -i $(TARGET_SAMPLES)/ac4/clip-stereo51.ac4
# SAMPLE-TBD + GOLDEN-TBD (UNVERIFIED, owner decoder-wrapper): D4 ADD
# (T1 bed/program pin). framemd5 pins c0/c1 content + c2-c5
# exact-zero; needs the same 6ch-presented assert as ims120.

# --- decode (framemd5): impulse pins codec delay (T3-class) ---
FATE_AC4_MD5 += fate-ac4-delay
fate-ac4-delay: CMD = framemd5 -i $(TARGET_SAMPLES)/ac4/clip-impulse.ac4
# SAMPLE-TBD + GOLDEN-TBD (UNVERIFIED, owner decoder-wrapper): D4 ADD
# (T3 delay embed). Clip is a T3-class impulse (few frames; cf. T3
# m3-imp dual-mono impulse train). Golden is POST-trim bytes (generic
# trim active by default); comment at implementation must document
# expected leading trim = spf (2048 @ spf-2048, D2 1-frame rule) —
# any delay change shifts bytes and fails md5, which is the point.

# --- decode (framemd5): CRC-bad frame rejected + counted, survivor decodes ---
FATE_AC4_MD5 += fate-ac4-crc-bad
fate-ac4-crc-bad: CMD = framemd5 -i $(TARGET_SAMPLES)/ac4/clip-crc-bad.ac4
# UNVERIFIED, owner decoder-wrapper: FATE-MORE slot FILLED (F4). Pack
# name clip-crc-bad.ac4 (1042 B, sha256 527fac3f...; F3 — the v1/v2
# crc-bad.ac4 name is superseded). Expect exit 0, 1 frame decoded,
# 1 bad frame rejected + counted; stdout golden goes to
# tests/ref/fate/fate-ac4-crc-bad at implementation time.
# Decoded-bytes anchor: clip-crc-bad-ref.pcm (8192 B s16le 48k 2ch,
# sha256 2ee1ade1..., cleanroom c-ac4dec — re-record from wrapper),
# byte-identical to the 2nd half of clip-stereo-ref.pcm.
# DECODE-PHASE ONLY: the A0 demuxer does not CRC-check packet payload
# and passes corrupted bytes through (exit 0), so do NOT add a
# demux-level expected-failure variant unless packet CRC validation
# is explicitly added to the demuxer (A0 corr 2). Re-confirmed by
# NEGPROOF cases 8-11 (t08/t09/t10 + clip-crc-bad: 3-pkt
# passthrough, exit 0, no reject).
# DECODE-PHASE ONLY (sibling): mid-stream fri/rate 14/15 (t17/t18)
# also passes through at demux (3 pkts, exit 0; pkt1 crc 0xbd7d0ab0
# = flipped byte only) — the TOC gate is decode-phase, stays out
# of demux FATE. CONTRAST: FIRST-frame fri 14/15 (n-fri14/15) IS
# demux-gated: params unset + `sample rate not set`, exit 234, 0
# pkts (NEGPROOF cases 12-13; same 234 shape for bv1 case 14 and
# multipres case 15). No FATE expected-failure mechanism exists
# for the 234 family → local-proof only, cite NEGPROOF.md.
FATE_AC4_MD5 += fate-ac4-crc-bad-note
fate-ac4-crc-bad-note: CMD = framemd5 -i $(TARGET_SAMPLES)/ac4/clip-crc-bad.ac4
fate-ac4-crc-bad-note: CMP = grep
fate-ac4-crc-bad-note: REF = crc1
# UNVERIFIED, owner decoder-wrapper: stderr companion to crc-bad
# (FATE-MORE recipe "framemd5 vs ref + grep stderr crc1"; CMP=grep
# against errfile + `; test` exit assert is stock, astc.mak:142-152).
# REF string assumes the wrapper surfaces a crc1 reject count; exact
# diagnostic wording TBD at implementation.

# --- decode (framemd5+grep): truncated input must fail loudly, never silent-pass ---
FATE_AC4_MD5 += fate-ac4-gap-trunc
fate-ac4-gap-trunc: CMD = framemd5 -i $(TARGET_SAMPLES)/ac4/clip-gap-fail.ac4 ; test $$? -ne 0
fate-ac4-gap-trunc: CMP = grep
fate-ac4-gap-trunc: REF = no-sync
# UNVERIFIED, owner decoder-wrapper: F7 coverage for pack file
# clip-gap-fail.ac4 (head -c 500 of clip-stereo.ac4, mid-frame-1,
# sha256 e54faf83...). CLI behavior: exit 3, note=no-sync, NO output
# file; wrapper exit code TBD, so CMD asserts nonzero (astc idiom)
# and grep asserts the no-sync diagnostic (exact wording TBD).
# No-output assert (empty/missing outfile check) TBD at
# implementation — this slot must never silent-pass (FATE-MORE §1).
# ADJACENT to fate-ac4-gap-fails, does not fill it.
# DEMUX SHAPE (NEGPROOF case 5, proven — the nonzero/no-sync assert
# above is DECODE-PHASE on top of this): demux emits pkt0 clean
# (crc 0x23081287 == base f0) + short trailing pkt (148 B, F=0x3
# corrupt flag), exit 0. Sibling: trunc-tail-midframe (t11) gives
# pkt2 101 B F=0x3 with pkts 0-1 crc-matching base (case 20).

# --- deferred fate slots (comment-only, D4) ---
# fate-ac4-delay-1536: DEFERRED — needs a 1536-spf impulse ref (does
#   not exist); same shape as fate-ac4-delay; falsifies D2's 1-frame
#   rule (T3 open Q).
# fate-ac4-float: DEFERRED — needs float-stage plumbing (D3); no
#   quantized-float golden is ever pinned before then (T4 note).

# --- demux: copy roundtrips, framecrc CMP (muxer rewrites file bytes) ---
FATE_AC4_REMUX += fate-ac4-copy-stereo
fate-ac4-copy-stereo: CMD = framecrc -i $(TARGET_SAMPLES)/ac4/clip-stereo.ac4 -c copy
# UNVERIFIED, owner demux/A0: golden tests/ref/fate/fate-ac4-copy-stereo
# TBD (record full stdout at implementation). Proven demux shape (A0
# F1/C1): 3 pkts pts 0/2048/4096 dur 2048, crcs 0x23081287 0xbad70aae
# 0x27346f7e; re-demux of the copy is framecrc byte-identical. MUST
# compare framecrc/md5, never file bytes: the ac4 muxer rewrites
# framing (sync byte 2 ac41->ac40, output 6 B shorter: 1036 vs 1042
# stereo). Packet payloads are bit-exact, files differ (A0 corr 1).
FATE_AC4_REMUX += fate-ac4-copy-51
fate-ac4-copy-51: CMD = framecrc -i $(TARGET_SAMPLES)/ac4/clip-51-real.ac4 -c copy
# UNVERIFIED, owner demux/A0: golden tests/ref/fate/fate-ac4-copy-51
# TBD. Proven demux shape (A0 F2/C2): 3 pkts same pts shape, crcs
# 0x9c591a26 0x469a1126 0xada5fdc8. Same framecrc-not-file-bytes rule
# as copy-stereo (A0 corr 1).

# --- demux: past-EOF seek clamps to last packet (edge slot) ---
FATE_AC4_REMUX += fate-ac4-seek-eof
fate-ac4-seek-eof: CMD = framecrc -ss 5 -i $(TARGET_SAMPLES)/ac4/clip-stereo.ac4 -c copy
# UNVERIFIED, owner demux/A0: golden tests/ref/fate/fate-ac4-seek-eof
# TBD. Proven shape (A0 S4): emits the LAST packet (pts -235904),
# exit 0 — generic clamp, no crash. Golden must be the clamped last
# packet, not zero packets (A0 corr 5).

# --- demux: garbage input yields zeroed stub stream (negative slot) ---
FATE_AC4_REMUX += fate-ac4-probe-garbage
fate-ac4-probe-garbage: CMD = framecrc -i $(TARGET_SAMPLES)/ac4/garbage.ac4 -c copy ; test $$? -eq 234
fate-ac4-probe-garbage: CMP = grep
fate-ac4-probe-garbage: REF = Output file does not contain any stream
# SAMPLE-STAGED + WORDING-PINNED (UNVERIFIED bytes, owner demux/A0):
# garbage input (1 KiB noise) probes by extension with a zeroed stub
# stream (`Stream #0:0: Audio: ac4, 0 channels`). F5 astc idiom: CMD
# asserts the exit (234 = EINVAL) and CMP=grep asserts the pinned
# diagnostic. Proven stderr shape (NEGPROOF case 30, A0r2):
#   `Format ac4 detected only with low score of 1`
#   `... Could not find codec parameters ... : unspecified sample format`
#   `Output file does not contain any stream`
#   `Error opening output files: Invalid argument`
# PIN SEPARATELY from the unset-rate 234 family (trunc-heads,
# fri14/15, bv1, multipres): those say `unspecified sample rate` +
# `sample rate not set` (cases 2-4/12-15) — garbage says sample
# FORMAT, not rate. Same garbage shape for noise-prefix t01 (case
# 16: NO header resync — demuxer does not skip leading noise).
# BYTES CAVEAT: negproof ran A0-local garbage.ac4 (1024 B, sha256
# 785b0751...); suite stages mak-samples garbage.ac4 (1024 B,
# Random(0xAC40), sha256 b6c0fe39...). Re-confirm REF against the
# suite bytes at B-time; REF follows suite bytes if they differ.
# Alternative: an ffprobe sample_rate=0 assert stanza instead of
# the exit-code grep.

FATE_AC4-$(call PCM, AC4, AC4, ARESAMPLE_FILTER) += $(FATE_AC4)
FATE_AC4_MD5-$(call FRAMEMD5, AC4, AC4, ARESAMPLE_FILTER) += $(FATE_AC4_MD5)
FATE_AC4_REMUX-$(call REMUX, AC4) += $(FATE_AC4_REMUX)
# F6: three split gates (PCM / FRAMEMD5 / REMUX) so
# --disable-everything runs pull only the deps each group needs.
# REMUX = muxer + demuxer + FRAMECRC_MUXER, no decoder dep —
# correct for the -c copy demux-only slots.

# --- parser torture slots (UNVERIFIED, owner parser track; TORTURE.md) ---
# Samples in ff-parser-torture/samples/ + SHA256SUMS (deterministic
# build.py, 30 samples from clip-stereo.ac4: 3x AC41 frames at off
# 0/348/695); upload to fate-suite as ac4/tNN-*.ac4 at implementation.
# Mechanism TBD (parser FATE harness or demux+framecrc packet counts).
# Expected parser emits (assert PARSER behavior, not decoder parity):
#   3 pkts: t01 noise-prefix, t03 noise-suffix, t04 swapped-endian,
#     t05 regtag, t15 extlen-valid, t19 fs-flip, t20 ig-clear-f0
#     (key_frame f0=0), t21 ac40-chain, t26 mixed-ac40-ac41,
#     t27 seq-zero, t28 seq-jump
#   3 pkts via resync: t02 noise-inter (f0,f1,f2)
#   2 pkts: t08 crc-bad-mid (drop f1), t10 crc-field-corrupt (drop f0),
#     t11 trunc-tail-midframe, t12 trunc-header, t16 extlen-huge
#     (drop head, resync f1+f2), t17 rate14 + t18 rate15 (TOC gate
#     drops f1; t17 -v debug proves oracle skips it at decode)
#   0 pkts, clean EOF, no hang: t06 empty, t07 noise-only,
#     t09 crc-bad-all, t13 trunc-ext-incomplete, t14 ext-hdr-nopayload,
#     t14b trunc-single, t23 ac40-single-naked, t24 ac40-false-embed
#   1 pkt (PARSER-DESIGN §3.8; DIVERGES from oracle no-sync):
#     t22 ac40-single-eof, t29 ac41-single-eof — single frame at EOF
#     wants >=2 chained in the decoder; FATE asserts emit-1
#   t25 ac40-falsechain: bounded recovery (re-lock at f1, no
#     hang/overread), NEVER rejection — the oracle itself false-locks
#     (§10.1 residual is real: bogus [10,388) + f1 + f2)
# (TORTURE.md finding 3 is truncated in the source file; used as-is.)
# DEMUX CONTRAST (NEGPROOF.md, A0r2 — do NOT assert demuxer resync;
# resync belongs to the parser lane; cite case #s, not parser counts):
#   t01 noise-prefix: PARSER 3 pkts, but DEMUX → 234 + 0 pkts, no
#     header resync past leading noise (case 16; garbage-shaped
#     stderr — see probe-garbage note above).
#   t02 noise-inter: PARSER "3 pkts via resync", but DEMUX → 1 pkt
#     (f0 clean) + fail-stop `Error during demuxing: Invalid data
#     found when processing input`, exit 0, no resync past noise
#     (case 17). Same fail-stop shape: n-badsync1 (case 7, 1 pkt)
#     and t23 ac40-single-naked (case 26, 1 pkt + fail-stop).
#   t08/t09/t10 + clip-crc-bad: PARSER drops, but DEMUX passes all
#     3 pkts through, exit 0 (cases 8-11) → decode-phase only.
#   t17/t18 rate14/15-mid: PARSER/TOC drops f1, but DEMUX passes
#     all 3 pkts through, exit 0 (cases 28-29) → decode-phase only.
#   t22/t29 single-at-EOF: DEMUX emits the lone frame (1 pkt,
#     cases 25/27) matching PARSER emit-1; decoder-side no-sync gap
#     stays flagged per-layer, assert each layer's own behavior.
#   t16 extlen-huge: DEMUX → `Truncating packet of size 16777215
#     to 1038` + unset + 234, 0 pkts (case 23).
#   t11/t12/t15: DEMUX t11 = 3 pkts w/ 101 B F=0x3 tail (case 20),
#     t12 = 2 clean pkts, 2 B stub dropped silently (case 21),
#     t15 = 3 pkts w/ pkt0 crc == base f0 (case 22).

# --- B-series tail (decode/demux only; ENC terms re-added at C4) ---
FATE_SAMPLES_AVCONV += $(FATE_AC4-yes) $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)

fate-ac4: $(FATE_AC4-yes) $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)
