# DEC-POST-CHECKLIST — decoder-first post pack (Series A), RE-CUT

Branch: `ff/dec-cleanfiles` from master `2ebab0c`. Staged
2026-10-16 in scratch `ff-cleanfiles-fix` (TMPDIR there); parent repo
untouched, nothing pushed, no MBP/remote. Staging commits live only
in the scratch ffmpeg clone (`ffsrc@c2c4aef`, branch
`ac4/A-decode-post-recut`, never pushed); the post artifact is the mbox.

Re-cut vs the `ff/doc-dec-post` staging (`bd8c505` set): the 8
`*_tables.c/h` core sources are renamed `*_tab.c/h` (P1 CLEANFILES
must-fix, closed here), and the 2/2 wrapper folds the F2 gap-trunc
loud-fail delta (`a12e7a9`, committed to master after the first
staging). Re-gated end to end below; ASan + patcheck-triage carried
as noted.

## Pack contents (this branch)

- `doc/covers/COVER-A.txt` — decoder cover letter, 0/2 shape (was 0/3
  with an A0-must-land-first premise). Owner name filled
  (`Michael Rowlinson <mrowlinson@gmail.com>`), N5 pending-repin +
  tripwire pin stated, rename rationale + truncated-tail rule stated,
  no open items. NOT yet sent.
- `doc/post/series-A-decoder.mbox` — the 2 in-tree patches vs
  `3e6cd5e`, byte-generated from `lavc-dec/` @ `e123c70` (wiring
  hunks content-identical to `wiring.patch`; core + wrapper
  byte-identical to the re-gated set; 605740 B).
- `doc/post/DEC-POST-CHECKLIST.md` — this file.

Naming flag (carried): the staging brief called this a "B-series
mbox"; the repo convention (SUBMIT-PLAN-V2, runbook step 10, COVER-B
= docs / FATE) names the decoder Series A, so the pack is staged as
Series A. Owner to confirm the letter before post.

## Post order (constraints, then recommendation)

Hard constraints (unchanged):
1. Mail-1 (samples request, `ff-samples-pack/REQUEST.txt`) must be
   SENT by the owner, confirmed, suite live, +24 h — before A0 (its
   squash carries M3 fate refs) and before B2 (decoder FATE).
2. A0 before B2 (B2 demux tests assume A0 copy/pts behavior).
3. B2 before C is NOT required (`No B dependency; B deferred`).

Decoder-first flip vs runbook step 10 (A0-posts-first): the decoder
series applies to pristine `3e6cd5e` and decodes byte-exact with no
A0 (proven both stagings: am-clean + smoke/gates on the am tree). It
carries no FATE and needs no suite files, while A0 waits on mail-1.
So: RECOMMENDED order is decoder now, A0+B after mail-1 clears —
owner to confirm the flip (runbook step 10 still says A0 first).

Full recommended sequence: mail-1 (owner, now) -> decoder A (this
pack, READY) -> A0 (after mail-1+24 h) -> B1/B2 -> C(11) -> E1 ->
E2-R -> D(5) -> (D6 placement TBD).

## Lists

- Patches (+ cover): `ffmpeg-devel@ffmpeg.org` via `git send-email`
  (mbox + COVER-A body; `[PATCH 0/2]` + 2 patches).
- Samples: `samples-request@ffmpeg.org` (mail-1, owner-sent; body =
  `ff-samples-pack/REQUEST.txt`, 11 attachments, NOT the .md docs).

## Sample deps (mail-1-dependent items marked, never faked)

- This series: ZERO sample deps — no FATE, no suite files. READY.
- Mail-1 state: owner name filled, SEND PENDING (UNSENT). PENDING.
- A0 M3 fate needs `ac4/clip-stereo.ac4` + `ac4/clip-51-real.ac4`
  live in suite: blocked on mail-1 + 24 h. PENDING.
- B2 decoder FATE needs the full mail-1 set + 24 h. PENDING.
- N5 re-pin: when `ff/dec-n5fix` lands (lib-owned, currently
  unlanded), ALL fri 0-12 FATE goldens must be re-recorded
  (resampled PCM + new labels). Tripwire pin (current, mislabeled
  bytes, deliberate): gpac vector fri=3/fs=1 -> sr 46034 Hz, spf
  1536, 298 frames x 1536 x 6ch x 2 B = 5492736 bytes. PENDING
  (blocks no post in this pack — this series pins no goldens).

