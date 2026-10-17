# A-PREFLIGHT-CHECKLIST — Series A decoder FINAL re-cut 6 (owner posts)

Branch: `ff/doc-must-fix-A` from master `6905822`.
Gated 2026-10-17 in scratch `ff-must-fix-A-retry` (TMPDIR there);
parent repo untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure: `doc/post/A-OWNER-SEND-STEPS.md`.

## Why re-cut 6 (MUST fixes M4-M9, all in this series)

- [x] M4 env-gated behavior + dumps + fprintf in libavcodec: all 97
  `getenv` sites, 179 `fprintf` sites, and every `fopen` dump
  deleted from `lavc-dec/core` (behavior knobs pinned to defaults:
  M4 rolloff on, experimental curves off, KBD table alphas, cached
  IMDCT kernels, hot-gap-fill on); the `fprintf`-to-log shim and
  the `log`/`log_u` callback deleted with them. Zero `getenv` /
  `fprintf` / `fopen` / `AC4DEC_` remain (the `AC4DEC_PARAM` option
  macro renamed `AC4_PARAM` to match).
- [x] M5 provenance-hostile comments: full scrub of the named
  classes (`duet*`, `tools/*.py`, `spec/*.md`, `ANSWER-*`,
  `tmp/wave05/WEB-e*`, memos, Lane/wave logs, `census.c`, ODK) to
  table/clause citations; all three `lavc-dec/README.md` pointers
  removed (2 reworded, 1 stripped with the dec_lib header);
  machine-generated table headers cite ETSI deliverables, not
  internal scripts. `ac4_stages.h` deleted (unreferenced dead
  header, pure process residue). Zero named-class hits remain.
- [x] M6 dead DRC/DE surface: the 5 options (`drc`, `drc_out_ref`,
  `drc_boost`, `drc_cut`, `de_level`) + struct fields + opt
  plumbing + the `UNSUPPORTED` fail-open branch deleted from
  `ac4dec.c`; `pres_sel` only. Lib keeps its `UNSUPPORTED`
  fail-open (unreachable: the wrapper passes zeros).
- [x] M7 flush unref: `av_packet_unref(s->pkt)` dropped from
  `ac4_decode_flush` (borrowed `in_pkt`, decode.c-owned) with an
  explanatory note; the get-packet consume/unref sites stand.
- [x] M8 unreachable CRC: kept the AC40 re-frame (the demuxer
  strips CRC; the layer cannot recover dropped bytes) and
  disclosed loudly in code (header + feed-site comment), the 2/2
  message, and this checklist. B-side test rename is the B seat's.
- [x] M9 portability: `_POSIX_C_SOURCE` + `<signal.h>` +
  `<unistd.h>` dropped (zero API users), 7 raw allocator calls →
  `av_calloc`/`av_malloc`/`av_free` (core now includes
  `libavutil/mem.h`, in-tree-only by design).
- [x] `COVER-A.txt`: scrub line past-tense ("folded ... in this
  re-cut"); generator-name clause dropped (headers cite ETSI
  sources now). DAG + `pres_sel`-only blurbs (stack seat) are now
  TRUE of these bytes. Subjects/prose verified exact below.

## Re-cut 6 assembly (this wave)

- [x] Staging `b2df32f` (1/2, 24 files: Makefile + 23 core,
  `ac4_stages.h` gone) + `1c1b2b0` (2/2, `ac4dec.c` +
  `allcodecs.c`) on pristine `3e6cd5e`: `wiring.patch`
  split-applied clean (Makefile hunk → 1/2, allcodecs hunk → 2/2;
  Makefile comment reworded, same 2 lines/position), 23 core +
  wrapper copied from this branch staging.
- [x] 1/2 + 2/2 messages new for r6 (strip + pinned defaults +
  allocator conversion + scrub + stages-drop in 1/2; CRC
  disclosure + `pres_sel`-only in 2/2).
- [x] Mbox `series-A-decoder.mbox` 507373 B / 12844 lines (was
  620888/15226; −113515 B of probes/dumps/scrub).

## Final gates (this wave, all green)

