# B-PREFLIGHT-CHECKLIST -- Series B doc + decoder-FATE FINAL re-cut 2 (owner posts)

> SCOPE NOTE (size sweep `COVER-SIZE-SWEEP.md`): this file records
> re-cut-2 bytes (19150 B, 14 diffs). The merged mbox is the later
> MUST-fix union re-cut: `series-B-fate.mbox` 40708 B, 16 diffs
> (3 + 13), 11 goldens, subjects/signoffs unchanged. Size/count
> claims below describe re-cut 2, not current bytes.

Branch: `ff/doc-B-post-final-2` from master `e72a4b9`.
Gated 2026-10-16 in scratch `ff-B-714-recut` (TMPDIR `tmp/`);
parent repo untouched, nothing pushed, no remote builds, nothing sent.
Send procedure: `doc/post/B-OWNER-SEND-STEPS.md`. Stage background
(deltas, stanza parity, suite history):
`doc/post/B-POST-CHECKLIST.md` (`d731060` STAGED pack -- never post).
Supersedes `2330a26` B-post-final (stale 51200 714 golden -- never
post). Blocker record: `doc/post/COMBO-RERUN-7.md` (finding F1).

## Suite-landed gate P1

- [x] Live re-verify in this scratch: `rsync
  rsync://fate-suite.ffmpeg.org/fate-suite/ac4/` rc=0 +
  `sha256sum -c EXPECTED.sha256` -> 12/12 OK (own mirror,
  first-party on top of `SUITE-LANDED.md` 22:23:09Z).
  clip-714 accepted (12/12, no drops).
- [ ] +24h clock ends 2026-10-17T22:23:09Z -- OWNER-AT-POST confirm.

## Re-cut assembly (this seat)

- [x] Staging stack: pristine `3e6cd5e` + `series-A-decoder.mbox`
  r5 (`ee13316`, `5276a21`) + `series-A0-head.mbox` (`9e0d9e1`),
  each `git am --whitespace=error` rc=0. B commits `f33da8d`
  (1/2 doc, patch-body identical to B-final -- options frozen 6/6,
  verified by diff) + `b26048f` (2/2 fate, 11 files,
  248 insertions).
- [x] Combo-7 F1 fixed (the ONLY delta vs B-final): 714 golden
  re-recorded from fresh decode on the A-r5+A0 stack per the
  re-record rule -- 48000 Hz / 1920 spf, bytes
  `6f5b3a..`/`6d73c3..`/`89090903..`, `cmp`-SAME vs the
  `COMBO-RERUN-7.md` bytes (independent confirm on r5, not a
  copy). Mak comment `51200 Hz clip reality` ->
  `48000 Hz / 1920 spf N5 output (out_sr = base_fs x sf_mult)`.
  F1 reproduced pre-fix (FAIL with exactly the combo-7 bytes),
  PASS post-fix. P2 patch-diff vs B-final = only the 714 ref
  (2 header + 3 frame lines) + the 1 comment line.
- [x] Carried intact from B-final (verified by diff, not re-derived):
  F8 prefix-less refs, crc-note `skipping bad packet` REF, P3
  F1/F1b/F2/F3 calls, ims120 pres-1 pin + honest mak comment.
  Zero PENDING stubs anywhere (mechanical grep over mbox + tree).
- [x] Mbox `doc/post/series-B-fate.mbox` 19150 B, 14 diffs (3 +
  11); final form (no caveats); 2/2 msg carries the ims120
  pres-1 note (unchanged).

## Final gates (this seat, all green)

- [x] `git am` clean on pristine `3e6cd5e` -- rc=0, commits
  `2f6caf6` (1/2) + `4fdc64a` (2/2), zero whitespace errors.
  14/14 B files `cmp`-SAME am-tree vs staging; filesets +
  namesets identical; `git show --check` clean on both commits.
- [x] `make fate-ac4` 14/14 rc=0 on the live suite + second run
  rc=0 (goldens deterministic); no-SAMPLES run gives the
  standard missing-samples failure shape.
- [x] Gate-split `--disable-everything` minimal-enables build
  rc=0 (decoder/demuxer/muxer ac4, pcm_s16le enc + muxer,
  rawvideo enc, framemd5/framecrc muxers, aresample, file+pipe)
  + `fate-ac4` 14/14 rc=0 -- F6 split gates proven on fixed bytes.
- [x] Pre-patch negatives CARRIED from B-final (proven there on
  identical stanzas): this re-cut changes one mak comment line +
  one ref file -- all 14 CMDs, all REFs, all gates byte-identical
  by diff, so the decoder-dependent / A0-dependent failure shapes
  cannot move. No C sources or headers added (by construction).
- [x] Full self-contained `make fate -j18`: 3054 TESTs, ZERO
  failures, rc=0 -- literal full-fate-green. The B-final sole-fail
  `fate-source` is FIXED by the A-r5 guard fix in the staging
  stack (this series still adds no C sources or headers). Zero
  ac4 tests in the subset (all 14 correctly extern-gated).
- [x] Subjects 1:1 cover<->mbox (`doc: AC-4 decoder docs`,
  `fate: add ac4 decoder tests`, each 1x/1x, mechanical);
  signoffs 2/2 `Michael Rowlinson <mrowlinson@gmail.com>`;
  ASCII-only mbox (0 non-ASCII); 0 added-line trailing-WS/tabs;
  0 `[OWNER`/`[TODO`/FIXME/XXX.
- [x] Builds (all out-of-tree): main + gate-split configure rc=0,
  make rc=0; main = 34 warnings (upstream baseline), gate-split
  = 2 (minimal enables); 0 ac4 in both. Suite mirror untouched
  (no GEN step -- ref written from captured decode output).
- [x] D1 stands: AV_CODEC_CAP_EXPERIMENTAL still wired
  (`ac4dec.c:879`), `-strict -2` kept on all 10 decode CMDs.
- [x] Nothing sent -- no SENT marker; suite +24h clock pending.

## Disclosed carries (non-blocking for this series)

- fate-source FIXED (was: B-final sole-fail, A-owned, escalated).
  Cover `COVER-B.txt` Testing block (`full make fate green`) is now
  literally true -- cover track to re-verify (cover prose untouched
  by this seat; subjects re-verified 1:1).
- N5 re-pin rule DISCHARGED: the 714 golden is recorded on the
  post-N5 core, and the other 8 goldens pass 14/14 on post-N5
  bytes -- no fri 0-12 golden can go stale from an N5 landing
  anymore (N5 already landed in A).
- crc-note asserts the skip warning, not a reject count (wrap has
  no CRC check); the same warning fires on the clean stereo clip.
  Pinned honestly as the skip, never as discrimination.
- 714 golden pins 48000 Hz / 1920 spf N5 output (3 frames, no
  skips). Pre-N5 51200 bytes are gone from pack + tree (mechanical
  grep).
- C05/E2-2/D4-D6 append-rebase still theirs (D5).

## Owner-at-post (fill when sending)

- [ ] A + A0 LANDED confirm (rebase the 2 patches onto landed A
  at post if the tree moved -- P6).
- [ ] +24h clock-end confirm (>=2026-10-17T22:23:09Z).
- [ ] Cover Testing re-verify landed (cover track).
- [ ] Message-IDs + date recorded here.
