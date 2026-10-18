# E-PREFLIGHT-CHECKLIST — Series E TS-mux FINAL (owner posts)

> SCOPE NOTE (size sweeps `COVER-SIZE-SWEEP.md` R1 +
> `COVER-SIZE-SWEEP-R2.md` R2): this file records the r3
> pre-stack-re-cut bytes (staging 11605 B; C(11)+E(2) 13/13).
> The merged mbox is the MUST-final fixed mbox (M22/M19):
> `series-E-ts.mbox` 12774 B, 2 patches (MUST-final: GEN
> `-strict -2` + message clause; ASSEMBLY-DEBTS-R2 debt 2);
> C is 10 (C08 dropped)
> so the stack arithmetic is C(10)+E(2) = 12 (arithmetic only,
> gates not re-proven here). The "identical bytes" carries below
> predate the re-cut. Size/count claims below describe r3,
> not current bytes.

Branch: `ff/doc-E-post-final` from master `f827e56`.
Gated 2026-10-16 in scratch `ff-E-post-pack` (TMPDIR there);
parent repo untouched, nothing pushed, no MBP/remote, nothing sent.
Send procedure: `doc/post/E-OWNER-SEND-STEPS.md`.

## Source line

- [x] `ac4/E-ts-r3@a2485a9` — 2 commits, linear, no merges
  (`5e2d5d5/a2485a9`, stack A0'->C'->E' over `3e6cd5e`, matches
  FIX-REPORT pins; supersedes V2-plan E1-tsdesc@5772a4e /
  E2-tsfate@39d4eab pre-recut names).
- [x] Mbox is a fresh `format-patch` of the E-only range
  (`1ccd10a..a2485a9`), byte-identical to the gated
  `submit-fix/E/0001..0002` (2/2 `cmp` clean, 11605 B).

## Final gates (this wave, all green)

- [x] `git am` clean — E 1/2 alone on pristine `3e6cd5e` rc=0
  (`63a622a`, 1 file); C(11)+E(2) sequential on pristine rc=0,
  13/13 (`b01c181` 1/2 1 file, `2b76d9a` 2/2 3 files).
  Applied tree == r3 tip tree minus exactly A0's 6 files
  (735 deletions, zero additions — zero A0/E file overlap;
  vs C only the designed `tests/fate/ac4.mak` append in 2/2).
- [x] Posts-after-C proven confined — 2/2 alone on pristine fails
  `apply --check` rc=1 (`tests/fate/ac4.mak: No such file`),
  i.e. the C dependency is real and limited to that one file.
- [x] `git diff --check` zero on both (no whitespace errors).
- [x] Subjects 1:1 cover↔mbox — both distinctive subject strings
  appear exactly once in `COVER-E.txt` and once in the mbox
  (mechanical counts 1/1 x2).
- [x] Zero placeholders — zero `[OWNER` / `[TODO` / FIXME / XXX /
  YOUR NAME in the cover and in the mbox (both `grep` rc=1).
- [x] Signoffs — `Michael Rowlinson <mrowlinson@gmail.com>` on both
  patches (2/2 `Signed-off-by` lines).
- [x] ASCII-only — zero non-ASCII bytes in mbox and cover (perl
  byte count 0/0; the 6 TAB lines in the mbox are make-recipe
  indentation, ASCII); expect zero `send-email` 8bit warnings.
- [x] No-drift — `COVER-E.txt` byte-identical to master (r3 cover +
  filled signoff line); mbox is new, cut from the pinned r3 tip.
- [x] Nothing sent yet — no SENT marker in repo or mail-combined;
  only blank fill-in lines; all series unsent at gate time.
- [x] Checklists complete — this file + `E-OWNER-SEND-STEPS.md`
  (devel address, order, no-attachments rule) written and committed.

## Carried gates (identical bytes, still stand)

- submit-fix: full 20-stack `git am` 20/20, tree-diff EMPTY vs tip;
  configure + `make` exit 0, zero ac4 warnings; `fate-ac4` 10/10
  incl pmt/probe + aliases; `.text` IDENT 25/26 (sole diff =
  movenc.o cold-path immediates, outside this series).
- MBP-R4BUILD: 20/20 `git am` on MBP, TREE-MATCH, build clean,
  `fate-ac4` 10/10 incl. pmt/probe passing.

## Disclosed carries (reviewer-visible, non-blocking)

- In-history E stacks on C (`5e2d5d5` parent `1ccd10a`, which
  stacks on A0): post is standalone vs A0 regardless (zero file
  overlap, tree-diff above); the C sequencing is functional
  (2/2 appends to C's ac4.mak) and stated in the cover.
- Cover `Branch state (draft, unposted)` line names the r3 tip +
  hashes: accurate at gate time; branch names never appear in
  patch bodies.
- No framecrc stage in 2/2 yet: demux-side sample rate/channels
  stay unknown until the Series D parser lands (stated in the
  patch message and cover).

## Owner-at-post (fill when sending)

- [ ] C landed (Series E posts only after C merges): _______________
- [ ] Sent date + Message-IDs (0/2, 1/2, 2/2): _______________
- [ ] Archive shows 0/2 + 1/2 + 2/2 threaded in order.
- [ ] Mail-1 (separate, `samples-request@ffmpeg.org` + 15
  attachments) sent date: _______________ (starts no clock for E).

## Verdict

**READY-TO-POST** — Series E TS-mux send pack final (after C lands).
