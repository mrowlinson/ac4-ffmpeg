# PREPOST-REVIEW — hostile maintainer pass over the six AC-4 mboxes

Seat: ff-prepost-review. Base: ac4-ffmpeg master `e72a4b9`.
Reviewed bytes: `doc/post/series-{A-decoder,A0-head,B-fate,C-encoder,D-mp4,E-ts}.mbox`
(A=r5 guard-fix, B=final, C=r5, D/E/A0 packs) + `doc/covers/COVER-*.txt`.
FFmpeg baseline for all precedent checks: `3e6cd5e` (the stack's own base).
checkpatch.pl: ABSENT from this tree — "if avail" does not trigger. N/A, noted.

## Verdict: HOLD ALL SIX. Nothing posts as-is.

| Series | Patches | Verdict | Blocking count |
|---|---|---|---|
| A decoder (1/2+2/2) | 2 | HOLD | 6 MUST |
| A0 demux params | 1 | HOLD | samples gate + walker |
| B docs+FATE | 2 | HOLD | samples gate + 3 MUST |
| C encoder (01–11) | 11 | HOLD | 9 MUST |
| D parser+mp4 (1–6) | 6 | HOLD | 3 MUST + order |
| E ts mux (1/2+2/2) | 2 | HOLD | order + 2 MUST |

E patch 1/2 (mpegtsenc descriptors) is the only near-clean patch in the
stack. Everything else needs a re-cut, not a re-word.

## Stack blockers (M1–M3 — these alone sink the post)

### M1 [MUST] The six mboxes do not stack in ANY order. Proven by `git am`.
Each series was cut against pristine `3e6cd5e`, and five sites collide:

- `libavcodec/Makefile`: A 1/2 and C 01/03 anchor on the same
  AC3_MF/ACELP context — A-after-C and C-after-A BOTH reject (verified).
- `libavcodec/allcodecs.c`: A 2/2 and C 03/11 insert at the identical
  `ff_ac3_fixed_decoder` / `ff_acelp_kelvin_decoder` seam — mutually
  exclusive both directions (verified by inspection; same 3-line window).
- `tests/fate/ac4.mak`: B 2/2 and C 05/11 both CREATE it (`new file`,
  different content, both define `fate-ac4:`) — second poster always fails.
- `tests/Makefile`: B 2/2 and C 05/11 add the IDENTICAL include line.
- `doc/general_contents.texi`: B 1/2 and C 04/11 add `@item AC-4` rows at
  the same anchor — and C's row duplicates B's instead of extending it
  (two AC-4 rows post-merge: decode-only + encode-only).

Verified runs (pristine `3e6cd5e` clone, plain `git am`, scratch `stack/`):
A0+A apply; C-after-A fails at 01/11 (Makefile); C-on-pristine 11/11 OK;
A-after-C fails at 1/2 (Makefile); E-after-(A+B) fails at 2/2 (mak).
E 2/2 and D 4–6/6 ARE correctly cut against C's mak (chain C→E→D works).
Fix: re-cut the whole submission as ONE ordered stack, each patch against
its true parent (suggested A0→A→B→C→E→D, or justify another), and gate every
re-cut on stacked plain-`git am` + per-step build. No `--3way` rescue: the
list workflow is plain am.

### M2 [MUST] Covers tell mutually exclusive ordering stories.
- COVER-A: A posts standalone on pristine master, "no earlier series
  required"; A0 "posted separately".
- COVER-A0: A0 is "the head patch", "every later series assumes this
  behavior".
- COVER-C: "No B dependency; B deferred".
- COVER-B: "Posts after A lands" — but never mentions A0, while B's
  goldens REQUIRE A0 timestamps (pts-bearing framecrc/pcm refs; without
  A0 the same commands emit NOPTS → different bytes).
- COVER-D: "Posts after Series C" — but D 6/6 asserts E01's 0x7F/0x15
  descriptors ("via the Series E descriptor path") without declaring E.
Both A-first and A0-first cannot be the merit argument. Fix: one dependency
DAG, stated identically in every cover (B declares A0; D declares E; A/A0
head contradiction resolved by the M1 re-cut order).

### M3 [MUST] A0 + B FATE are unpostable: samples mail UNSENT, suite 0/12.
B-POST-CHECKLIST (master): "0/12 present, 0/12 sha-ok"; COVER-A0: "samples
mail still UNSENT". FATE_SAMPLES tests with missing suite files FAIL (they
do not skip), so posting A0/B now breaks `make fate` for everyone who
rsyncs. The covers already promise the +24h clock — honor it: post code
series first, A0/B at clock end. No mbox change, but the "six mboxes post
together" plan is dead.

