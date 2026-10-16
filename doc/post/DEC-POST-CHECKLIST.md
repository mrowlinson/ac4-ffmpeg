# DEC-POST-CHECKLIST — decoder-first post pack (Series A), RE-CUT 2

Branch: `ff/dec-wrap-revendor` from master `7ba72eb`. Staged
2026-10-16 in scratch `ff-wrap-revendor` (TMPDIR there); parent repo
untouched, nothing pushed, no MBP/remote. Staging commits live only
in the scratch ffmpeg clone (`mboxsrc@82a49d8`, branch
`ac4/A-decode-post-recut2`, never pushed); the post artifact is the mbox.

Re-cut vs the `ff/dec-cleanfiles` staging (`7ba72eb` set): patch 1/2
carries the re-vendored core — the FINDING-1/FUZZ-DEC-1 dec-lib fixes
(ngis init + E4_MAXGRP bound + ngis[sel] reject, `d20e772`/`61366ac`)
ported into `lavc-dec/core/dec_lib.c` (+14 lines, 0 removals,
`999b4e3`; closes the carried stale-core NOTE). Patch 2/2 is
byte-identical to the prior staging. Cover letter UNCHANGED (subjects
identical; the fix is inside 1/2's snapshot, no new reviewer-visible
behavior to describe). Re-gated end to end below.

## Pack contents (this branch)

- `doc/covers/COVER-A.txt` — UNCHANGED from `7ba72eb` (decoder cover
  letter, 0/2 shape, owner name filled, N5 + tripwire + rename + F2
  stated, no open items). NOT yet sent.
- `doc/post/series-A-decoder.mbox` — the 2 in-tree patches vs
  `3e6cd5e`, byte-generated from `lavc-dec/` @ `999b4e3` (wiring
  hunks content-identical to `wiring.patch`; core + wrapper
  byte-identical to the re-gated set; 606436 B; delta vs prior mbox
  is exactly the 14 fix lines + mail headers).
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
A0 (proven all stagings: am-clean + smoke/gates on the am tree). It
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

- FINDING-1 repros fixed through wrap 3/3: PASS (WRAP-FUZZ-RERUN
  isolated pins `wrap 13272178844 --repro 6`, `wrap 34507664996
  --repro 2`, `corner 2654435771` — all rc=0 3x each under
  ASan/UBSan, was `:1340` OOB abort; negative control: the pre-fix
  binary still aborts at `dec_lib.c:1340` on the same command).
- Wrap fuzz 300-iter smoke: PASS (`run.py wrap 1 300 --batch 25`,
  both corpora: 12/12 batches ok, crash=0 hang=0 ubsan=0 oom=0;
  batch b4 seed 13272178844 previously crashed, now ok). 0 new crashes.
- `git am` clean on pristine `3e6cd5e`: PASS (fresh clone, empty
  status pre-am; whitespace warning is the known `entropy.c:1226`
  wart only; post-am tree diff-empty vs the staging commits).
- Full build: PASS (`configure --disable-doc`, no extras, out-of-tree;
  `make ffmpeg` rc=0; 34 warnings, 0 in our 24 files — all
  pre-existing elsewhere; decoder row `D.A.L. ac4 AC-4` registered).
- Patch-1 standalone build: PASS (rc=0; `-decoders` shows no ac4 —
  inert under the undefined guard, as designed; no core warnings).
- `make clean` survival: PASS (in-tree configure + `make clean` on
  the am tree: ZERO tracked deletions).
- Byte-exact 11/11: PASS (oracle shim + sprobe from the cleanfiles
  set; every vector `ffrc=0 shimrc=0 SAME/SAME`: chan, G6-514,
  corr51-96, w6-514, w6-ims, w6-stereo, w7-51-96k, w7-ims64k,
  w8-514, gpac N5-tripwire 46034 Hz, pbde).
- Negs 9/9: PASS (rc + bytes match the B2F2-verify table exactly, and
  all nonzero PCM outputs `cmp`-identical to the cleanfiles wrap's:
  splice 69+294912B, ajoc 69+0B, garbage cap-abort 69+0B, garbage5
  234, empty 234, bad40 rc0+2138112B, F1 crc-bad rc0+16384B, F3
  probe-garbage 234+0B, stereo rc0+16384B).
- tseek T1-T6: PASS (rc=0: T1 49f/2ch/48k 401408 B; T2 IDENTICAL;
  T3 47f/6ch/48k IDENTICAL + relatch; T4 req-flt fails (-22);
  T5 pres_sel=15 ret=-1; T6 CLEAN-EOF; `TSEEK: ALL PASS` —
  numbers identical to the B2F2 verify).
- F2 gate (committed `lavc-dec/tests/f2-gap-trunc.sh`): PASS
  (`== F2 GATE PASS ==`: rc=69, no-sync loud, 0 PCM bytes; stereo
  control rc=0).
- ASan/UBSan: PASS, re-run (whole-tree instrumented build): 11/11
  vectors rc=0 with PCM byte-identical to the normal build, zero
  sanitizer reports; 6 neg-spot legs (splice/ajoc/garbage/bad40/F1/
  f2gap) rc-match + PCM SAME, zero reports. Plus the 300-iter
  sanitizer fuzz smoke above (ubsan=0).
- patcheck files form: clean-under-triage (in-tree `tools/patcheck`
  @ 3e6cd5e over the 2 patches; number-blind diff vs the prior
  triage shows exactly ONE added line — the word "else" inside the
  new FUZZ-DEC-1 comment, same already-triaged comment-text class;
  single real wart still `entropy.c:1226`).
- Cover subjects match mbox: PASS (mechanical string compare, 1/1 each).

## READY / PENDING

READY:
- mbox (FINDING-1 fixed through wrap, fuzz smoke clean, am-clean,
  builds incl. patch-1 standalone, byte-exact, negs, tseek, F2 gate,
  ASan re-run, subjects match) — postable.
- Cover letter (unchanged, still accurate: name filled, N5 +
  tripwire stated, rename + F2 stated, no open items) — ready to send.
- `git send-email` mechanics (base, subjects, signoffs verified).
- P1 CLEANFILES: FIXED + re-gated + re-cut. CLOSED.
- Wrap-core re-vendor (stale-core NOTE): DONE here — `core/` now at
  dec-lib tree `8cdccea` (was `d2d641a`), procedure fidelity proven
  (OLDCORE-IDENTICAL + 14-line-only delta). CLOSED.

PENDING (owner or follow-up):
- P2: mail-1 send (owner) + 24 h clock. Blocks A0 + B2, not this
  series.
- P3: owner confirms Series-A letter + decoder-first order flip.
- P4: N5fix landing + fri 0-12 golden re-pin (future B2 concern;
  this series pins no goldens).
- P5 (hygiene backlog, postable as-is): `entropy.c:1226` trailing
  whitespace + vendored-style patcheck classes at next re-vendor.
