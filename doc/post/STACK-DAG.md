# STACK-DAG — one ordered stack for the six AC-4 series (M1/M2)

Seat: ff-must-fix-stack. Base: master `97d224c`. This record defines
the single stack order every cover and mbox now follows, the
resolution of each M1 collision site, and what each content seat
must preserve when re-cutting its series.

## The DAG (linear)

```
A0 (1: raw demux params+timestamps, pristine 3e6cd5e head)
 └─> A (2: decoder core+wrapper)
      └─> B (2: decoder docs + FATE)
           └─> C (11: encoder + docs + FATE)
                └─> E (2: TS descriptors + FATE)
                     └─> D (6: parser + MP4 + FATE)
```

24 patches total. `git am` in this order on pristine `3e6cd5e`
with plain settings + `--whitespace=error` is rc=0, 24/24
(proof below). Every cover states this chain with its stack parent.

## Why this order (constraints)

- A stacks after A0 (assumes the carriage fill at runtime for
  copy/seek pts; zero file overlap, but the stack applies
  head-first and C's context chains through both).
- B needs A (frozen option list for docs; decoder for legs) and A0
  (pts-bearing copy/seek goldens; garbage failure shape).
- C needs B (extends the docs row; appends the FATE mak) and A
  (Makefile/allcodecs context).
- E needs C (tests run `-c:a ac4`; appends the C-extended mak).
- D needs C (DSI-extradata contract; api/Makefile entries) and E
  (6/6 asserts E's 0x7F/0x15 bytes; appends the E-extended mak).
- B-before-C is forced by M18 (C extends B's docs row, single row);
  C-before-B would also strand B's encoder-negative guard.

## M1 site resolutions (all in the re-cut mboxes)

| File | Series | Resolution | Owner going forward |
|---|---|---|---|
| libavcodec/Makefile (~220) | A then C | A keeps pristine position (after AC3_MF); C01/C02/C03 hunks re-cut against the A-applied tree (encoder lines before AC3_MF, decoder block after) | C seat: preserve A-block context |
| libavcodec/allcodecs.c (~430) | A then C | A adds decoder extern; C03 adds encoder extern after it | C seat: keep decoder-first order |
| libavcodec/Makefile (~1285, parsers) | D | Untouched by A/C; D01 applies as-is | D seat |
| configure (demuxer selects ~3888 vs float probe ~7957) | C then D | Disjoint regions; both apply | D seat |
| Changelog `<next>` | B, C, E | B appends at list end; C prepends at list top; E inserts after C's line. Three disjoint anchors, no collisions | D seat: parser + mov lines go after E's line (see M19) |
| doc/general_contents.texi (@item AC-4) | B then C | B lands the decode-only row; C04 rebased below it (two rows interim; M18 single-row fold rides the C-seat C04 re-cut, which must stay chained on B's row) | C seat (M18) |
| tests/Makefile (ac4.mak include) | B | B adds it; C05's duplicate hunk dropped | — |
| tests/fate/ac4.mak | B creates; C/E/D append | B 2/2 creates (B section + aggregate); C05 appends encoder section + extends aggregate; C06 flips its own tail line; C09 appends fold8; E 2/2 appends mux section; D 4/6-6/6 append parser/mp4/tsdemux sections. Aggregate grows monotonically: B… → …ENC → …PMT PROBE → …PARSE MP4 TSDEMUX | C/D seats: keep appendix shape, extend aggregate |
| tests/api/Makefile | D | Untouched by A/B/C/E; D hunks apply as-is | D seat |

A0 (lavf/ac4dec.c, demux.mak, seek.mak) and A (lavc only) touch
files no other series touches: zero mechanical change needed.

## Content fixes in this re-cut (beyond stacking)

- M22 (E 2/2 message): the "[mpegts] frame size not set" noise is
  real but the old blame ("encoder never sets frame_size") is
  false. True cause, proven: mpegtsenc's default-PCR helper calls
  `av_get_audio_frame_duration2(par, 0)`; with frame_bytes=0 the
  frame_size fallback cannot fire (utils.c) while AC-4 has no
  switch case, so it warns once and uses 512 (PCR cadence only).
  The encoder DOES set frame_size=2048. Message rewritten with
  the true cause; the loglevel-error dodge stays (proven necessary).
- M19 E-part: E 1/2 gains a Changelog line after C's
  (`- MPEG-TS muxer: AC-4 descriptors`). One policy: B/C/E carry
  lines; D 1/6 + 3/6 add the parser + mov lines after E's (D seat).
- M6 B-part: B 1/2 documents pres_sel only (DRC/DE docs deleted;
  A seat deletes the dead options in A 2/2).
- M8 B-part: fate-ac4-crc-bad renamed fate-ac4-skip-bad (+ ref),
  honest comments (pins skip-with-warning, not a reject). A seat
  verifies CRC or keeps the honest name.
- M17 B-part: C08's two layout negatives folded into B 2/2 as
  FATE (GEN lavfi→wav + `; test $$? -eq 234` + grep REF), behind
  `ifeq ($(CONFIG_AC4_ENCODER),yes)` (a bare ALLYES hard-errors on
  the missing pre-C config — proven). C seat deletes
  tests/ac4-neg-check.sh (C08 drops out; series becomes 10).
- T1: cover-min — COVER-A's circular "byte-exact vs oracle" claim
  replaced with an honest port-parity note; B 2/2 message + mak
  header state goldens pin current behavior. External anchor
  (ETSI vectors / independent-decoder diff) stays a follow-up:
  it needs suite-uploaded reference PCM, which no offline source
  provides (c-ac4dec/DEE are same-lineage, not independent).
- T2: fractional pair over pre-existing suite
  audio_id_2ch_2997fps.ac4 (fri row 3, 120 frames): fate-ac4-frac
  (decode, pins 1601/1602 output counts) + fate-ac4-frac-copy
  (demux, pins 1601/1602 pts alternation). Goldens recorded from
  the proof stack, deterministic re-run.
- T3: NOT landed (placement: Series D). Spec for the D seat: an
  encoder→decoder audio roundtrip FATE (asynth in → `-c:a ac4` →
  decode → framemd5 pin) catching priming/skip + DSI end-to-end
  (encoder initial_padding 4096 vs decoder no-skip offset is
  untested). Natural home: extend D 5/6's roundtrip area or a new
  mak section after it (needs A+C+E in-stack; D-tip has all).
  Landing it in B/C/E is wrong (B pre-dates the encoder; C is
  synth-self-contained by design; E defers framecrc to post-D).
- T4: B-side disclosure — ims120 mak comment + 2/2 message now
  state the default-pres fatality is a known v1 gap under
  EXPERIMENTAL, covered pres-1-only. A seat: drain+reopen (or the
  disclosure stands as the v1 contract).
- T5: SOLVED B-side. Root cause proven: ffmpeg CLI input-seeking
  subtracts the -ss target from every stamp (ts_offset,
  fftools/ffmpeg_demux.c) unless -copy_ts; `-copyts` shows the
  demuxer's true +4096 clamp stamp. A0 blameless (its stamps are
  frame_dur multiples; -235904 is not). seek-eof carries -copyts
  and the golden re-pins to the true +4096.
- E probe post-D break (found in-stack): D's parser makes
  find_stream_info open the experimental decoder, failing the E
  probe test on post-D trees. E 2/2 probe carries -strict -2
  (verified green pre- and post-D).
- M21: A0 does NOT set need_parsing. Proven: HEADERS parsing is
  byte-compatible but flags delta frames non-KEY (`, F=0x0`),
  breaking A0/B goldens on post-D trees with no way to keep both
  pre-D and post-D trees green (A0-after-D would cycle: B needs
  A0 needs D needs C needs B). Follow-up after this stack lands:
  raw demuxer sets need_parsing=HEADERS, goldens re-pinned once
  to parser keyflags, rate tables converged via one shared
  header. A0 keeps peek + whole-frame KEY; its message stays true.
- M3 residue: COVER-A0's UNSENT line refreshed (suite live
  2026-10-16T22:23:09Z 12/12; post at/after +24h clock end).
- A0 mbox: byte-identical re-cut (verified stack-clean as head;
  no M1/M2/M21/T5 change required after the B-side resolutions).

## Placeholders the content seats own (marked here, not in mboxes)

- C seat: M10 rename, M11 EXPERIMENTAL, M12 cap, M13 sincos, M14
  allocators, M15 TLS, M16 bump-drop, M17 script delete (C08 drops;
  series → 10, COVER-C renumber), M18 single-row C04 (chained on
  B's row), T3 if D seat disagrees. Preserve: Makefile/allcodecs
  A-context, mak appendix shape + aggregate order, Changelog top
  anchor, C05 message appendix note.
- A seat: M4 strip, M5 scrub, M6 option delete (docs already
  shrunk here), M7 unref, M8 CRC-verify (or keep skip-bad name),
  M9 portability, T4 drain+reopen. Preserve: Makefile/allcodecs
  hunk positions (C chains on them).
- D seat: M19 lines (parser + mov after E's Changelog line), M20
  split-or-fail, T3 roundtrip (spec above). Preserve: mak appendix
  shape + aggregate order, api/Makefile chain.
- No seat may reintroduce a pristine-cut hunk on a chained file:
  re-verify 24/24 (23/24 after C08 drops) on every re-cut.

## Proof record (this seat, scratch ff-must-fix-stack)

- Mboxes: A0/A kept from master (unchanged); B/C/E/D fresh
  `format-patch` of the proof stack. Sizes: A 620888, A0 35400,
  B 40394, C 653750, E 12693, D 129514. 2/1/2/11/2/6 patches.
- 24/24 plain `git am --whitespace=error` on a fresh pristine
  `3e6cd5e` clone, in DAG order (verify tree == proof tree by
  tree hash). `git show --check` clean all 24; 0 non-ASCII bytes.
- Per-step build: all 24 steps build (out-of-tree, local suite
  mirror). Registration steps (A 2/2, C 03/11, D 1/6) need a
  configure rerun (codec_list/parser_list regen) — standard.
- Boundary FATE (local 12/12 + pre-existing suite files): A0
  demux/demux-51/seek-extra PASS; A decode smoke 2 frames; B
  `make fate-ac4` 16/16 + encneg absent pre-C (no-rule, proven);
  C 23/23; E 25/25; D 28/28 + deterministic re-run.
- T2 goldens recorded from the proof stack via fate outputs
  (bitexact); T5 +4096 re-pin likewise. Full log: scratch
  `proof-gates.log` (seat-local; not committed).

## Cover map (M2)

All six covers carry the identical DAG chain + a per-cover stack
parent line (A0: none; A: A0; B: A0+A; C: B; E: C; D: C+E). Fixed
in the rewrite: A standalone/A0-head contradiction (A stacks on
A0), C "No B dependency", B's missing A0, D's missing E, A/C/D
provenance-comments overclaims (softened to scrub-pending truth),
A "byte-exact vs oracle" circularity, C false mutex sentence, C08
false "no FATE negative slot" premise, A0 samples-UNSENT staleness.
Dropped everywhere: draft branch-state scaffolding. Subjects
unchanged. COVER-B Testing block kept verbatim (send-docs grep it).
