# ASSEMBLY-DEBTS-R2 — MUST-final fixed mboxes + proof

Seat `ff-assembly-debts` R2 (resume). Branch `ff/doc-assembly-debts` @
`89d04d7` (rebased from `224ec0b`; delta docs-only, 15 files, 0 mbox —
`git diff 224ec0b..89d04d7 -- doc/post/series-*.mbox` EMPTY, first-hand).
Worktree mbox patch IDENTICAL pre/post rebase (`cmp` of stash vs worktree
diff). Prior seat artifacts in `scratch/ff-assembly-debts/` reused as
discovery only; every claim below re-verified first-hand this round.

## Debt table (7 mapped, 4 = 7)

| # | Spec source | Fix (mbox location) | Bytes |
|---|---|---|---|
| 1 | MUST-VERIFY union-C SEAM (A-PREFLIGHT predicted, C-seat-owned) | C01/C02/C03: refresh r5→r6 Makefile decoder-comment context | 6 lines, context-only; + mechanical hunk drift (`@@ -167→-170`, `-206→-209`, `-201→-204`) + index/From churn, zero C content change |
| 2 | MUST-VERIFY #2 + E-VERIFY M11 re-cut §1-3 | E 2/2: tail-CMD context touch + GEN `-strict -2` + 1 msg clause (`passes -strict -2 (the native encoder is EXPERIMENTAL)`) | 2 code lines + 1 msg clause |
| 3 | MUST-VERIFY D SEAM 2 (UNION-D parent seam) | D 1/6: drop `libavcodec/ac4_toc.h` create block, keep A0's shared header (A0-vs-D delta comment-only); msg rewrite | −684 lines create block (first-hand: 685 diff lines span old toc.h hunk); fixed D keeps 2 `ac4_toc.h` refs (msg + `#include`) |
| 4 = 7 | MUST-VERIFY #4 (D SEAM 3) = COMBO-RERUN-8 cut-item 4 (`RT-GEN strict`, was GEN Error 88); same T3 GEN line | D 5/6 roundtrip GEN: `-c:a ac4` → `-c:a ac4 -strict -2` (pre-fix D mbox's ONLY bare `-c:a ac4`) | 1 line |
| 5 | MUST-VERIFY #5 (D SEAM 4; proven at C-tip: enc-neg71 FAILS w/o) | B 2/2: both enc-neg CMDs `-strict -2` | 2 lines |
| 6 | MUST-VERIFY #6 + COMBO-8 G1 (7/7 PREDICTED-EXACT vs `d-repin/`) | D 1/6 carries 7 re-pins: `ac4-{copy-stereo,copy-51,frac-copy,seek-eof,demux,demux-51}` + `seek/extra-ac4` (parser keyflags `F=0x0`, true `-copyts` stamps, 695→348 clamp) | 7 ref files; no suite bytes change |

Debt 7 is not a separate fix: COMBO-8 independently derived the same
single T3 GEN line as MUST-VERIFY #4 (verified: pre-fix D mbox held
exactly 1 bare `-c:a ac4`; post-fix all 6 mboxes hold 0).

## Fixed mbox bytes (first-hand, post-rebase worktree)

| Mbox | Old B | New B | numstat +/− |
|---|---|---|---|
| series-B-fate.mbox | 40708 | 40729 | +5/−6 |
| series-C-encoder.mbox | 650974 | 650794 | +26/−35 |
| series-D-mp4.mbox | 162643 | 163076 | +386/−718 |
| series-E-ts.mbox | 12693 | 12774 | +7/−7 |
| A0 + A | unchanged | unchanged | 0 |

Hygiene (post-rebase, first-hand): 23 patches (1+2+2+10+2+6);
subjects worktree-vs-HEAD 1:1 identical; signoffs 23/23; conflict
markers 0; non-ASCII lines 0; mailsplit 23/23.

## Proof logs (`scratch/ff-assembly-debts/logs/`)

- `am-rebase.log` — fresh `3e6cd5e` tree (`ffrebase`), plain `git am`
  A0→A→B→C→E→D: 6/6 mbox rc=0, 23 commits, zero hand.
  Tree `1601eddc…` == prior `ffvalid` tree: rebase changed zero mbox
  bytes of consequence.
- `valid23-rebase.log` + `subj-wt.txt`/`subj-head.txt` — counts above.
- `configure-rebase.log` (rc=0), `make-rebase.log` (rc=0, out-of-tree
  `build/ff-assembly-debts`, 34 warning lines all pre-existing upstream
  classes, **0 ac4**).
- `fate-ac4-rebase.log` — `make fate-ac4 SAMPLES=suite`: **rc=0,
  29/29 TEST, zero fail/error** (16 suite + 13 self-contained).
- `fate-A0D-rebase.log` — demux/demux-51/seek-extra-ac4: **rc=0, 3/3**.
- Carried (prior seat, consistent, not re-run): `am-valid.log`,
  `contentdiff-{B,C,D,E}-*.txt`, `fate-ac4-{pref,valid}.log`
  (pref-tree 4+3 reds = G1 shape, now green via debt 6),
  `fate-A0D-{pref,valid}.log`, live suite 12/12.
- `worktree-post-rebase.patch` / `stash-pre-rebase.patch` — `cmp`
  identical (rebase no-op on mbox bytes).

## Venue

R2 MBP-TB probe `ssh 169.254.103.231` TIMED OUT (prior seat: OK,
`MacBook-Pro.lan`). Per venue rule this round ran LOCAL-ONLY
pure-FFmpeg: zero Dolby-binary invocation. Dolby-dependent checks:
UNRUN this round, none in scope (a)/(b)/(c).