## Series A — decoder (MUST)

### M4 [MUST] ~20 getenv behavior knobs + stderr/file dumps in libavcodec.
`AC4DEC_M4`, `AC4DEC_SHF`, `AC4DEC_SHFN`, `AC4DEC_KBD128/1024/2048`,
`AC4DEC_DRC_OUTREF`, `AC4DEC_NOCOSCACHE`, `AC4DEC_CMPHOTFILL`,
`AC4DEC_SPFILL`, … change decode behavior from the environment; `fopen`
append-dumps (`AC4DEC_PCMDUMP/CODUMP/ZDUMP`), ~178 `fprintf` calls
(acpl.c writes stderr directly). Library code must not read the
environment for behavior or touch stderr/files. Fix: strip every
env-gated behavior change; route genuine diagnostics via `av_log`;
delete dump paths. The `#define fprintf(fp, ...) ac4d_logf2(...)` in
dec_lib.c goes regardless — redefining libc identifiers is never OK.

### M5 [MUST] Provenance claim in commit 1/2 + COVER-A is false on its face.
"All comments are original prose referencing the standards by
table/clause number only" — the patch contains `tmp/wave05/WEB-*.md`,
`tools/*.py`, `spec/*.md`, `tests/CLI.md`, `ANSWER-*`, `JOBS#32`,
"owner D3a", "Lane e2, wave5", "duet26", wave/Q-number logs, and THREE
references to `lavc-dec/README.md` (Makefile stanza, ac4dec.c header ×2)
— a file shipped NOWHERE in the stack. Fix: scrub dangling/internal
pointers (or correct the claim to what is actually true).

### M6 [MUST] AVOptions that can never work: drc/drc_out_ref/drc_boost/
drc_cut/de_level always EINVAL at init, yet are registered + documented
("Reserved for future use"). Dead option surface can never be redefined
later. Fix: delete until implemented (B docs shrink accordingly).

### M7 [MUST] `av_packet_unref(s->pkt)` in `ac4_decode_flush` (ac4dec.c).
Borrowing `avctx->internal->in_pkt` has in-tree precedent (av1dec.c:902)
— unref'ing decode.c's owned packet in flush does not. Fix: drop the unref.

### M8 [MUST] No CRC check anywhere on the decode path; B pins acceptance.
AC41 frames are re-framed to AC40, discarding the wire CRC; B's
`fate-ac4-crc-bad` golden is BYTE-EQUAL to the clean stereo decode and the
mak comment admits "the same warning fires on the clean stereo clip (no
CRC check)". A test named "crc-bad" that cannot distinguish corrupt from
clean is worse than no test. Fix: verify the CRC and skip/reject loudly —
or EXPERIMENTAL-disclose + rename the test to what it actually pins.

### M9 [MUST] Windows/portability breaks in dec_lib.c/imdct.c.
`#include <signal.h>` + `#include <unistd.h>` with ZERO call sites
(unused — and `unistd.h` does not exist on MSVC); `#define
_POSIX_C_SOURCE 200809L` mid-TU (useless-to-harmful next to config.h
feature macros); 7 raw malloc/calloc/free calls (imdct cos-cache +
`c = calloc(...)` ×dec_lib). Fix: drop the includes/define, use av_malloc.

