# A-PREFLIGHT-CHECKLIST — Series A decoder FINAL re-cut 3 (owner posts)

Branch: `ff/doc-A-post-r2` from master `cc42e2c`.
Gated 2026-10-16 in scratch `ff-A-post-r2-retry` (TMPDIR there);
parent repo untouched (read-only clone source), nothing pushed,
no MBP/remote, nothing sent.
Send procedure: `doc/post/A-OWNER-SEND-STEPS.md`.

## Stay-alive gate

- [x] master contains `b86587f` — PASS round 1
  (`cc42e2c` merges it via `0e5aa0f`; no sleep/re-poll needed).
  Prior seat `ff-A-post-r2` saw pre-merge master `ec13cba` and
  exited BLOCKED; parent has since merged.

## Why re-cut 3 (drift since re-cut 2)

- [x] `b86587f..master` touches 11 files in mbox scope
  (`lavc-dec/core/*`, the `5c8740a` hygiene: 64 comment lines
  UTF-8→ASCII + `entropy.c` tail-WS removal) → re-cut REQUIRED.
  (The `A-*` files also list in that range only because `999b4e3`
  branched from `7ba72eb`; not real drift.)
- [x] Merge fidelity: all 14 `999b4e3` FINDING-1 fix lines present
  verbatim in merged staging; hygiene conversions present.
- [x] `COVER-A.txt` unchanged in range (no drift) → cover kept
  verbatim (rationale: subjects identical, delta reviewer-invisible).

## Re-cut 3 assembly (this wave)

- [x] Staging `5602d33` (1/2, 24 files) + `de33e47` (2/2, 2 files)
  on pristine `3e6cd5e`: `wiring.patch` applies clean, 23 core +
  wrapper copied from `cc42e2c` staging, commit messages verbatim
  from re-cut 2 (verified byte-equal to re-cut-2 mbox bodies).
- [x] Mbox `series-A-decoder.mbox` 606385 B (was 606436).
  Normalized delta vs re-cut 2 = exactly 64 comment-line swaps +
  1 tail-line deletion + diffstat/index/mail-header churn
  (`entropy.c` 1226→1225, insertions 13739→13738). The 3
  code+trailing-comment lines keep exact code twins. Zero code change.

## Final gates (this wave, all green)

- [x] `git am` clean on pristine `3e6cd5e` — rc=0, commits
  `6d38cf5` (1/2) + `f7f9fd9` (2/2). ZERO warnings: the known
  `entropy.c:1226` wart is GONE (`git show --check`: 0/0).
- [x] Re-cut identity — all 24 files `cmp`-identical am-tree vs
  staging; wiring hunks content-identical to `wiring.patch`;
  am-tree `format-patch` round-trips byte-identical (mod headers).
- [x] Subjects 1:1 cover↔mbox — each of
  `[PATCH 1/2] avcodec: add native AC-4 decoder core` and
  `[PATCH 2/2] avcodec: wire ff_ac4_decoder` appears exactly once
  in `COVER-A.txt` and once in the mbox (mechanical counts).
- [x] Zero placeholders — zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in cover + mbox (mechanical grep).
- [x] Signoffs — `Michael Rowlinson <mrowlinson@gmail.com>` on both
  patches (mbox lines 28, 13998; From lines 1, 13970).
- [x] Strip-identity — comment-stripped 24/24 byte-identical am-tree
  vs staging AND vs the fully-gated re-cut-2 am tree (same strip
  tool as the hygiene verify). Carries all behavioral gates below.
- [x] Full build — out-of-tree `configure --disable-doc` rc=0,
  `make ffmpeg` rc=0; 34 warnings, 0 in our 24 files; decoder row
  `A..X.D ac4 ATSC A/344 Dolby AC-4`; buildconf shows bare
  `--disable-doc`, no extras.
- [x] `make clean` survival — in-tree configure + `make clean`:
  ZERO tracked dirty, 0 deletions; all `*_tab` sources survive.
- [x] patcheck — 714→713 hits; the single removed hit is the
  entropy tail-WS line. All other classes byte-identical counts
  (clean-under-triage, same triage as re-cut 2).
- [x] `git send-email` expectation — no MIME headers in mbox; 3
  non-ASCII lines total (mbox line 3570 + cover lines 5, 49) →
  benign informational 8bit warnings only, sends fine.
- [x] Nothing sent yet — no SENT marker in repo or mail-combined;
  Series A and mail-1 both unsent at gate time.
- [x] Checklists complete — this file + `A-OWNER-SEND-STEPS.md`
  (devel address, order, no-attachments rule) written and committed.

## Carried gates (re-cut 2, stand via strip-identity)

Byte-exact 11/11, negs 9/9 (rc+bytes+PCM), tseek T1-T6, F2 gate,
ASan/UBSan re-run clean, 300-iter fuzz smoke 0 crashes, patch-1
standalone inert — all in `doc/post/DEC-POST-CHECKLIST.md`
(re-cut-2 record; its byte claims are superseded by this re-cut,
its behavioral gates stand on identical stripped code).

## Disclosed carries (reviewer-visible, non-blocking)

- 1 em-dash in mbox line 3570 (`dec_lib.c` FINDING-1 comment,
  reintroduced by the revendor fix after hygiene branched) + 2
  long-carried cover em-dashes (lines 5, 49). Benign 8bit warnings
  only. Not hand-fixed: `core/` is generate-only, and the cover
  track (`ff/doc-cover-r3`) owns letter prose.
- `dec_lib.c` 3ch comment `coupling apply TBD` (mbox line 5256):
  in-code honest-gap note (parse-only for alignment), NOT a send
  placeholder; behavioral gates pass with it.
- N5 rate mislabel: cover carries the note + gpac tripwire pin
  (46034 Hz / spf 1536 / 5492736 B); `ff/dec-n5fix` unlanded; this
  series pins no goldens → nothing stale.

## Owner-at-post (fill when sending)

- [ ] P3 flip confirmed by posting (runbook step 10 stale: says A0
  first; decoder-first approved with this pack).
- [ ] Sent date + Message-IDs (0/2, 1/2, 2/2): _______________
- [ ] Archive shows 0/2 + 1/2 + 2/2 threaded in order.
- [ ] Mail-1 (separate, `samples-request@ffmpeg.org` + 15
  attachments) sent date: _______________ (starts no clock for A;
  +24 h after suite-live gates A0/B2).

## Verdict

**READY-TO-POST** — Series A decoder send pack final (re-cut 3).
