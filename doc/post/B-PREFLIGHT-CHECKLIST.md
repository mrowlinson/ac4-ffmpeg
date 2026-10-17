# B-PREFLIGHT-CHECKLIST -- Series B doc + decoder-FATE FINAL re-cut (owner posts)

Branch: `ff/doc-B-post-final` from master `df3f2de`.
Gated 2026-10-16 in scratch `ff-B-recut-final` (TMPDIR there);
parent repo untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure: `doc/post/B-OWNER-SEND-STEPS.md`. Stage background
(deltas, stanza parity, suite history):
`doc/post/B-POST-CHECKLIST.md` (`d731060` STAGED pack -- never post).

## Suite-landed gate P1

- [x] Live re-verify in this scratch: `rsync
  rsync://fate-suite.ffmpeg.org/fate-suite/ac4/` rc=0 +
  `sha256sum -c EXPECTED.sha256` -> 12/12 OK (own mirror,
  first-party on top of `SUITE-LANDED.md` 22:23:09Z).
  clip-714 accepted (12/12, no drops).
- [ ] +24h clock ends 2026-10-17T22:23:09Z -- OWNER-AT-POST confirm.

## Re-cut assembly (this seat)

- [x] Staging stack: pristine `3e6cd5e` + `series-A-decoder.mbox`
  (`b9c90fd`, `a93e34e`) + `series-A0-head.mbox` (`c275ad7`), each
  `git am --whitespace=error` rc=0. B commits `0a49864` (1/2 doc,
  content-identical to stage -- options frozen 6/6, verified vs
  the `ac4dec.c` AVOption table) + `42d53d9` (2/2 fate, 11 files,
  248 insertions).
- [x] F8 fixed: 9 refs renamed prefix-less `tests/ref/fate/ac4-*`
  (default-ref strip rule `tests/fate-run.sh:12-17` proven;
  matches the A0/C/E landed convention); crc-note REF `crc1` ->
  `skipping bad packet` (no `crc` string exists in wrapper
  stderr; actual wording `skipping bad packet #2 (341 bytes):
  bad packet`, stable prefix pinned).
- [x] P3 folded: F1 wrap 2-frame accept (golden follows the
  WRAPPER per re-record rule; bytes == stereo decode -- wrap
  performs no CRC check: AC40 re-frame); F1b wording above;
  F2 rc=69 + `no-sync: lone frame ... input truncated`; F3
  rc=234 + `Output file does not contain any stream` re-confirmed
  on the `-c copy` path with the `sample format` (not rate) shape.
- [x] NEW finding folded (not in stage/combo): ims120 default pres
  is v1-fatal at packet 17 (mid-stream config change -- clip
  reality: a distinct 16-packet 765/766 B region inside 57
  packets; demux emits all 57 sane). Pres 1 skips that region
  with warnings and decodes 41 stereo frames, exit 0, matching
  the manifest "41 frames real pcm". Test pins pres 1 via
  `-pres_sel 1` with an honest mak comment (pres-0 fatal is
  v1-by-design one-shot-init D6, not a decoder bug for B scope).
- [x] Mbox `doc/post/series-B-fate.mbox` 19113 B, 14 diffs (3 +
  11); STAGED caveat dropped (final form); 2/2 msg carries the
  ims120 pres-1 note; zero PENDING stubs anywhere (mechanical
  grep over mbox + tree).

## Final gates (this seat, all green)

- [x] `git am` clean on pristine `3e6cd5e` -- rc=0, commits
  `9b13b5b` (1/2) + `ae2b708` (2/2), zero whitespace errors;
  also clean on the A0 stack (neg2 tree). 13/13 B files
  `cmp`-SAME am-tree vs staging; 2/2 filesets + namesets
  identical; `git show --check` clean on both commits.
- [x] `make fate-ac4` 14/14 rc=0 on the live suite + second run
  rc=0 (goldens deterministic); no-SAMPLES run gives the
  standard `requires external samples` refusal.
