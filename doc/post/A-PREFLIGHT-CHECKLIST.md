# A-PREFLIGHT-CHECKLIST — Series A decoder FINAL re-cut 5 (owner posts)

Branch: `ff/doc-A-guard-fix` from master `ef93aa9`.
Gated 2026-10-16 in scratch `ff-A-guard-fix` (TMPDIR there);
parent repo untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure: `doc/post/A-OWNER-SEND-STEPS.md`.

## Why re-cut 5 (drift since re-cut 4)

- [x] B-recut escalation 1: `fate-source` fails on the Series-A
  headers — the 12 pre-N5 `AC4_*` inclusion guards do not match the
  `AVCODEC_<file>_H` form `tests/fate/source-check.sh` derives from
  the in-tree path (plus `ac4_src.h`, new in re-cut 4: 13 failing
  headers on the re-cut-4 bytes). Upstream CI would fail on
  Series A → re-cut REQUIRED. Proven this wave: the check run on
  the re-cut-4 tree lists all 13 (`fatesource-prefix.log`).
- [x] Fix: all 13 `lavc-dec/core/*.h` guards remapped to
  `AVCODEC_<file>_H` (`#endif` carries the `/* MACRO */` comment,
  `ac3.h`/`aactab.h` precedent); `lavc-dec/README.md` port-delta +
  re-vendor rule updated. `dec-lib/` keeps its standalone `AC4_*`
  guards (correct out-of-tree; never enters the ffmpeg tree).
  Zero behavior delta: r5 staged tree vs r4 = guard triples only
  (13 headers x 6 diff-lines; all other files `cmp`-identical).
- [x] `COVER-A.txt` unchanged: subjects and prose still exact
  against this core (no cover delta this re-cut).

## Re-cut 5 assembly (this wave)

- [x] Staging `1ef339d` (1/2, 25 files: Makefile + 24 core) +
  `8875e17` (2/2, `ac4dec.c` + `allcodecs.c`) on pristine `3e6cd5e`:
  `wiring.patch` split-applied clean (Makefile hunk → 1/2, allcodecs
  hunk → 2/2), 24 core + wrapper copied from this branch staging.
- [x] 1/2 + 2/2 messages verbatim from re-cut 4 (bodies
  byte-identical; port text still accurate — the guard form is an
  in-tree-port detail, zero behavior edits).
- [x] Mbox `series-A-decoder.mbox` 620888 B / 15226 lines (was 620505).
  Delta vs re-cut 4 = guard triples only (+383 B from the longer
  `AVCODEC_` names + `#endif` comments).

## Final gates (this wave, all green)

- [x] `git am` clean on pristine `3e6cd5e` — rc=0, commits `038cb8f`
  (1/2) + `43a0562` (2/2). ZERO warnings (`am.log` = 2 `Applying:`
  lines only). `git show --check`: 0/0. Whole-tree hashes identical
  to the staging commits' trees.
- [x] Re-cut identity — all 25 new files `cmp`-identical am-tree vs
  this-branch staging; am-tree `format-patch` round-trips
  byte-identical (same 620888 B; normalized content identical).
- [x] `fate-source` GREEN — `tests/fate/source-check.sh` output on
  the am-tree is byte-identical to `tests/ref/fate/source`
  (zero ours listed; was 13 pre-fix). Upstream CI blocker closed.
- [x] Subjects 1:1 cover↔mbox — each of
  `[PATCH 1/2] avcodec: add native AC-4 decoder core` and
  `[PATCH 2/2] avcodec: wire ff_ac4_decoder` appears exactly once
  in `COVER-A.txt` and once in the mbox
  (Subject: lines 4, 14282).
- [x] Zero placeholders — zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in cover + mbox (mechanical grep).
- [x] Signoffs — `Michael Rowlinson <mrowlinson@gmail.com>` on both
  patches (mbox `From:` lines 2, 14280; `From ` lines 1, 14279).
- [x] ASCII — mbox ZERO non-ASCII lines; cover ZERO non-ASCII
  (untouched). No MIME headers → `git send-email` expects zero
  8bit warnings.
- [x] Honest-gap note — single `TBD` is mbox line 5467, the in-code
  3ch coupling note, as disclosed (NOT a send placeholder).
- [x] `_tables` — 6 content-only hits (2 in the 1/2 message + 4
  in-code `src/*` provenance comments); zero `*_tables` filenames.
