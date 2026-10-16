# A-PREFLIGHT-CHECKLIST — Series A decoder FINAL (owner posts)

Branch: `ff/doc-A-post-final` from master `7ba72eb`.
Gated 2026-10-16 in scratch `ff-A-post-final` (TMPDIR there);
parent repo untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure: `doc/post/A-OWNER-SEND-STEPS.md`.

## Stay-alive gate

- [x] master contains `ec78459` — PASS round 1
  (`7ba72eb` merges it; no sleep/re-poll needed).

## Final gates (this wave, all green)

- [x] `git am` clean on pristine `3e6cd5e` — rc=0, commits
  `9eba363` (1/2, 24 files) + `87a395e` (2/2, 2 files).
  Only diagnostic is the known `entropy.c:1226` trailing-whitespace
  wart on 1/2 (`git show --check`: 1/2 flags that line only,
  2/2 zero). Wart is dec-lib-owned, postable as-is.
- [x] Subjects 1:1 cover↔mbox — each of
  `[PATCH 1/2] avcodec: add native AC-4 decoder core` and
  `[PATCH 2/2] avcodec: wire ff_ac4_decoder` appears exactly once
  in `COVER-A.txt` and once in the mbox (mechanical counts).
- [x] Zero placeholders — zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in cover + mbox (mechanical grep).
- [x] Signoffs — `Michael Rowlinson <mrowlinson@gmail.com>` on both
  patches (mbox lines 28, 13984).
- [x] No-drift — `ec78459..master` touches ZERO files in mbox scope
  (`lavc-dec/`, `COVER-A.txt`, `doc/post/`); mbox byte-identical to
  the re-cut gated in `DEC-POST-CHECKLIST.md` (605740 B). All carried
  gates (full build, patch-1 standalone, byte-exact 11/11, negs 9/9,
  tseek T1-T6, F2 gate, make-clean survival, patcheck triage, ASan
  carried) still stand on identical bytes.
- [x] Nothing sent yet — no SENT marker in repo or mail-combined;
  Series A and mail-1 both unsent at gate time.
- [x] Checklists complete — this file + `A-OWNER-SEND-STEPS.md`
  (devel address, order, no-attachments rule) written and committed.

## Disclosed carries (reviewer-visible, non-blocking)

- `entropy.c:1226` wart (above); UTF-8 in comments → benign 8bit
  send-email warnings; normalize at next re-vendor per Series-C bar.
- `dec_lib.c` 3ch comment `coupling apply TBD` (mbox line 5242):
  in-code honest-gap note (parse-only for alignment), NOT a send
  placeholder; byte-exact 11/11 + neg/tseek gates pass with it.
- `lavc-dec/core` stale vs master cfg6 ngis fixes (`d20e772` +
  `61366ac`): wrapper/demux path rejects the pinned crash shapes
  loud (rc=234 EINVAL, zero output, no crash — probed in dryrun);
  re-vendor (incl. rename map) owed as first follow-up.
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

**READY-TO-POST** — Series A decoder send pack final.
