# B-POST-CHECKLIST -- Series B doc + decoder-FATE STAGE (post at clock end)

Branch: `ff/doc-B-series-stage` from master `cc42e2c`.
Staged 2026-10-16 in scratch `ff-B-series-stage` (TMPDIR there);
parent repo untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure (clock end): `doc/post/B-OWNER-SEND-STEPS.md`.

## Suite-landed gate (PENDING -- the post blocker)

- [ ] Suite shows all 12 `ac4/clip-*` + `garbage.ac4` files, sha256
  match `ff-suite-watch/EXPECTED.sha256`, live +24h.
  State at stage time: Round 1 PENDING (0/12 present, 0/12 sha-ok,
  2026-10-16T18:19:56Z; remote `ac4/` holds only 4 pre-existing
  files, 0 mailed clips). 24h clock runs from suite-live confirm.

## Pack contents (this branch)

- `doc/post/series-B-decfate.mbox` -- 2 patches vs `3e6cd5e`
  (13067 B): 1/2 doc, 2/2 fate. STAGED (goldens are PENDING
  stubs; re-cut at clock end, never post as-is).
- `doc/covers/COVER-B.txt` -- post-ready cover draft (subjects 1:1
  with the staged mbox; re-verify at re-cut).
- `doc/post/B-POST-CHECKLIST.md` -- this file.
- `doc/post/B-OWNER-SEND-STEPS.md` -- clock-end record + send steps.

## Staged deltas vs fate-b2/ac4.mak v3n (all evidence-driven)

- D1 `-strict -2` on all 10 decode CMDs (pcm + framemd5). REQUIRED:
  Series A wires AV_CODEC_CAP_EXPERIMENTAL, fate-run passes no
  strict flag, and B2EXEC-REAL proves every row fails without it.
  Precedent: fate/acodec.mak DCA `-strict -2`. Input-side placement
  mirrors the proven runner (`-strict -2 -i`). REMUX `-c copy`
  tests carry no flag (no decode). If Series A drops EXPERIMENTAL
  before post, strip the flags at re-cut.
- D2 mono + gap-fails are comment-only deferred slots (no stanzas).
  No public mono AC-4 or codec-4 bytes exist anywhere (REQUEST.txt
  "Their tests stay TBD"); a stanza referencing a nonexistent
  suite file can never pass upstream CI.
- D3 9 in-tree goldens are PENDING stubs (unmistakable 2-line
  text, never faked bytes): 51, ims120, 714, stereo51, delay,
  crc-bad, copy-stereo, copy-51, seek-eof. Recorded from the real
  wrapper + suite bytes at clock end per the re-record rule.
- D4 in-tree comments (105-line mak): sample shape + test intent
  only. Staging metadata (UNVERIFIED, owner tracks, changelogs,
  parser block, BYTES CAVEATs) stays in fate-b2/, not in-tree.
- D5 B2 creates `tests/fate/ac4.mak` + the Makefile include (line
  157, between ac3/adpcm). No ac4.mak exists at `3e6cd5e`; A/A0
  touch disjoint paths (lavc-only / demux.mak+seek.mak). C05 and
  E2/2 currently create the same file -> THEY rebase to append at
  their time (B lands first; disclosed follow-ups, not this pack).
- D6 Changelog APPENDS `- native AC-4 audio decoder` at the
  `<next>` chapter end (oldest->youngest; 9552764/cc234e7
  precedent). No textual conflict with C3's prepended encoder
  line (different lines, opposite ends).

## Gates run this stage (all green)

- [x] `git am` clean on pristine `3e6cd5e` -- rc=0, commits
  `02bc75b` (1/2, 3 files) + `af19c5e` (2/2, 12 files), tree
  clean, zero whitespace warnings, am tree byte-identical to the
  staging tree (5/5 files cmp SAME + 9/9 refs SAME).
- [x] Subjects 1:1 cover<->mbox -- `doc: AC-4 decoder docs` and
  `fate: add ac4 decoder tests` each once in COVER-B.txt and once
  in the mbox (mechanical counts).
- [x] Paths/names match suite conventions -- all 14 live stanzas
  use `$(TARGET_SAMPLES)/ac4/*.ac4` inputs + `$(SAMPLES)/ac4/*.pcm`
  pcm refs (ac3.mak shape); all 11 referenced suite names exist
  in the mailed 15-file set (12th, clip-chanbased, is the
  unreferenced D4 alternate); `fate-ac4:` aggregate single (count
  1); PCM/FRAMEMD5/REMUX gates + grep/`test $$?` idioms verified
  against tests/Makefile + fate-run.sh + astc.mak:142-152.
- [x] Stanza parity 16/16 vs staging v3n -- 14 live + mono and
  gap-fails deferred-as-comments (each named once, no CMD).
- [x] Doc options frozen 6/6 -- pres_sel, drc, drc_out_ref,
  drc_boost, drc_cut, de_level match lavc-dec/ac4dec.c @ master
  (names, defaults, ranges); table row after AC-3; no table-row
  conflict (C04 adds encode-side at the same spot -> C rebases).
- [x] Hygiene -- ASCII-only mbox; 0 added-line trailing-WS/tabs;
  zero `[OWNER`/`[TODO`/FIXME/XXX placeholders; signoffs
  `Michael Rowlinson <mrowlinson@gmail.com>` on both patches.
- [x] Nothing sent -- no SENT marker added; suite PENDING.

## PENDING at clock end (record, re-cut, re-verify, then post)

- [ ] P1 suite-landed gate above (12/12 sha-ok + 24h).
- [ ] P2 record 9 goldens from the real wrapper + suite bytes on
  the A+A0 stack; replace stubs; golden bytes follow the WRAPPER
  (re-record rule), never the staged lib pins.
- [ ] P3 B2EXEC-REAL wrap calls resolved + folded:
  F1 crc-bad shape (wrap 2-frame stereo-identical vs shim 1-frame
  drop) decides the crc-bad golden; F1b crc1 wording (absent in
  wrapper stderr today) decides the -note REF; F2 gap-trunc
  (wrap rc=0 silent zeros vs shim rc=3 no-sync) decides the
  nonzero/no-sync assert; F3 garbage-note re-confirm on `-c copy`
  (F3 observed plain-decode, a different path).
- [ ] P4 `make fate-ac4` green on the re-cut tree (all 14 live),
  full `make fate` green, pre-patch negative (fail without A),
  `--disable-everything` gate-split run.
- [ ] P5 re-cut mbox + re-verify (am-clean, subjects 1:1, no
  PENDING stubs left, cover finalized) then post per send steps.
- [ ] P6 Series A landed first (B rides the A tail; rebase the
  2 patches onto landed A at post if the tree moved).

## Disclosed carries (non-blocking)

- Staged 2/2 commit message carries a STAGED caveat line; the
  re-cut commit message drops it (final form).
- C05/E2-2/D4-D6 ac4.mak follow-ups (D5) belong to those series.
- N5 rate mislabel rides Series A; B goldens are recorded, not
  carried, so nothing here goes stale (re-pin rule still binds
  fri 0-12 goldens if N5fix lands between record and post).

## Verdict

**READY-AT-CLOCK-END** -- stage complete; post after P1-P6.
