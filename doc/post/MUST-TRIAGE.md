# MUST-TRIAGE — all PREPOST-REVIEW MUSTs vs current master bytes

Seat: ff-must-triage. Base: master `84b43e5` (first-hand; mbox bytes +
stacked `git am` on pristine `3e6cd5e` in scratch `stack-t*`).
Review base was `e72a4b9`; delta `e72a4b9..84b43e5` is DOCS-ONLY
(checklists, send-steps, SEND-SITTING, COVER-B 3 lines, B mbox 714
golden only) — so no code MUST can be STALE-by-code-fix; code items
are CONFIRMED or FALSE-POSITIVE only.

Count note: review says "23 MUSTs" but carries 27 distinct items
(M1–M22 + T1–T5). All 27 triaged below. No STALE found.

## Verdict rollup

HOLD ALL SIX STANDS. 26/27 CONFIRMED; 1 FALSE-POSITIVE (M3 suite
fact — but its clock residue is already the SEND-SITTING plan, and
every series stays held by other CONFIRMED items).

| MUST | Verdict | Series | Re-cut? |
|---|---|---|---|
| M1 stack collisions | CONFIRMED | all six | Y (one ordered stack) |
| M2 cover contradictions | CONFIRMED | all six covers | Y (one DAG) |
| M3 samples 0/12 | FALSE-POSITIVE | A0 cover line only | N (cover line rides A0) |
| M4 env knobs/dumps | CONFIRMED | A 1/2 | Y |
| M5 provenance claim | CONFIRMED | A 1/2 + COVER-A | Y |
| M6 dead options | CONFIRMED | A 2/2 + B 1/2 | Y |
| M7 flush unref | CONFIRMED | A 2/2 | Y |
| M8 no CRC | CONFIRMED | A 2/2 + B 2/2 | Y |
| M9 portability | CONFIRMED | A 1/2 | Y |
| M10 `*_tables` clean-delete | CONFIRMED | C 01/11 | Y |
| M11 whole-stream, no EXPERIM. | CONFIRMED | C | Y |
| M12 opaque cap unplumbed | CONFIRMED | C 03/11 | Y (1 line) |
| M13 sincos | CONFIRMED | C 02/11 | Y |
| M14 raw allocators | CONFIRMED | C 02/11 | Y |
| M15 TLS + false mutex claim | CONFIRMED | C 02/11 + COVER-C | Y |
| M16 version bump | CONFIRMED | C 03/11 | Y (1 line) |
| M17 C08 false premise | CONFIRMED | C 08/11 + B 2/2 | Y (fold+delete) |
| M18 second texi row | CONFIRMED | C 04/11 | Y |
| M19 Changelog policy | CONFIRMED | D 1/6+3/6 + E | Y |
| M20 multi-frame one sample | CONFIRMED | D 3/6 | Y |
| M21 three TOC walkers | CONFIRMED | A0 + D (+A later) | Y |
| M22 E02 false frame_size claim | CONFIRMED | E 2/2 | Y |
| T1 circular oracle goldens | CONFIRMED | A cover + anchor work | Y (cover min) |
| T2 no fractional-spf FATE | CONFIRMED | B 2/2 | Y |
| T3 no roundtrip FATE | CONFIRMED | C or D | Y |
| T4 ims120 pres workaround pin | CONFIRMED | A 2/2 + B 2/2 | Y |
| T5 negative-pts seek pin | CONFIRMED | A0 or B | Y |

## Stack blockers

### M1 CONFIRMED — five collision sites, all reproduced on master mboxes
- `libavcodec/Makefile`: C-after-A fails 01/11 (`Makefile:219`),
  A-after-C fails 1/2 (`Makefile:220`) — plain `git am`, rc!=0 both.
- `libavcodec/allcodecs.c`: A 2/2 (`series-A-decoder.mbox:15212`)
  and C 03/11 (`series-C-encoder.mbox:17042`) insert at the identical
  seam (`ff_ac3_fixed_decoder`/`ff_acelp_kelvin_decoder`, same hunk
  `@@ -430,6 +430,7 @@`, same blob `c344cb5`) — mutually exclusive.
- `tests/fate/ac4.mak`: B 2/2 (`series-B-fate.mbox:137`) and C 05/11
  (`series-C-encoder.mbox:17197`) both `new file`; different content,
  both define `fate-ac4:`.
- `tests/Makefile`: identical `+include $(SRC_PATH)/tests/fate/ac4.mak`
  in B (`series-B-fate.mbox:133`) and C (`series-C-encoder.mbox:17193`).
