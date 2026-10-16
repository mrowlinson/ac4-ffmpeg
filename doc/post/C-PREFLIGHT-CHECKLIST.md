# C-PREFLIGHT-CHECKLIST — Series C encoder FINAL (owner posts)

Branch: `ff/doc-C-post-final` from master `f827e56`.
Gated 2026-10-16 in scratch `ff-C-post-pack` (TMPDIR there);
parent repo untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure: `doc/post/C-OWNER-SEND-STEPS.md`.

## Source line

- [x] `ac4/C-encoder-split-r4@1ccd10a` — 11 commits, linear, no merges
  (`282ac3b/acc1bb8/0b3f890/3631fcf/b3f3a1f/38a19be/f36f407/
  f07df7f/e6f99ec/e017d9b/1ccd10a`, matches runbook pins).
- [x] Mbox is a fresh `format-patch` of that line, byte-identical to
  the gated `submit-fix/C/0001..0011` (11/11 `cmp` clean).

## Final gates (this wave, all green)

- [x] `git am` clean on pristine `3e6cd5e` — rc=0, 11/11 applied
  (`48d44e9` 01/11 7 files, `d7f44aa` 02/11 28 files, `90fcb42`
  03/11 7 files, `1922eac` 04/11 2 files, `e36b545` 05/11 4 files,
  `0ba255e` 06/11 1 line, `9549d0f` 07/11 1 file, `3a6850f` 08/11
  1 file, `bf8ca21` 09/11 2 files, `8ec07e5` 10/11 10 files,
  `dca6da2` 11/11 1 file). Applied tree == r4 tip tree minus
  exactly A0's 6 files (735 deletions, zero additions — zero A0/C
  file overlap, standalone post proven).
- [x] `git show --check` zero on all 11 (no whitespace errors).
- [x] Subjects 1:1 cover↔mbox — all 11 distinctive subject strings
  appear exactly once in `COVER-C.txt` and once in the mbox
  (mechanical counts 1/1 x11).
- [x] Zero placeholders — zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in the cover; mbox's single XXX-pattern hit is the
  `mktemp ac4neg.XXXXXX` template in `tests/ac4-neg-check.sh`
  (08/11, required mktemp syntax, NOT a placeholder).
- [x] Signoffs — `Michael Rowlinson <mrowlinson@gmail.com>` on all
  11 patches (11/11 `Signed-off-by` lines).
- [x] Goldens pinned — stereo `86828e84..` (98193 B) x2 and 5.1
  `53a44aca..` x2 in mbox patch 05/11 refs; cover carries 4 golden
  refs + `#extradata` stereo `0xb7a30a0b`, fold8 `0xbafa0ae9`.
- [x] ASCII-only — zero non-ASCII bytes in the mbox; expect zero
  `send-email` 8bit warnings.
- [x] No-drift — `COVER-C.txt` byte-identical to master (r4 cover +
  filled signoff line); mbox is new, cut from the pinned r4 tip.
- [x] Nothing sent yet — no SENT marker in repo or mail-combined;
  only blank fill-in lines; all series unsent at gate time.
- [x] Checklists complete — this file + `C-OWNER-SEND-STEPS.md`
  (devel address, order, no-attachments rule) written and committed.

## Carried gates (identical bytes, still stand)

- submit-fix: full 20-stack `git am` 20/20, tree-diff EMPTY vs tip;
  configure + `make` exit 0, zero ac4 warnings; `fate-ac4` 10/10 +
  aliases + neg gate; `.text` IDENT 25/26 (sole diff = movenc.o
  cold-path immediates, outside this series).
- MBP-R4BUILD: 20/20 `git am` on MBP, TREE-MATCH, build clean,
  `fate-ac4` 10/10 incl. stereo `86828e84..` + 5.1 `53a44aca..`
  goldens compared and passing, neg 2/2 PASS.

## Disclosed carries (reviewer-visible, non-blocking)

- Generator wide rows (172: huffman 129 + aspx 43) kept verbatim
  per FIX-REPORT justification (upstream accepts
  generator-emitted table rows; rewrapping desyncs generators).
- Cover `Branch state (draft, unposted)` line names the r4 tip +
  hashes: accurate at gate time; branch names never appear in
  patch bodies.
- In-history C stacks on A0 (`282ac3b` parent `fc4866b`): post is
  standalone regardless (zero file overlap, am-clean on pristine
  base, tree-diff above).

## Owner-at-post (fill when sending)

- [ ] Sent date + Message-IDs (0/11 .. 11/11): _______________
- [ ] Archive shows 0/11 + 01/11 .. 11/11 threaded in order.
- [ ] Mail-1 (separate, `samples-request@ffmpeg.org` + 15
  attachments) sent date: _______________ (starts no clock for C).

## Verdict

**READY-TO-POST** — Series C encoder send pack final.