### NITs A (post anyway)
- Process-wide `AVMutex` around every lib call (thread-unsafe lib papered
  over): disclosed + EXPERIMENTAL → acceptable v1, must die before
  de-experimental. Same for `delay=0`/no `initial_padding` (disclosed).
- `ac4d_gap_note()` result used AFTER `ff_mutex_unlock` (ac4dec.c
  `ac4_queue_push`, `ac4_lib_fatal`): copy under the lock.
- `CODEC_CH_LAYOUTS` advertises 4 layouts but the latch accepts any lib
  mask — align list with reality.
- `frame->pkt_dts = pts`; DR1 claimed but every emit memcpy's; leading-
  underscore file-scope `_ac4d_self` (reserved identifier — rename);
  27 over-80 code lines in dec_lib.c/entropy.c/acpl.c/ac4_src.h (wrap;
  long TABLE lines tolerated); patcheck `for(;;)`/`x==0`/av_cold hits
  all triaged benign.

## Series C — encoder (MUST)

### M10 [MUST] `make clean` DELETES C's committed table sources.
C 01/11 lands `ac4_huffman_tables.c/h` + `ac4_aspx_tables.c/h`, which
match libavcodec/Makefile:1426 `CLEANFILES = *_tables.c *_tables.h …`
(verified) and `.gitignore`. A's cover documents this exact rule and
renamed to `*_tab` — C violates it. Fix: rename to `*_tab.*` like A.

### M11 [MUST] Whole-stream buffering encoder ships WITHOUT EXPERIMENTAL.
Buffers the ENTIRE input as int rows (+ a double pass-1 row): ~70 MB for
a 3-minute song, hard `INT_MAX` refusal past ~1.07B samples, encode happens
only at flush. The conservative decoder carries `AV_CODEC_CAP_EXPERIMENTAL`
and this does not. Default settings refuse fate's own full-span asynth
input (C05 pins the `-loudness_target 0` dodge, calling it "by-design").
Fix: minimum = EXPERIMENTAL + documented limits; real fix = streaming.
(Unbounded-RAM + refuse-by-default is release-blocking either way.)

### M12 [MUST] `AV_CODEC_CAP_ENCODER_REORDERED_OPAQUE` claimed, never honored.
ac4enc.c never reads `frame->opaque*` nor writes packet opaque — with
`AV_CODEC_FLAG_COPY_OPAQUE` (gated on exactly this cap, encode.c:915),
packets silently lose opaque data. Fix: delete the cap (1 line) until
plumbed.

### M13 [MUST] Unconditional `sincos()` + `extern __sincos` hack.
`port_sincos` calls glibc/BSD `sincos` on non-Apple with NO configure
check (MSVC has no `sincos` → Windows link break) and declares Apple's
PRIVATE `__sincos` symbol on Apple. Fix: configure-check or plain
sin()+cos().

### M14 [MUST] ~300 raw malloc/free/calloc/realloc/strdup in 8 lavc files
(ac4_enc_core_enc.c alone: 40 malloc + most of 200 free). libavcodec
requires the av_malloc family (custom allocators, leak checks).
Fix: mechanical conversion (wrappers already comply; it's the core).

### M15 [MUST] `_Thread_local` diagnostics — zero in-tree precedent.
No `_Thread_local`/`__thread` anywhere in baseline lavc/lavu/lavf;
pre-19.28 MSVC risk; novel pattern for what should ride in ctx. Fix:
carry diagnostics in the context. — Plus COVER-C's "Thread-safe (mutex
in the lib layer)" is FALSE: zero mutex hits in C 02/11. Fix the cover.

