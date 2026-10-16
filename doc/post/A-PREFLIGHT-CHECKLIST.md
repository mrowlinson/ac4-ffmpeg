# A-PREFLIGHT-CHECKLIST — Series A decoder FINAL re-cut 4 (owner posts)

Branch: `ff/doc-A-post-r4` from master `df3f2de`.
Gated 2026-10-16 in scratch `ff-A-post-r4-recut` (TMPDIR there);
parent repo untouched (read-only clone source), nothing pushed,
no MBP/remote, nothing sent.
Send procedure: `doc/post/A-OWNER-SEND-STEPS.md`.

## Why re-cut 4 (drift since re-cut 3)

- [x] Master `df3f2de` merges the N5 re-vendor (`96369c7`): `core/`
  gains `ac4_src.h` (s5.11 count rules) + N5 DSP/API in `dec_lib.c` /
  `ac4d.h`, and `ac4dec.c` takes the N5 contract adaptation (4-arg
  flush, get_info-max spf latch, per-frame counts). The re-cut-3 mbox
  embeds the PRE-N5 core (gpac 46034 Hz) → re-cut REQUIRED.
- [x] Merge-resolution audit: `core/` identical `96369c7..df3f2de`;
  `ac4dec.c` delta = 2 comment lines + 1 log-string line ("shape" →
  "channel count", same fatal path). Zero behavior delta vs the
  revendor-validated tree.
- [x] `COVER-A.txt` N5 note ("pending re-pin", 46034 Hz) is FALSE
  against this core → cover flipped to LANDED in this re-cut (see
  "Cover delta" below). Subjects unchanged.

## Re-cut 4 assembly (this wave)

- [x] Staging `7955a1e` (1/2, 25 files: Makefile + 24 core) +
  `6144274` (2/2, `ac4dec.c` + `allcodecs.c`) on pristine `3e6cd5e`:
  `wiring.patch` split-applied clean (Makefile hunk → 1/2, allcodecs
  hunk → 2/2), 24 core + wrapper copied from `df3f2de` staging.
- [x] 1/2 message verbatim from re-cut 2/3 (body byte-identical; port
  text still accurate). 2/2 message: N5 paragraph rewritten LANDED
  (base_fs x sf_mult, 48000 Hz / 1601-1602 frames, max-latch);
  paragraphs 1–2 + signoff verbatim.
- [x] Mbox `series-A-decoder.mbox` 620505 B / 15226 lines (was 606385).
  Delta vs re-cut 3 = N5 DSP/API + `ac4_src.h` + wrapper adaptation
  + 2/2 message + ASCII map completion (em-dash gone).

## Final gates (this wave, all green)

- [x] `git am` clean on pristine `3e6cd5e` — rc=0, commits `6f026cb`
  (1/2) + `f6a620e` (2/2). ZERO warnings (`am.log` = 2 `Applying:`
  lines only). `git show --check`: 0/0. Whole-tree hashes identical
  to the staging commits' trees.
- [x] Re-cut identity — all 25 new files `cmp`-identical am-tree vs
  `df3f2de` staging; am-tree `format-patch` round-trips byte-identical
  (same 620505 B; 15220/15220 normalized lines).
- [x] Subjects 1:1 cover↔mbox — each of
  `[PATCH 1/2] avcodec: add native AC-4 decoder core` and
  `[PATCH 2/2] avcodec: wire ff_ac4_decoder` appears exactly once
  in `COVER-A.txt` (lines 11, 19) and once in the mbox
  (Subject: lines 4, 14282).
- [x] Zero placeholders — zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in cover + mbox (mechanical grep).
- [x] Signoffs — `Michael Rowlinson <mrowlinson@gmail.com>` on both
  patches (mbox lines 28, 14308; `From ` lines 1, 14279; `From:`
  headers match).
- [x] ASCII — mbox ZERO non-ASCII lines (r3's carried em-dash is
  gone: the re-vendor ASCII map transliterated it); cover ZERO
  non-ASCII (2 old em-dashes transliterated with the N5 flip). No
  MIME headers → `git send-email` expects zero 8bit warnings.
- [x] Honest-gap note — single `TBD` is mbox line 5467, the in-code
  3ch coupling note, as disclosed (NOT a send placeholder).
- [x] `_tables` — 6 content-only hits (2 in the 1/2 message + 4
  in-code `src/*` provenance comments); zero `*_tables` filenames.
