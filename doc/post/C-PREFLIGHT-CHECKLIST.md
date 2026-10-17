# C-PREFLIGHT-CHECKLIST — Series C encoder r5 (owner posts)

Branch: `ff/doc-C-post-r2` from master `240dad8`.
Gated 2026-10-16 in scratch `ff-C-post-r2-retry` (TMPDIR there);
parent repo untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure: `doc/post/C-OWNER-SEND-STEPS.md`.

Re-cut driver: combo-6 finding L1 (BLOCKER): Series A
`huffman_tab.o` + Series C `ac4_huffman_tables.o` both define the
same 34 AJOC/DE/DRC huffman globals (identical bytes) -> 34
duplicate symbols at A+C link. Fix, folded into 01/11 here:
`AC4_`-prefix the 34 C-side symbols (definitions + registry uses +
header decls; string-literal book names kept). Decoder untouched.
Source: `doc/post/COMBO-RERUN-6.md` + fix ref commit `e34ba44` in
scratch `ff-combo-6-verify/wt`.

## Source line

- [x] `ac4/C-encoder-split-r5@fd578ca` — 11 commits, linear, no merges
  (`6c07969/065c5e5/2cd9378/bd7a122/1985788/c8c74e1/
  1f76f88/1ef16bd/6a4733d/383a1e9/fd578ca`, in scratch
  `ff-C-post-r2-retry/ffmpeg`; r4 `1ccd10a` kept intact in
  `ff-submit-fix/ffmpeg`).
- [x] Mbox is a fresh `format-patch` of that line (11/11, 653799 B);
  02/11..11/11 byte-identical to the gated r4
  `submit-fix/C/0002..0011` (modulo From-SHA); r5-vs-r4 delta ==
  the combo-6 `e34ba44` L1 delta exactly (2 files, 85/85).

## Final gates (this wave, all green)

- [x] `git am` clean on pristine `3e6cd5e` — rc=0, 11/11 applied
  (`db60632` 01/11 7 files, `02de5a6` 02/11 28 files, `d5968f6`
  03/11 7 files, `b8e0f28` 04/11 2 files, `f2c2537` 05/11 4 files,
  `56f1933` 06/11 1 file, `b04bdbf` 07/11 1 file, `bd3726b`
  08/11 1 file, `b44b8ff` 09/11 2 files, `c0ce3cc` 10/11 10 files,
  `968e116` 11/11 1 file). Applied tree == r5 tip tree minus
  exactly A0's 6 files (735 deletions, zero additions — zero A0/C
  file overlap, standalone post proven).
- [x] A+C link (L1 closed): A mbox 2/2 plain + new C (C1/C2 `-3`
  auto, C3 `-3` + hand allcodecs dec+enc adjacency per combo-6,
  C4-C11 plain) assembles clean, zero markers; out-of-tree
  `configure --disable-doc --disable-debug` + `make -j18` exit 0,
  34 warnings (baseline) / 0 ac4 / ZERO duplicate symbols.
- [x] Encoder fate 5/5 on the A+C tree — rc=0, zero failure
  markers: `fate-ac4-encode` (md5 `86828e84..`), `-encode-51`
  (md5 `53a44aca..`), `-framecrc`, `-tail`, `-fold8` (goldens
  matched — rename byte-neutral). Neg gate `ac4-neg: 2/2 PASS`.
  Codec registers `DEA.L. ac4`.
- [x] `git show --check` zero on all 11 (no whitespace errors).
- [x] Subjects 1:1 cover<->mbox — all 11 distinctive subject strings
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
- [x] Cover current — `COVER-C.txt` carries the r5 01/11 note +
  r5 branch-state (`fd578ca`, 11 hashes); no other cover changes.
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
- combo-6: 24-patch A+A0+B+C+E+D assembly record
  (`COMBO-RERUN-6.md`); r5 folds its L1 resolution into C source
  (this wave re-proves A+C link + C goldens on the folded bytes).

## Disclosed carries (reviewer-visible, non-blocking)

- Generator wide rows (172: huffman 129 + aspx 43) kept verbatim
  per FIX-REPORT justification (upstream accepts
  generator-emitted table rows; rewrapping desyncs generators).
- Cover `Branch state (draft, unposted)` line names the r5 tip +
  hashes: accurate at gate time; branch names never appear in
  patch bodies.
- In-history C stacks on A0 (`6c07969` parent `fc4866b`): post is
  standalone regardless (zero file overlap, am-clean on pristine
  base, tree-diff above).
- A-context re-cut notes F2-F6 in `COMBO-RERUN-6.md` (Makefile /
  allcodecs / general-contents / mak anchors) describe merge-time
  assembly on a landed A, NOT this standalone post (vs `3e6cd5e`).

## Owner-at-post (fill when sending)

- [ ] Sent date + Message-IDs (0/11 .. 11/11): _______________
- [ ] Archive shows 0/11 + 01/11 .. 11/11 threaded in order.
- [ ] Mail-1 (separate, `samples-request@ffmpeg.org` + 15
  attachments) sent date: _______________ (starts no clock for C).

## Verdict

**READY-TO-POST** — Series C encoder send pack r5 (L1 fixed).