### M16 [MUST] Unjustified `LIBAVCODEC_VERSION_MINOR` 20→21 bump.
A new internal codec is not a public API change — C's own message
("No APIchanges entry" appears for B; C has no APIchanges entry either)
concedes it. Fix: drop the bump.

### M17 [MUST] C 08/11's premise is false inside this very submission.
"FATE has no negative-test slot, so this committed script is the gate" —
but B 2/2 implements negatives TWICE via `; test $$?` + `CMP = grep`
(gap-trunc, probe-garbage). Fix: fold the two layout negatives into
ac4.mak in B's idiom; delete `tests/ac4-neg-check.sh`.

### M18 [MUST] C 04/11 adds a SECOND `@item AC-4` general-contents row.
Beyond the M1 same-anchor collision: merged content would show two AC-4
rows (decode-only + encode-only), and C's message brags about it
("decoding stays empty — the decoder is a separate patch series").
Fix: rebase C docs onto B — single `@tab IX @tab IX` row.

### NITs C (post anyway)
- ENCODE_CB vs receive_packet (cover asks D1 openly — asking reviewers is
  legitimate; convert iff maintainer insists; same for H1 loudness-manual).
- `avctx->ch_layout.u.mask` read without checking `order == NATIVE`
  (C 11/11 — gate probably saves it; assert it).
- Unexplained `probe[48]` / `ac4_enc_core_iframe_set(48, …)` magic.
- Internal IDs in comments/messages: m1/m2/m4–m9, S5FU/R3/EL/FUZZ-CENC,
  "DSI-12", "DSI differential suite", parent branch names
  (`ff/dsi-lavcshape-merged`), "oracle"/"DEE"/"reference encoder"/
  `dolby-ac4-cleanroom encoder/enc.py` pointers — scrub to in-tree truth.
- `ac4_pycompat.*`: load-bearing (sum/round/int/float all called) but the
  NAME + "CPython 3.14 / Unicode 16.0" pins + dangling
  `tests/test_pycompat.py` pointer leak process — rename neutral.
  `strtod` locale dependence deserves one line.
- Encoder `initial_padding=4096` vs decoder no-skip: roundtrips grow a
  4096-sample leading offset; no roundtrip test exists (see T3).
- `-c ac4` vs `-c:a ac4` inconsistency; long .mak lines.

## Series D — parser + MP4 (MUST)

### M19 [MUST] Changelog policy contradicts B/C in the same submission.
D 1/6 + D 3/6: "No Changelog hunk: maintainer-curated at release" — while
B 1/2 and C 03/11 INCLUDE Changelog hunks (E omits silently). Both cannot
be the rule. Fix: one policy; recommended = add lines (parser, mov
ac-4/dac4, TS descriptors) since B/C already do.

### M20 [MUST] movenc writes multi-frame packets verbatim as ONE sample.
`mov_ac4_raw_offset` comment admits it; E.3 requires one frame per sample,
and the project's own parser (COMPLETE_FRAMES) then misparses the file.
Silently writing spec-invalid MP4. Fix: split into multiple samples or
fail loudly.

### M21 [MUST] THREE private TOC walkers + private rate tables.
dec_lib.c, ac4_parser.c, and lavf/ac4dec.c each walk the TOC with own
helpers and own fps/spf tables — guaranteed drift. Sharpest instance:
A0's demuxer duplicates exactly what the D parser exists to do (params
AND keyflags), flags EVERY packet KEY with "no parser yet", while D
lands in the same stack. D's cover defers unification to "after Series A
lands (third reader)" — incoherent members and timing. Fix: demuxer sets
`need_parsing` and uses the parser; converge on ONE walker (parser or
shared header).

### NITs D (post anyway)
- In-muxer strip vs BSF: expect the question, but ff_mov_write_packet
  already converts (av1/vvc/nal) in-tree — local precedent is real; cite it.