- [x] Full build — out-of-tree `configure --disable-doc` rc=0,
  `make ffmpeg` rc=0; 34 warning-lines (29 `warning:` + 5 summary),
  0 in our files — same shape as re-cut 4; decoder row
  `A..X.D ac4 ATSC A/344 Dolby AC-4`; buildconf shows bare
  `--disable-doc`, no extras; no ac4 encoder row.
- [x] Smoke 11/11 POST bytes — exact `gate11.sh` recipe
  (`-strict experimental -v info -i vec -map 0:a -f s16le`), every
  `vec/*.ac4` rc=0 `cmp`-identical to BOTH the master shim
  (`dec-lib` @ this branch) and the independent streaming driver
  (all `ff-vs-shim=SAME`, `ff-vs-sprobe=SAME`), AND to the re-cut-4
  bytes (`ff-vs-r4=SAME` all 11: unchanged-bytes proof).
- [x] N5 pins live — gpac log shows 48000 Hz / 1602 spf max / 6ch;
  gpac PCM 5727312 B, sha256 `8b41f7cf…b39f6`, input sha
  `b82fabba…70ce1`: all three EXACT-match `fate-n5/pins.txt`.
- [x] F2 gate (`lavc-dec/tests/f2-gap-trunc.sh` vs fresh binary):
  `== F2 GATE PASS ==` (rc=69 no-sync loud, 0 B; stereo control rc=0).
- [x] `make clean` survival — separate in-tree clone: 0 tracked dirty,
  0 deletions; 25/25 new files survive incl. 8/8 ours `*_tab`
  sources (13 `*_tab.*` on disk = 8 ours + 5 pre-existing in-tree).
- [x] patcheck — 738 hits, identical classes to re-cut 4 (guard
  lines add zero hits). Clean-under-triage (same triage: vendored
  DSP keeps its style).
- [x] Send-shape — `mkdir -p send && git mailsplit -osend` → 0001+0002
  with correct subjects; `COVER-A.txt` has Subject + blank + body 0000
  shape.
- [x] Nothing sent yet — no SENT marker in repo or mail-combined;
  Series A and mail-1 both unsent at gate time.

## Carried gates (re-cut-4 validation, stand via byte-identity)

Am-tree DSP bytes are identical to the re-cut-4 bytes the
`ff-A-post-r4-recut` seat validated (r5 delta = guard triples only,
preprocessor-neutral modulo macro names; smoke/F2/N5 re-proven
above on the new bytes), so these stand without re-run: dec-lib
unit PASS; fate-n5 14/14; ngis-gate + fuzzfix-gate PASS (pins,
repros, grinds); wrap-level repro75 + corner100 rc=0; tseek T1-T6
ALL PASS; ASan/UBSan 6/6 clean. Neg controls (splice fatal, garbage
cap, no-bed, drc/FLT init refusals, empty input) stand on the same
identity. Patch-1 standalone inert carried by mechanism-identity:
the Makefile hunk is byte-identical r4→r5 and the guard stays
undefined without 2/2.

## Cover delta (this re-cut: none)

- `COVER-A.txt` untouched — subjects, patch blurbs, Testing/FATE/
  N5-note/Provenance/Signoff all still exact. No revert path needed.

## Disclosed carries (reviewer-visible, non-blocking)

- `dec_lib.c` 3ch comment `coupling apply TBD` (mbox line 5467):
  in-code honest-gap note (parse-only for alignment), NOT a send
  placeholder; behavioral gates pass with it.
- This series pins no goldens; Series B FATE goldens are recorded
  against the fixed N5 labels (cover says so).
- B-escalation counted 12 failing headers on the pre-N5 tree; the
  re-cut-4 tree carries a 13th (`ac4_src.h`, same defect). All 13
  fixed here.

## Owner-at-post (fill when sending)

- [ ] P3 flip confirmed by posting (runbook step 10 stale: says A0
  first; decoder-first approved with this pack).
- [ ] Sent date + Message-IDs (0/2, 1/2, 2/2): _______________
- [ ] Archive shows 0/2 + 1/2 + 2/2 threaded in order.
- [ ] Mail-1 (separate, `samples-request@ffmpeg.org` + 15
  attachments) sent date: _______________ (starts no clock for A;
  +24 h after suite-live gates A0/B2).

## Verdict

**READY-TO-POST** — Series A decoder send pack final (re-cut 5).