## Gates run this re-cut (all green)

- `git am` clean on pristine `3e6cd5e`: PASS (fresh clone, empty
  status pre-am; whitespace warning is the known `entropy.c:1226`
  wart only — am logs old/new mbox byte-identical; post-am tree
  diff-empty vs the staging commits).
- Full build: PASS (`configure --disable-doc`, no extras, out-of-tree;
  `make ffmpeg` rc=0; 34 warnings, 0 in our 24 files — all
  pre-existing elsewhere; decoder row
  `A..X.D ac4 ATSC A/344 Dolby AC-4` registered).
- Patch-1 standalone build: PASS (rc=0; no ac4 decoder row — inert
  under the undefined guard, as designed; no core warnings).
- `make clean` survival: PASS (empirical, non-vacuous control).
  In-tree configure + `make clean` on the re-cut tree: ZERO tracked
  deletions. Same procedure on the pre-rename mbox deletes exactly
  the 8 `*_tables.c/h` (replicates the first staging's t8 FAIL).
  P1 CLOSED.
- Byte-exact 11/11: PASS (fresh oracle shim from branch `dec-lib` +
  fresh sprobe from the RENAMED core, zero warnings; every vector
  `ffrc=0 shimrc=0 SAME/SAME`: chan, G6-514, corr51-96, w6-514,
  w6-ims, w6-stereo, w7-51-96k, w7-ims64k, w8-514, gpac N5-tripwire
  46034 Hz, pbde).
- Negs 9/9: PASS (rc + bytes match the B2F2-verify table exactly, and
  all 9 PCM outputs `cmp`-identical to the pre-rename wrap's:
  splice 69+294912B, ajoc 69+0B, garbage cap-abort 69+0B, garbage5
  234, empty 234, bad40 rc0+2138112B, F1 crc-bad rc0+16384B, F3
  probe-garbage 234+0B, stereo rc0+16384B).
- tseek T1-T6: PASS (rc=0: T1 49f/2ch/48k 401408 B; T2 IDENTICAL;
  T3 8ch IDENTICAL + 2ch→8ch relatch; T4 req-flt fails (-22);
  T5 pres_sel=15 ret=-1; T6 CLEAN-EOF; `TSEEK: ALL PASS` —
  numbers identical to the pre-rename verify).
- F2 gate (committed `lavc-dec/tests/f2-gap-trunc.sh`): PASS
  (`== F2 GATE PASS ==`: rc=69, no-sync loud, 0 PCM bytes; stereo
  control rc=0) — proves the F2 fold is live in the re-cut 2/2.
- patcheck files form: clean-under-triage (713 hits, same count and
  classes as the pre-rename triage `PATCHECK-TRIAGE.md`; single real
  wart still `entropy.c:1226`, dec-lib-owned; zero `tables` hits).
- Cover subjects match mbox: PASS (mechanical string compare).
- ASan/UBSan: CARRIED, not re-run (10/10 + 6/6 clean on the
  behavior-identical pre-rename bytes, B2F2-fix beat 5 + B2F2-verify
  §6; this wave proves byte-identity of all PCM outputs old/new, so
  no sanitizer-legible behavior changed).

## READY / PENDING

READY:
- mbox (am-clean, builds incl. patch-1 standalone, byte-exact,
  negs, tseek, F2 gate, subjects match) — postable.
- Cover letter (name filled, N5 + tripwire stated, rename + F2
  stated, no open items) — ready to send.
- `git send-email` mechanics (base, subjects, signoffs verified).
- P1 CLEANFILES: FIXED + re-gated + re-cut. CLOSED.

PENDING (owner or follow-up):
- P2: mail-1 send (owner) + 24 h clock. Blocks A0 + B2, not this
  series.
- P3: owner confirms Series-A letter + decoder-first order flip.
- P4: N5fix landing + fri 0-12 golden re-pin (future B2 concern;
  this series pins no goldens).
- P5 (hygiene backlog, postable as-is): `entropy.c:1226` trailing
  whitespace + vendored-style patcheck classes at next re-vendor.
- NOTE (carried, out of scope): `lavc-dec/core/` is stale vs the
  `ff/dec-ngis-bounds` dec-lib fix (E4_MAXGRP bound missing);
  re-vendor owed per the Q1 rule (now incl. the rename map).