- [x] Gate-split `--disable-everything` minimal-enables build
  rc=0 (decoder/demuxer/muxer ac4, pcm_s16le enc + muxer,
  rawvideo enc, framemd5/framecrc muxers, aresample, file+pipe)
  + `fate-ac4` 14/14 rc=0 -- F6 split gates proven.
- [x] Pre-patch negatives (direct commands + make-parse):
  pristine+B `make fate-ac4` fails at parse (no
  CONFIG_AC4_DECODER); per-leg: 10 decode legs rc=234/8
  (`no decoder found`, ims120 `Unrecognized option 'pres_sel'`),
  3 copy legs rc=234 unset-rate (A0 fill absent), garbage rc=234
  with the WRONG (rate-family) message. A0+B without decoder:
  3 copy goldens BYTE-MATCH, garbage rc=234+pin, decode legs
  rc=234 no-decoder. Decode legs = decoder-dependent, remux
  legs = A0-dependent and decoder-independent -- exactly designed.
- [x] Full self-contained `make fate`: 3054 TESTs, SOLE failure
  `fate-source` (12 Series-A headers lack conforming inclusion
  guards -- pre-existing, A-owned; this series adds no C sources
  or headers, proven by construction). Zero other failures;
  zero ac4 tests in the subset (all 14 correctly extern-gated).
  Literal full-fate-green is blocked behind A (escalated).
- [x] Subjects 1:1 cover<->mbox (`doc: AC-4 decoder docs`,
  `fate: add ac4 decoder tests`, each 1x/1x, mechanical);
  signoffs 2/2 `Michael Rowlinson <mrowlinson@gmail.com>`;
  ASCII-only mbox (perl scan, 0 non-ASCII); 0 added-line
  trailing-WS/tabs; 0 `[OWNER`/`[TODO`/FIXME/XXX.
- [x] Builds (all out-of-tree): main + gate-split + neg1 + neg2
  configure rc=0, make rc=0; main/neg1/neg2 = 34 warnings
  (upstream baseline), gate-split = 2 (minimal enables);
  0 ac4 in all four. Suite mirror untouched by GEN (`diff -r`
  vs backup).
- [x] D1 stands: AV_CODEC_CAP_EXPERIMENTAL still wired
  (`ac4dec.c:858`), `-strict -2` kept on all 10 decode CMDs.
- [x] Nothing sent -- no SENT marker; suite +24h clock pending.

## Disclosed carries (non-blocking for this series)

- fate-source (Series-A header guards) fails upstream CI on A as
  well as here -- ESCALATED to parent/A lane. Cover `COVER-B.txt`
  Testing block (`full make fate green`) needs a cover-track
  refresh for this finding before send (cover prose untouched by
  this seat; subjects re-verified 1:1).
- N5 re-pin rule still binds fri 0-12 goldens IF N5fix lands
  between record and post. Recorded from the pre-N5fix A-post-r2
  core bytes; A lands without N5fix per current packs, so no
  staleness at send time barring an N5fix-first landing.
- crc-note asserts the skip warning, not a reject count (wrap has
  no CRC check); the same warning fires on the clean stereo clip.
  Pinned honestly as the skip, never as discrimination.
- 714 golden pins 51200 Hz clip reality (3 frames, no skips).
- Combo-6 correction (record only): its `dec-*.md5` were 4-line
  head-truncated headers (Broken-pipe stderr), not full-decode
  proof -- ims120's pres-0 fatal was masked by stage-state there.
  Full-decode proof is this seat's fate run.
- C05/E2-2/D4-D6 append-rebase still theirs (D5).

## Owner-at-post (fill when sending)

- [ ] A + A0 LANDED confirm (rebase the 2 patches onto landed A
  at post if the tree moved -- P6).
- [ ] +24h clock-end confirm (>=2026-10-17T22:23:09Z).
- [ ] Cover Testing refresh landed (cover track).
- [ ] Message-IDs + date recorded here.