- [x] `git am` clean on pristine `3e6cd5e` — rc=0, commits `42ff262`
  (1/2) + `d086749` (2/2). ZERO warnings (`am.log` = 2 `Applying:`
  lines only). `git show --check`: 0/0. Am trees `b7f3d49` /
  `10ac64b` EXACT-match the staging commits' trees. The shipped
  `From ` lines name the staging commits `b2df32f` / `1c1b2b0`.
- [x] Re-cut identity — all 24 new files `cmp`-identical am-tree vs
  this-branch staging; am-tree `format-patch` round-trips
  byte-identical (same 507373 B; `From `-stripped content
  `cmp`-identical).
- [x] `fate-source` GREEN — `tests/fate/source-check.sh` output on
  the am-tree is byte-identical to `tests/ref/fate/source`
  (zero ours listed).
- [x] Subjects 1:1 cover↔mbox — each of
  `[PATCH 1/2] avcodec: add native AC-4 decoder core` and
  `[PATCH 2/2] avcodec: wire ff_ac4_decoder` appears exactly once
  in `COVER-A.txt` and once in the mbox.
- [x] Zero placeholders — zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in cover + mbox (mechanical grep).
- [x] Signoffs — `Michael Rowlinson <mrowlinson@gmail.com>` on both
  patches (mbox `From:` lines 2, 11930; `From ` lines 1, 11929;
  signoffs lines 32, 11964).
- [x] ASCII — mbox ZERO non-ASCII lines; cover ZERO non-ASCII. No
  MIME headers → `git send-email` expects zero 8bit warnings.
- [x] Honest-gap note — single `TBD` is mbox line 4820, the in-code
  3ch coupling note, as disclosed (NOT a send placeholder).
- [x] `_tables` — 2 hits, both in the 1/2 message (rename
  rationale); zero `*_tables` filenames, zero in-code `src/*`
  hits (scrubbed).
- [x] MUST-byte audit — mbox grep: 0 `getenv`/`fprintf`/`fopen`/
  `AC4DEC_`; 0 `duet`/`tools/`/`spec/`/`ANSWER-`/`WEB-e`/`tmp/wave`;
  0 wrapper `drc` options (lib `drc_on`/`de_level` fields +
  fail-open kept deliberately); 0 `signal.h`/`unistd.h`/
  `_POSIX_C_SOURCE`; 3 CRC-never-checked disclosure hits.
- [x] Full build — out-of-tree `configure --disable-doc` rc=0,
  `make ffmpeg` rc=0; 34 warning-lines, 0 in our files — same
  shape as re-cut 5; decoder row
  `A..X.D ac4 ATSC A/344 Dolby AC-4`; buildconf shows bare
  `--disable-doc`, no extras; no ac4 encoder row.
- [x] Smoke 11/11 POST bytes — exact `gate11.sh` recipe
  (`-strict experimental -v info -i vec -map 0:a -f s16le`), every
  `vec/*.ac4` rc=0 `cmp`-identical to BOTH the N5-current shim
  (`dec-lib` @ this branch, built fresh here) and the independent
  streaming driver (all `ff-vs-shim=SAME`, `ff-vs-sprobe=SAME`),
  AND to the re-cut-5 bytes (`ff-vs-r5=SAME` all 11:
  unchanged-bytes proof).
- [x] N5 pins live — ff gpac log shows 48000 Hz / 1602 spf max /
  6ch; gpac PCM 5727312 B, sha256 `8b41f7cf…39f6`, input sha
  `b82fabba…70ce1`: all EXACT-match `fate-n5/pins.txt`.
- [x] F2 gate (`lavc-dec/tests/f2-gap-trunc.sh` vs fresh binary):
  `== F2 GATE PASS ==` (rc=69 no-sync loud 0 B; stereo control
  rc=0).
- [x] tseek C-API suite (re-proven, not carried): T1 straight,
  T2 flush+re-A IDENTICAL, T3 flush+re-B IDENTICAL (2ch→6ch
  relatch), T4 req-flt fails, T5 stuck `pres_sel=15` fatal,
  T6 virgin-drain clean EOF. `TSEEK: ALL PASS`.