- `doc/general_contents.texi`: B-after-C fails 1/2 and C-after-B
  fails 04/11, both at `general_contents.texi:1307` — plain `git am`.
- Corroboration: C→E→D applies rc=0 (19 commits on t5); E-after-B
  fails 2/2 (`ac4.mak:50`); E 2/2 + D 4–6/6 are `--- a/` modifies
  chained on C's mak blob. SEND-SITTING's "contained" deviation covers
  only the E/D appends — A↔C and B↔C collisions stand regardless of
  landing order. Route: re-cut all six as ONE ordered stack (suggested
  A0→A→B→C→E→D), stacked-am + per-step-build gate. Re-cut: Y (all).

### M2 CONFIRMED — all five contradictions verbatim at master
- COVER-A L5-8: standalone, "no earlier series required", A0 "posted
  separately" vs COVER-A0 L2-6: "head patch", "Every later series
  assumes this behavior". Both present.
- COVER-C L4: "No B dependency; B deferred" vs M1 B↔C textual collision.
- COVER-B: "Posts after A lands", zero A0 mention — while B's own
  B-OWNER-SEND-STEPS preconditions require A0 landed and B-PREFLIGHT
  cites "A0-dependent failure shapes"; copy legs are pts-bearing
  framecrc (`series-B-fate.mbox:220-225`), NOPTS-without-A0 → bytes move.
- COVER-D L4-6: "Posts after Series C" only — while D 6/6 asserts E's
  bytes: "via the Series E descriptor path" + `0x7F/0x15` + registration
  CHECKs (`series-D-mp4.mbox:2922,3286-3297`), which only E 1/2 writes
  (`series-E-ts.mbox:39-67`). D declares no E.
- Covers unchanged since review base except COVER-B 3-line fate-source
  refresh — not STALE. Route: one DAG in all six covers, part of M1
  re-cut (B declares A0; D declares E). Re-cut: Y (covers).

### M3 FALSE-POSITIVE — suite fact wrong; clock residue already planned
- Claim "suite 0/12" cites B-POST-CHECKLIST (STAGED-pack history,
  `d731060`, "never post"). Current record B-PREFLIGHT-CHECKLIST P1:
  live rsync + `sha256sum -c EXPECTED.sha256` → 12/12 OK, suite live
  2026-10-16T22:23:09Z; SEND-SITTING L6 agrees ("Mail-1 already done").
- Suite landed ~3h BEFORE the review commit (review 2026-10-17T01:18Z;
  suite 2026-10-16T22:23Z) — wrong at write time, hence
  FALSE-POSITIVE rather than STALE.
- Residue: +24h clock ends 2026-10-17T22:23:09Z (still running at
  triage, 03:15Z); SEND-SITTING step 0 already gates the sitting on
  it, and B/A0 preconditions already require clock-end. No mbox change.
- Stale satellite: COVER-A0 L28-31 still says samples mail "UNSENT" —
  refresh that line on the A0 cover re-cut (rides M2). Re-cut: N.

## Series A

### M4 CONFIRMED — env-gated behavior + dumps + fprintf in libavcodec
- 97 `getenv` hits in `series-A-decoder.mbox` (AC4DEC_M4/SHF/KBD128/
  DRC_OUTREF/NOCOSCACHE/CMPHOTFILL/SPFILL/DRC…); `AC4DEC_PCMDUMP`
  (L4295, L8692), `AC4DEC_CODUMP` (L4313), `AC4DEC_ZDUMP` (L1066) with
  fopen append-dumps; 179 added-line `fprintf`; `#define fprintf(fp,
  ...) ac4d_logf2(...)` at L2998. Route: A 1/2 strip + av_log. Re-cut: Y.