- dby1 brand omission: cite the E.4 clause or write the brand.
- Parser guesses 6ch→5.1 / 8ch→7.1 and row-15→24ch "tentative": consider
  UNSPEC + document; bit-by-bit CRC is resync-hot (table-drive it).
- "DSI-CONTRACT 5"/"dashif shape" test-comment jargon; D cover's
  branch-state/review-ID scaffolding ("P4 DEFER-OK", "REVIEW-DMUX M1").
- mov slogan "grid-rate display wart pre-decode" disclosed ✓; exotic TOC
  legs "0 wire vectors" disclosed ✓.

## Series E — TS mux (MUST)

### M22 [MUST] E 2/2 message states a falsehood about Series C.
"(the encoder never sets frame_size)" — C's `ac4_encode_init` DOES
(`avctx->frame_size = AC4ENC_FRAME_SAMPLES`), and
`avcodec_parameters_from_context` copies it (codec_par.c:179), so the
cited `[mpegts] frame size not set` path (mpegtsenc.c:1057–1059, reads
codecpar) cannot fire that way. The `loglevel error` workaround it
justifies is unproven. Fix: correct the message; prove-or-drop the dodge.
- E also needs a Changelog line (M19) and inherits M1/M2 (order).

### NITs E (post anyway)
- E 1/2 itself: clean. Nested-ternary channel_mode; unknown-layout→stereo
  guess is documented ✓; macros/demux-side all verified pre-existing
  (mpegts.h:227/232, mpegts.c:899/2376); new bounds check exceeds neighbor
  idiom (good). ATSC signaling undescribed (SYSTEM_B-only v1 — say so).
- A "frame size not set" hunt that ends in a stale claim is exactly why
  E 2/2's noise paragraph must go.

## Series A0 — demux params
- Blocked by M1/M2 (order) + M3 (samples: fate-ac4-demux/-51/seek-extra
  need suite clips) + M21 (third TOC walker; all-KEY vs D parser).
- Code itself is careful (bounded peek, NOPTS-on-rebase-miss, bail-to-prior
  behavior). NITs: direct `ch_layout` struct assignment (use the copy
  helper); goto-spaghetti readability; cover's "Branch state (draft,
  unposted)" + SUITE-REQUIRED admissions must be resolved-or-removed at
  post; patcheck "missing } prior to else" verified false positive.