- [x] M6 surface probe — `decoder=ac4` lists `-pres_sel` only;
  `-drc 1` → `Unrecognized option 'drc'` / `Option not found`.
- [x] `make clean` survival (separate in-tree clone): 0 tracked
  dirty, 0 deletions; 24/24 new files survive (13 `*_tab.*` on
  disk = 8 ours + 5 pre-existing).
- [x] patcheck — 455 findings (was 738: fewer lines), NO new
  class vs re-cut 5; three classes eliminated (`Please use
  av_log`, `Mergeable calls`, `Please use av_assert0...`).
  Clean-under-triage stands, improved.
- [x] Send-shape — `mkdir -p send && git mailsplit -osend` →
  0001+0002 with correct subjects; `COVER-A.txt` has Subject +
  blank + body 0000 shape.
- [x] Stack span — A0 + A(r6) + B `git am` rc=0, 5/5 clean on
  pristine `3e6cd5e` (C01's 2-line Makefile context names r5's
  comment; refresh is C-seat-owned with the M10 re-cut — see
  disclosed carries).
- [x] Nothing sent yet — no SENT marker in repo or mail-combined;
  Series A and mail-1 both unsent at gate time.

## Carried gates (stand via the ff-vs-r5 SAME leg)

Am-tree PCM bytes are identical to the re-cut-5 bytes the
`ff-A-guard-fix`/`ff-A-r5-verify` seats validated (11/11
`ff-vs-r5=SAME` above, plus shim+sprobe legs), so these stand
without re-run: dec-lib unit PASS; fate-n5 14/14; ngis-gate +
fuzzfix-gate PASS (pins, repros, grinds); wrap-level repro75 +
corner100 rc=0. Neg controls (splice fatal, garbage cap, no-bed,
empty input) stand on the same identity; the `drc` init refusal is
SUPERSEDED by the M6 surface probe above (options deleted) and
the FLT refusal is re-proven (tseek T4). ASan/UBSan 6/6 stands
via identity (allocator swap is `av_*`-for-`*` with no size
change). Patch-1 standalone inert carried by mechanism-identity:
the Makefile hunk delta r5→r6 is comment-only and the guard stays
undefined without 2/2.

## Cover delta (this re-cut: scrub line only)

- `COVER-A.txt`: scrub parenthetical → past tense; generator
  clause → ETSI-source clause. DAG + `pres_sel`-only + Testing +
  FATE + N5-note + Provenance + Signoff all exact against these
  bytes (verified: subjects 1:1, options surface, N5 pins).

## Disclosed carries (reviewer-visible, non-blocking)

- `dec_lib.c` 3ch comment `coupling apply TBD` (mbox line 4820):
  in-code honest-gap note (parse-only for alignment), NOT a send
  placeholder; behavioral gates pass with it.
- This series pins no goldens; Series B FATE goldens are recorded
  against the fixed N5 labels (cover says so).
- C 01/11's Makefile context quotes r5's 2-line decoder comment;
  the C seat refreshes those 2 context lines with the M10 re-cut
  (STACK-DAG assigns C's A-context to the C seat; hunk positions
  preserved here). E/D stack behind C, unaffected otherwise.
- T4 drain+reopen stays future work (code + cover disclose the
  v1-fatal; README tracked follow-ups); out of the M4-M9 brief.
- A-xx/Q-x/Dx finding IDs remain in a few comments (out of the
  M5 named scope; follow-up if reviewers flag them).

## Owner-at-post (fill when sending)

- [ ] A0 merged first (merged stack DAG: A0 → A → B → C → E → D).
- [ ] Sent date + Message-IDs (0/2, 1/2, 2/2): _______________
- [ ] Archive shows 0/2 + 1/2 + 2/2 threaded in order.
- [ ] Mail-1 (separate, `samples-request@ffmpeg.org` + 15
  attachments) sent date: _______________ (starts no clock for A;
  +24 h after suite-live gates A0/B2).

## Verdict

**READY-TO-POST** — Series A decoder send pack final (re-cut 6).