### M5 CONFIRMED — "table/clause only" claim vs patch bytes
- COVER-A L63-64: "all comments are original prose referencing the
  standards by table/clause number only". Mbox contains `tmp/wave05/
  WEB-e0.md` (L1900), `tools/*.py` (L658, L1331, …), `spec/*.md`
  (L1897), `ANSWER-*` (L2308), `duet*` (L559, L1307), Lane/wave logs,
  and THREE `lavc-dec/README.md` refs (L90 + L2409 + L14387) for a
  file shipped NOWHERE in the stack (no diff/create for it).
  Route: A 1/2 scrub + correct COVER-A. Re-cut: Y.

### M6 CONFIRMED — dead DRC/DE option surface
- Registered (`series-A-decoder.mbox:15164-15184`, 6 opts) + documented
  in B 1/2 ("Reserved for future use", `series-B-fate.mbox:51-67`).
- `drc=1` or `de_level!=0` → `AC4D_UNSUPPORTED` (L7739-7750) →
  `AVERROR(EINVAL)` at init (L14516-14523); `drc_out_ref/drc_boost/
  drc_cut` accepted and stored but never acted on (drc_on stuck 0).
  Dead surface that can never be redefined later. Route: delete in
  A 2/2, shrink B 1/2 docs. Re-cut: Y.

### M7 CONFIRMED — flush unrefs decode.c's owned packet
- `s->pkt` borrows `avctx->internal->in_pkt` (L14442 comment, L15102
  assignment); `ac4_decode_flush` calls `av_packet_unref(s->pkt)`
  (L15120). Borrowing has precedent; unref'ing does not. Route: drop
  the unref in A 2/2. Re-cut: Y.

### M8 CONFIRMED — CRC code exists but is unreachable on the decode path
- Lib `parse_sync` verifies AC41 CRC (`crc16_ok`, L3112; reject at
  L3152) and `ac4d_feed` calls it (L8037) — BUT the wrapper re-frames
  every payload to AC40 (L14891-14930, `0xAC,0x40`), which has no CRC,
  because "the raw demuxer strips sync/size/CRC". Wire CRC discarded
  before the decoder sees it — exactly the reviewer's mechanism.
- B mak admits it verbatim: "bytes equal the stereo decode (wrap
  performs no CRC check: AC40 re-frame)" (L201-202) and "The same
  warning fires on the clean stereo clip (no CRC check)" (L208-210).
  A `crc-bad` test passing identically on clean input pins nothing.
  Route: verify CRC (or EXPERIMENTAL-disclose + rename test) in A 2/2
  + B 2/2. Re-cut: Y.

### M9 CONFIRMED — 7 raw allocs, unused unportable headers
- `#define _POSIX_C_SOURCE 200809L` (L2437) + `#include <signal.h>`
  (L2439) + `#include <unistd.h>` (L2444), zero API users of either
  header (mechanical grep); 7 raw calls: `calloc` L7713, `free` ×4
  L7727/7740/7747/7848, `malloc`+`free` L13119/13126 (imdct cos-cache).
  Route: drop includes/define, av_malloc, in A 1/2. Re-cut: Y.

## Series C

### M10 CONFIRMED — C01 lands `*_tables.*` that `make clean` deletes
- C 01/11 creates `ac4_huffman_tables.c/h` + `ac4_aspx_tables.c/h`
  (`series-C-encoder.mbox:24-32`); baseline `libavcodec/Makefile:1426`
  `CLEANFILES = *_tables.c *_tables.h …` + matching `.gitignore`
  glob both match — committed sources deleted by `make clean`.
  A's cover documents this exact rule and renamed to `*_tab`.
  Route: rename like A in C 01/11. Re-cut: Y.

### M11 CONFIRMED — whole-stream encoder, no EXPERIMENTAL, refuses defaults
- Zero `EXPERIMENTAL` in C (decoder carries the cap; encoder does not).
- Whole-input buffering + emit-at-flush after loudness pass (L16537,
  "feed all input, then" L12357); `want > INT_MAX/2` hard refusal
  (~1.07B-sample cap, L9241); C05 pins `-loudness_target 0` with
  "full-span defaults trip the by-design loud refusal on fate's own
  input" (L17159-17163 + mak L17207-17213). Route: EXPERIMENTAL +
  documented limits now, streaming on roadmap, in C. Re-cut: Y.

### M12 CONFIRMED — opaque cap claimed, helper never called
- `AV_CODEC_CAP_ENCODER_REORDERED_OPAQUE` claimed (L16950); zero
  `frame->opaque`/`ff_encode_reordered_opaque` calls in C (only
  unrelated DSI-struct `p->opaque` fields). Baseline `encode.c:915`
  gates COPY_OPAQUE on exactly this cap, and the copy helper
  (`encode.c:281-291`) must be called BY the encoder — in-tree
  precedent: libtheoraenc.c:377, mpegvideo_enc.c:2125,
  pngenc.c:1073. Claimed-but-unplumbed = silent opaque loss.
  Route: delete the cap (1 line) in C 03/11. Re-cut: Y.

### M13 CONFIRMED — unconditional sincos + private `__sincos`
- `port_sincos`: `extern __sincos` on Apple (L12815-12816), bare
  `sincos()` elsewhere (L12818), zero configure checks (mechanical
  grep). MSVC has no `sincos` → link break. Route: configure-check
  or sin()+cos() in C 02/11. Re-cut: Y.

### M14 CONFIRMED — 299 raw allocator calls, 8+ lavc files
- 299 added-line raw `malloc/calloc/realloc/free/strdup` vs 20 `av_*`
  in C; `ac4_enc_core_enc.c` alone: 40 `malloc` + 91 `free`.
  Route: mechanical av_malloc conversion in C 02/11. Re-cut: Y.

### M15 CONFIRMED — TLS with zero precedent + false cover claim
- `static _Thread_local char core_errmsg[256]` (L4260); zero
  `_Thread_local`/`__thread` in baseline lavc+lavu+lavf (mechanical).
- COVER-C L22-23: "Thread-safe (mutex in the lib layer)" — zero
  mutex hits in all of C (grep count 0). Both the code pattern and
  the cover sentence must go. Route: ctx-carried diagnostics in
  C 02/11 + fix COVER-C. Re-cut: Y.

### M16 CONFIRMED — unjustified minor bump, no APIchanges
- `LIBAVCODEC_VERSION_MINOR` 20→21 (L17062-17063); zero `APIchanges`
  hits in C; C03 message lists "lavc minor bump" with no API
  justification. A new internal codec is not a public API change.
  Route: drop the bump in C 03/11. Re-cut: Y.

### M17 CONFIRMED — C08 premise false inside the same submission
- C08 message verbatim: "FATE has no negative-test slot, so this
  committed script is the gate" (L17359) + `tests/ac4-neg-check.sh`
  (L17366-17368) — while B 2/2 implements negatives TWICE via
  `; test $$?` + `CMP = grep` (gap-trunc L214-215, probe-garbage
  L235-236). Route: fold both layout negatives into ac4.mak in B's
  idiom (B 2/2), delete the script (C 08/11). Re-cut: Y (both).

### M18 CONFIRMED — C04 adds a second `@item AC-4` row
- B 1/2: `@item AC-4 @tab _ @tab X` decode-only
  (`series-B-fate.mbox:85`); C 04/11: `@item AC-4 @tab IX @tab _`
  encode-only (`series-C-encoder.mbox:17146`) — same `@@ -1307`
  anchor (M1 collision), merged tree would show two AC-4 rows; C04
  message: "decoding stays empty — the decoder is a separate patch
  series" (L17078-17080). Route: rebase C docs onto B, single row,
  in C 04/11. Re-cut: Y.

## Series D

### M19 CONFIRMED — Changelog rule contradicts B/C in one submission
- D 1/6: "No Changelog hunk: maintainer-curated at release"
  (`series-D-mp4.mbox:30`); D 3/6: same (L1031-1032) — while B 1/2
  (`series-B-fate.mbox:16`) and C 03/11 (`series-C-encoder.mbox:16241`)
  INCLUDE Changelog hunks, and E omits silently (no hunk, no note).
  Route: one policy — add lines (parser, mov ac-4/dac4, TS
  descriptors) in D 1/6+3/6 + E, since B/C already do. Re-cut: Y.

### M20 CONFIRMED — muxer writes multi-frame packets verbatim, one sample
- Comment admits: "a multi-frame packet fails the gate and is written
  verbatim" (`series-D-mp4.mbox:1061-1062`); code writes
  `pkt->data + offset, size` as ONE sample (L1182-1184). Own D 3/6
  message claims "1 packet = 1 sample" while the gate it describes
  funnels multi-frame packets into exactly that violation; own parser
  (COMPLETE_FRAMES) then misparses the file. Route: split into
  multiple samples or fail loudly in D 3/6. Re-cut: Y.

### M21 CONFIRMED — three private TOC walkers + three rate tables
- dec_lib `toc_parse_bv2` (A L3712, called L8043) + `ac4_src_fps_*`
  tables (A L177-181); D parser own walk + own `ac4_spf[14]` (D
  L142-143); A0 demuxer own first-frame walk (A0 L127, L419, L663) +
  own fps rows (A0 L91-107). Guaranteed drift surface.
- Sharpest instance: A0 flags EVERY packet KEY — "Whole sync frames;
  no parser yet for per-frame keyflags" (message L23, code L765-767)
  — while the D parser lands in the SAME stack; A0 sets no
  `need_parsing` (zero hits in A0 mbox). D cover defers unification
  to "after Series A lands (third reader)" (COVER-D L44-48).
  Route: demuxer sets `need_parsing`, uses the parser; converge on
  ONE walker (A0 + D re-cut; A lib convergence follows). Re-cut: Y.

## Series E

### M22 CONFIRMED — E02 message falsehood about Series C
- E 2/2 message: "'[mpegts] frame size not set' noise is Series C's
  (the encoder never sets frame_size); fate runs at loglevel error"
  (`series-E-ts.mbox:93-94`). But C `ac4_encode_init` DOES:
  `avctx->frame_size = AC4ENC_FRAME_SAMPLES`
  (`series-C-encoder.mbox:16528`), `codec_par.c:179` copies it, and
  baseline `mpegtsenc.c` reads codecpar via
  `av_get_audio_frame_duration2` — the cited path cannot fire that way,
  so the `loglevel error` dodge is unproven. Route: correct the
  message, prove-or-drop the dodge in E 2/2 (+ Changelog line per
  M19). Re-cut: Y.

## Series B / test gaps

### T1 CONFIRMED — goldens pin the submitted implementation, "oracle" circular
- COVER-A L35-36: "byte-exact vs the dec-lib oracle … (11/11
  cmp-identical)" — while A 1/2 message says "In-tree port of the
  dec-lib snapshot … zero behavior edits" (A L6-9). Oracle IS the
  same code. Every golden in the stack is recorded from submitted
  bytes ("Goldens recorded from the real decoder", B 2/2 message).
  Route: anchor ≥1 clip externally (ETSI vectors / independent
  decoder diff) or downgrade the claim (A cover min). Re-cut: Y.

### T2 CONFIRMED — no fractional-spf FATE
- Zero `1601/1001/29.97/fractional` hits in `series-B-fate.mbox`
  (mechanical). N5's core rate fix (48000/1920 repin, the ONLY delta
  in the B re-cut-2) has zero fractional-row pins. Route: add 1001-row
  1601/1602 tests in B 2/2. Re-cut: Y.

### T3 CONFIRMED — no encoder→decoder roundtrip FATE
- No encode→decode test anywhere: C has none; D's roundtrips are
  carriage-level synth (mp4 mux/demux, ts demux — no decode step).
  Priming/skip + DSI end-to-end uncaught (encoder `initial_padding`
  vs decoder no-skip offset untested). Blocked on M1 (needs A+C
  stacked) — land in C or D after the stack re-cut. Re-cut: Y.

### T4 CONFIRMED — ims120 pins the pres-1 workaround, default pres fatal
- B 2/2 message admits: "ims120 pins presentation 1 (default pres
  hits a v1-fatal mid-stream config change in that clip)"
  (`series-B-fate.mbox:98`). A code: `AC4D_CONFIG_CHANGE` fatal +
  sticky (A L469, L8062-8064, L14361). Decoder aborts on real input;
  the test looks away. (Reviewer's "packet 17" granularity not
  re-verified; immaterial to the verdict.) Route: handle (drain+
  reopen) or EXPERIMENTAL-disclose loudly in A 2/2 + B 2/2. Re-cut: Y.

### T5 CONFIRMED — seek-eof golden pins negative pts, unexplained
- Ref: `0, -235904, -235904, 2048, 341, 0x27346f7e`
  (`series-B-fate.mbox:426`); mak: "past-EOF seek clamps to the last
  packet" (L228-230), no comment on the negative stamp. A0's own
  rebase-miss rule emits NOPTS — this is a large NEGATIVE pts, so
  either a rebase bug or in need of one honest comment. Route:
  explain or fix (A0 or B 2/2). Re-cut: Y.

## Method + satellite notes
- Every CONFIRMED above ties to master mbox/cover bytes (mbox:line) or
  a `git am` run on pristine `3e6cd5e` (M1 sites, C→E→D chain); the
  single FALSE-POSITIVE ties to dated records (suite-live 22:23:09Z
  pre-dates review 01:18Z). No item taken on the reviewer's word.
- Mbox sizes at master (all 24 patches): A 620888/2, A0 35400/1,
  B 19150/2, C 653799/11, E 11605/2, D 128977/6. Satellite wart:
  SEND-SITTING L24 expects B at 19113 (37 B stale vs the 714 re-cut
  19150) — refresh on next send-docs touch.
- Suggested parent order (unchanged from review §Re-cut checklist):
  M1+M2 stack+DAG first; build-break cluster (M10/M9/M13/M15);
  hygiene (M4/M14); scrub+policy (M5/M17/M19/M22); one-liners
  (M6/M7/M12/M16); behavior (M8/T4/T5/M20/M21); M11 limits;
  M3-clock/T1/T2/T3 at the end with A0/B posting last.