- [x] Full build — out-of-tree `configure --disable-doc` rc=0,
  `make ffmpeg` rc=0; 34 warning-lines (29 `warning:` + 5 summary),
  0 in our files — same shape as re-cut 3; decoder row
  `A..X.D ac4 ATSC A/344 Dolby AC-4`; buildconf shows bare
  `--disable-doc`, no extras; no ac4 encoder row.
- [x] Smoke 11/11 POST bytes — exact `gate11.sh` recipe
  (`-strict experimental -v info -i vec -map 0:a -f s16le`), every
  `vec/*.ac4` rc=0 `cmp`-identical to BOTH the N5-current shim
  (`dec-lib` @ master) and the independent streaming driver
  (all `ff-vs-shim=SAME`, `ff-vs-sprobe=SAME`).
- [x] N5 pins live — gpac log shows 48000 Hz / 1602 spf max / 6ch;
  gpac PCM 5727312 B, sha256 `8b41f7cf…b39f6`, input sha
  `b82fabba…70ce1`: all three EXACT-match `fate-n5/pins.txt`.
- [x] F2 gate (`lavc-dec/tests/f2-gap-trunc.sh` vs fresh binary):
  `== F2 GATE PASS ==` (rc=69 no-sync loud, 0 B; stereo control rc=0).
- [x] `make clean` survival — separate in-tree clone: 0 tracked dirty,
  0 deletions; 25/25 new files survive incl. 8/8 ours `*_tab`
  sources (13 `*_tab.*` on disk = 8 ours + 5 pre-existing in-tree).
- [x] patcheck — 738 hits vs r3's 713: same classes, no new class;
  +25 all in N5-new lines (`ac4_src.h` + s5.11 DSP: +8 x==0, +7
  divide-by-2^x, +5 brace-style, +4 missing-}-else, +1 reserved-ident).
  Clean-under-triage (same triage: vendored DSP keeps its style).
- [x] Send-shape — `mkdir -p send && git mailsplit -osend` → 0001+0002
  with correct subjects; `COVER-A.txt` has Subject + blank + body 0000
  shape.
- [x] Nothing sent yet — no SENT marker in repo or mail-combined;
  Series A and mail-1 both unsent at gate time.

## Carried gates (revendor-N5, stand via byte-identity)

Am-tree bytes are identical to the `df3f2de` staging the
`ff-wrap-revendor-N5` seat validated in-tree (modulo the 1 log-string
line noted above), so these stand without re-run: dec-lib unit PASS;
fate-n5 14/14; ngis-gate + fuzzfix-gate PASS (pins, repros, grinds);
wrap-level repro75 + corner100 rc=0; tseek T1-T6 ALL PASS; ASan/UBSan
6/6 clean (from the Q1 wave, same DSP modulo N5 semantics which the
smoke re-proves byte-exact). Neg controls (splice fatal, garbage cap,
no-bed, drc/FLT init refusals, empty input) re-proven by the revendor
seat on these bytes. Patch-1 standalone inert carried by
mechanism-identity: the Makefile hunk is byte-identical r3→r4 and the
guard stays undefined without 2/2.

## Cover delta (this re-cut; parent can revert surgically)

- N5 note rewritten pending→LANDED (48000 Hz / 1601-1602-sample
  frames / 5727312 B; max-latch; B goldens recorded on fixed labels).
  Posting the old note would misstate the shipped behavior.
- 2 long-carried em-dashes (old lines 5, 49) transliterated to `--`
  (one inside the rewritten note, one in the intro) → cover pure ASCII.
- Subjects, patch blurbs, Testing/FATE/Provenance/Signoff lines
  untouched. If the cover track wants its own wording, only the N5
  paragraph needs re-replacement; subjects are unaffected.

## Disclosed carries (reviewer-visible, non-blocking)

- `dec_lib.c` 3ch comment `coupling apply TBD` (mbox line 5467):
  in-code honest-gap note (parse-only for alignment), NOT a send
  placeholder; behavioral gates pass with it.
- This series pins no goldens; Series B FATE goldens are recorded
  against the fixed N5 labels (cover says so).

## Owner-at-post (fill when sending)

- [ ] P3 flip confirmed by posting (runbook step 10 stale: says A0
  first; decoder-first approved with this pack).
- [ ] Sent date + Message-IDs (0/2, 1/2, 2/2): _______________
- [ ] Archive shows 0/2 + 1/2 + 2/2 threaded in order.
- [ ] Mail-1 (separate, `samples-request@ffmpeg.org` + 15
  attachments) sent date: _______________ (starts no clock for A;
  +24 h after suite-live gates A0/B2).

## Verdict

**READY-TO-POST** — Series A decoder send pack final (re-cut 4).
