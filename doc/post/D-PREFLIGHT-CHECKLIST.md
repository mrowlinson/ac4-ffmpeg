# D-PREFLIGHT-CHECKLIST — Series D MP4+dac4 FINAL (owner posts)

Branch: `ff/doc-D-post-final` from master `2436f47`.
Gated 2026-10-16 in scratch `ff-D-post-pack` (TMPDIR there);
parent repo untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure: `doc/post/D-OWNER-SEND-STEPS.md`.

## Source line

- [x] `ac4/D-split-r3@08b4f00` — 6 commits, linear, no merges
  (`083c676/c01ad3f/499892d/dd69611/0747972/08b4f00`, stack
  A0'->C'->E'->D' over `3e6cd5e`, matches FIX-REPORT pins +
  COVER-D branch-state line; supersedes V2-plan D1-D4 pre-split
  names. D11 doc-only, D6 folded pre-r3, no r4 exists;
  D-cover-align verdict `ea5d22d`: mbox 0/6 right, D6 legit).
- [x] Mbox is a fresh `format-patch` of the D-only range
  (`a2485a9..08b4f00`), byte-identical to the gated
  `submit-fix/D/0001..0006` (6/6 `cmp` clean, 128977 B).

## Final gates (this wave, all green)

- [x] `git am` clean — D 1/6-3/6 alone on pristine `3e6cd5e` rc=0
  (`e9dff4f/4291035/20fe7b8`); C(11)+E(2)+D(6) sequential on
  pristine rc=0, 19/19 (tip `0258da8`: `b81125d/de0926b/491a5b0/
  81a8cce/266b29d/0258da8`). Applied tree == r3 tip tree minus
  exactly A0's 6 files (735 insertions, zero deletions — zero
  A0/CED file overlap; vs C+E only the designed `ac4.mak` +
  `tests/api/Makefile` appends in 4/6-6/6).
- [x] Posts-after-C+E proven confined — 4/6 alone on pristine fails
  `apply --check` rc=1 (`tests/fate/ac4.mak: No such file`);
  5/6 + 6/6 alone fail rc=1 the same way plus C's
  `tests/api/Makefile` context; 4/6 after C-alone fails at
  `ac4.mak:93` (needs E's extension), i.e. the C+E dependency is
  real, E-before-D is REQUIRED, and limited to those two files.
- [x] `git diff --check` zero on the D range and on the full applied
  tree (no whitespace errors).
- [x] Subjects 1:1 cover↔mbox — all six distinctive subject strings
  appear exactly once in `COVER-D.txt` and once in the mbox
  (mechanical counts 1/1 x6; 5/6 matched on its first-line
  portion, cover wraps the trailing `test` word).
- [x] Zero placeholders — zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in the cover and in the mbox (both `grep` rc=1).
- [x] Signoffs — `Michael Rowlinson <mrowlinson@gmail.com>` on all
  six patches (6/6 `Signed-off-by` lines).
- [x] ASCII-only — zero non-ASCII bytes in mbox and cover (perl
  byte count 0/0; zero TAB lines — the mak hunks are variable
  assignments, no recipes); expect zero `send-email` 8bit warnings.
- [x] No-drift — `COVER-D.txt` = r4 refresh (`0177e87`, pins r3 tip
  `08b4f00`, D 0/5->0/6) + signoff fill (`945d23e`), untouched
  since; mbox is new, cut from the pinned r3 tip.
- [x] Nothing sent yet — no SENT marker in repo or mail-combined;
  only blank fill-in lines; all series unsent at gate time.
- [x] Checklists complete — this file + `D-OWNER-SEND-STEPS.md`
  (devel address, order, no-attachments rule) written and committed.

## Carried gates (identical bytes, still stand)

- submit-fix: full 20-stack `git am` 20/20, tree-diff EMPTY vs tip;
  configure + `make` exit 0, zero ac4 warnings; `fate-ac4` 10/10
  incl pmt/probe + aliases; `.text` IDENT 25/26 (sole diff =
  movenc.o assert-line immediates from D 3/6's own added lines,
  cold paths only).
- MBP-R4BUILD: 20/20 `git am` on MBP, TREE-MATCH, build clean,
  `fate-ac4` 10/10 incl. pmt/probe passing.

## Disclosed carries (reviewer-visible, non-blocking)

- In-history D stacks on E (`083c676` parent `a2485a9`, which
  stacks on C): post is standalone vs A0 regardless (zero file
  overlap, tree-diff above); the C+E sequencing is functional
  (4/6-6/6 append to the `ac4.mak` C creates and E extends;
  5/6-6/6 need C's `tests/api/Makefile` entries) and stated in
  the cover.
- Cover `Branch state (draft, unposted)` line names the r3 tip +
  hashes: accurate at gate time; branch names never appear in
  patch bodies.
- Open items (stated in cover): exotic TOC legs (0 wire vectors,
  spec-positional); grid-rate display wart pre-decode (decoder
  latch decides, no fix here); no shared ac4_header.h in this
  series (P4 DEFER-OK, unifies after Series A lands).

## Owner-at-post (fill when sending)

- [ ] C landed AND E landed (Series D posts only after both merge): _______________
- [ ] Sent date + Message-IDs (0/6 .. 6/6): _______________
- [ ] Archive shows 0/6 + 1/6 .. 6/6 threaded in order.
- [ ] Mail-1 (separate, `samples-request@ffmpeg.org` + 15
  attachments) sent date: _______________ (starts no clock for D).

## Verdict

**READY-TO-POST** — Series D MP4+dac4 send pack final (after C+E land).