## Series B — docs + FATE (MUST beyond M1–M3)
- B 1/2: correct placement/anchors; shrinks with M6 (dead options).
- T1 [MUST] Every golden in the stack is recorded from the submitted
  implementation. The "oracle" (cover A: "byte-exact vs the dec-lib
  oracle") is the SAME code ("in-tree port … zero behavior edits") —
  circular. Decoder goldens pin current behavior, nothing more. Fix:
  anchor at least one clip externally (ETSI conformance vectors or an
  independent decoder diff) or downgrade the claim to what it is.
- T4 [MUST] `fate-ac4-ims120` pins the pres-1 WORKAROUND because default
  pres hits a v1-FATAL mid-stream config change at packet 17 of a real
  clip. The decoder aborts on real input and the test looks away. Fix:
  handle (drain+reopen) or EXPERIMENTAL-disclose loudly; do not enshrine.
- T5 [MUST] `ac4-seek-eof` golden pins pts −235904. Negative timestamps
  on a seek-clamp smell like an enshrined rebase bug. Fix: explain or fix.

## Test gaps a maintainer would demand (all MUST)
- T1 independent-output anchor (above). T2 fractional-spf (1001-row
  1601/1602) FATE — N5's core fix has zero pins. T3 encoder→decoder
  roundtrip FATE (catches priming/skip + DSI end-to-end; needs A+C —
  land in C or D). T4 ims120 default-pres fatal. T5 negative-pts seek.
- Disclosed-and-acceptable: mono/float/1536-spf (no bytes exist); A0
  fri-row positives (SUITE-REQUIRED); E framecrc stage (post-D follow-up);
  D exotic legs (0 vectors). Disclosure is done right in these cases.

## Cover-claims vs reality (all verified against patch bytes)
| Claim | Verdict |
|---|---|
| A: standalone on pristine master / A0: everything assumes A0 | CONTRADICT each other (M2) |
| A/C: comments reference "standards by table/clause only" | FALSE — tmp/tools/ANSWER/JOBS/owner/lavc-dec pointers (M5) |
| A: "patcheck clean-under-triage" | WEASEL — hits are real (M4/M9/M14) |
| A: "byte-exact vs oracle" (11/11) | CIRCULAR — oracle is the same code (T1) |
| A0: "patcheck clean" | TRUE (one FP) |
| B: "pre-patch negative" | TRUE, good practice |
| C: "Thread-safe (mutex in lib layer)" | FALSE — no mutex exists (M15) |
| C: "MSVC-safe probe" | UNPROVEN — sincos/_Thread_local/unistd break MSVC (M9/M13/M15) |
| C: every patch builds standalone | TRUE (verified C 11/11 on pristine) |
| C08: "FATE has no negative slot" | FALSE — B does it twice (M17) |
| D: mov/ac-4 tags "already exist" | TRUE (isom_tags.c:312, movaudio) |
| D: "MP4 samples one frame per sample" | VIOLATED by own mux on multi-frame pkts (M20) |
| E02: "encoder never sets frame_size" | FALSE — it does (M22) |
| D/E: "maintainer-curated Changelog" | CONTRADICTS B/C's own hunks (M19) |
| C10: "oracle-verified bytes" | UNVERIFIABLE out-of-tree (NIT: reword) |

## Style appendix (what was actually run)
- `tools/patcheck` on all six mboxes (this tree's copy): A 882 / C 747 /
  D 188 / E 48 / A0 26 / B 24 log lines, triaged above. Real signal fully
  captured in M4/M9/M14 (+NITs); the rest is idiom (`for(;;)`), preference
  (`x==0`), false positives (verified), or test/shell scope (D printf in
  api tools, E awk sprintf, Makefile recipe tabs).
- Added-line scan: 0 trailing-whitespace / 0 stray tabs in all six ✓.
  Over-80: A 348 / C 182 — all table data except 27 A code lines (NIT);
  D/E/A0/B overs are .mak REF/hash lines (conventional).
- `git apply --check --whitespace=error`: fails only for missing base
  files outside a tree (expected); the stacked `git am` runs in M1 are the
  operative check, and they fail on collisions, not whitespace.
- `tools/checkpatch.pl`: does not exist in this tree. N/A.

## Method
Cold read of all 24 patches' full bytes; patcheck + whitespace scans;
precedent checks against baseline `3e6cd5e` (tags, macros, demux keys,
CLEANFILES:1426, codec_par.c:179, encode.c:915, in_pkt users, allocator
policy); empirical stacked-`git am` both orders in scratch `stack/`.
Findings above are all tied to patch bytes or baseline lines, not vibes.

## Re-cut checklist (parent order)
1. M1+M2: single ordered stack + one DAG in all covers; stacked-am gate.
2. M10+M9+M13+M15: `*_tab` rename; drop unistd/signal/POSIX; kill sincos
   and TLS (or compat-proof both) — the build-break cluster.
3. M4+M14: env/dump strip + av_malloc conversion (the hygiene cluster).
4. M5+M17+M19+M22: message/cover/jargon scrub + Changelog policy + E02 fix.
5. M6+M7+M12+M16: dead options, flush unref, opaque cap, version bump
   (one-line-ish each).
6. M8+T4+T5+M20+M21: CRC, ims-default, seek-eof, multi-frame, one walker.
7. M11: EXPERIMENTAL + limits (now) / streaming (roadmap).
8. M3+T1+T2+T3: samples clock; external anchor; fractional-spf + roundtrip
   FATE. Then A0/B post at clock end.
