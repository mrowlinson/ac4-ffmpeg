# COVER-SIZE-SWEEP — doc size/count claims vs merged mbox bytes

> SCOPE NOTE (R2 sweep `COVER-SIZE-SWEEP-R2.md`): R1 record at
> master `224ec0b`. Mboxes B/C/D/E re-cut by `20bb917`
> (now 40729/650794/163076/12774 B; A0/A unchanged at
> 43263/507373 B). Numbers below describe R1 bytes, kept as
> gated evidence; current record is `COVER-SIZE-SWEEP-R2.md`.

Seat `ff-cover-size-sweep`, 2026-10-17. Base master `224ec0b`.
Method: `wc -c` / `wc -l`, `git mailsplit`, `grep -c` subjects /
signoffs / From lines, `git apply --stat/--numstat`, perl
non-ASCII sweep — all run locally in this seat against the
worktree bytes. Zero Dolby binaries (mechanical doc-vs-bytes check).

Path notes: `doc/post/COVERS-*` does not exist; covers live at
`doc/covers/COVER-*.txt`. `SUBMIT-PLAN-V2` is not in the tree
(scratch `ff-submit-v2/` only); v1 in scratch `ff-submit-plan/`.

## Actuals (this seat, master 224ec0b)

| mbox | bytes | lines | patches (mailsplit) | signoffs | From `Michael Rowlinson <mrowlinson@gmail.com>` |
|---|---|---|---|---|---|
| series-A-decoder.mbox | 507373 | 12844 | 2 (0001 0002) | 2 | 2 |
| series-A0-head.mbox | 43263 | 1212 | 1 (0001) | 1 | 1 |
| series-B-fate.mbox | 40708 | 771 | 2 (0001 0002) | 2 | 2 |
| series-C-encoder.mbox | 650974 | 18403 | 10 (0001..0010) | 10 | 10 |
| series-D-mp4.mbox | 162643 | 4399 | 6 (0001..0006) | 6 | 6 |
| series-E-ts.mbox | 12693 | 304 | 2 (0001 0002) | 2 | 2 |
| total | 1417654 | 37933 | 23 | 23 | 23 |

Hygiene: non-ASCII bytes 0 across all mboxes + covers (perl).
TAB lines: A 0, A0 0, B 6, C 0, D 6, E 6 — all leading
make-recipe indentation (`+<TAB>$(M)...`), ASCII.
Per-patch files (`diff --git` count): A 24+2; A0 7; B 3+13
(11 golden refs); C 7/28/6/2/3/1/1/2/10/1; D 6/1/2/3/4/3;
E 2/3. Subjects 1:1 cover↔mbox on all 23 (D 5/6 matches on
first-line portion: cover wraps trailing `test`, disclosed in
D-PREFLIGHT). 29 wire = 23 patches + 6 covers (2+3+3+11+3+7).

Status key: MATCH = claim equals bytes as found. FIXED = was
mismatch, corrected in place (post-fix line cited). SCOPED =
frozen record of older bytes; scope note added at file head,
historical numbers kept as gated evidence.

## Covers (no edits; all MATCH)

| file+line | claim | actual | status |
|---|---|---|---|
| COVER-A.txt L1,L12,L20 | 0/2, 1/2, 2/2 subjects | identical strings, 1:1 | MATCH |
| COVER-A0.txt L1 | single `[PATCH] lavf/ac4...` | identical, 1:1 | MATCH |
| COVER-B.txt L1,L10,L15 | 0/2, 1/2, 2/2 subjects | identical, 1:1 | MATCH |
| COVER-B.txt L33 | `incl. fate-source` line | present (SEND-SITTING check target) | MATCH |
| COVER-C.txt L1,L18-104 | 0/10, 01/10..10/10 subjects | identical, 1:1 x10 | MATCH |
| COVER-C.txt L12 | old 08/11 gate dropped | 10 mails, no C08 | MATCH |
| COVER-D.txt L1,L14-52 | 0/6, 1/6..6/6 subjects | identical (5/6 wraps in cover) | MATCH |
| COVER-D.txt L47 | 5/6 "4 loud negatives" | mbox 0005: "Four negatives", "4 shapes + 4 negatives" | MATCH |
| COVER-E.txt L1,L12,L20 | 0/2, 1/2, 2/2 subjects | identical, 1:1 | MATCH |

## Send steps

| file+line | claim | actual | status |
|---|---|---|---|
| A-OWNER-SEND-STEPS L13-14 | pack 2 patches, 507373 B | 507373 B, 2 | MATCH |
| A-OWNER-SEND-STEPS L6 | supersedes 620888-B r5 (STALE) | historical, explicit | MATCH |
| A-OWNER-SEND-STEPS L36,L48-53 | mailsplit 0001 0002; 1/2 24 files; 2/2 ac4dec+allcodecs | 24+2 files, names match | MATCH |
| A0-OWNER-SEND-STEPS L11 | pack 1 patch, 43263 B | 43263 B, 1 | MATCH |
| A0-OWNER-SEND-STEPS L38,L47-57 | mailsplit 0001; 7 files +678/+290/+4/+2 + 3 refs | numstat identical | MATCH |
| A0-OWNER-SEND-STEPS L78 | seq ... B -> C(10) -> E(2) -> D(6) | C=10, one B mbox | FIXED (was B1/B2, C(11)) |
| B-OWNER-SEND-STEPS L15-16 | pack 2 patches, 40708 B | 40708 B, 2 | MATCH |
| B-OWNER-SEND-STEPS L51,L60-64 | mailsplit 0001 0002; 2/2 mak+include+11 goldens | 13 files: mak+Makefile+11 refs | FIXED (was 9 goldens) |
| B-OWNER-SEND-STEPS L73 | seq B -> C(10) -> E(2) -> D(6) | C=10 | FIXED (was B1/B2, C(11)) |
| C-OWNER-SEND-STEPS L5,L12-13 | 10 mails; pack 10, 650974 B | 650974 B, 10 | MATCH |
| C-OWNER-SEND-STEPS L18-20 | 10 commits; am 10/10 | count 10; gate record | MATCH |
| C-OWNER-SEND-STEPS L40,L50-76 | mailsplit 0001..0010; files 7/28/6/(2)/3/1/1/2/10/1 | diff counts identical; 06/10 +1/-1 | MATCH |
| C-OWNER-SEND-STEPS L78 | "No 08/11-style negatives gate" | historical ref, explicit | MATCH |
| C-OWNER-SEND-STEPS L100-101 | seq C(10) -> E(2) -> D(6) | 10/2/6 | MATCH |
| D-OWNER-SEND-STEPS L9 | pack 6 patches, 162643 B, union | 162643 B, 6 | MATCH |
| D-OWNER-SEND-STEPS L15-20 | union re-cut; r3 staging 128977 B superseded | 162643 != 128977 | FIXED (was "byte-identical to submit-fix") |
| D-OWNER-SEND-STEPS L48-52 | 1/6: 6 files, parser.c +420, toc.h +678, +1s, Changelog | numstat identical | FIXED (was 4 files, +826) |
| D-OWNER-SEND-STEPS L53 | 2/6: 1 file, mov.c +47 | +47, 1 file | MATCH |
| D-OWNER-SEND-STEPS L58-61 | 3/6: 2 files, movenc +141, Changelog +1 | numstat identical | FIXED (was 1 file, +97) |
| D-OWNER-SEND-STEPS L62-65 | 4/6: 3 files, test +1007, mak +12/-1 | numstat identical | FIXED (was +871, +13/-1) |
| D-OWNER-SEND-STEPS L66-71 | 5/6: 4 files, test +708, mak +35/-2, ref +150, 4 negs | numstat identical; "Four negatives" | FIXED (was 3 files, +691, +14/-2, 3 negs) |
| D-OWNER-SEND-STEPS L72-75 | 6/6: 3 files, +691/+2, mak +12/-2, 2 negs | numstat identical | FIXED (was mak +13/-2) |
| D-OWNER-SEND-STEPS L77-79 | 6 TAB lines (make-recipe, ASCII) | 6 leading-TAB recipe lines | FIXED (was "zero TAB lines") |
| D-OWNER-SEND-STEPS L97 | seq B -> C(10) -> E(2) -> D(6) | C=10 | FIXED (was B1/B2, C(11)) |
| E-OWNER-SEND-STEPS L9 | pack 2 patches, 12693 B | 12693 B, 2 | MATCH |
| E-OWNER-SEND-STEPS L15-20 | stack re-cut; r3 staging 11605 B superseded | 12693 != 11605 | FIXED (was "byte-identical to submit-fix") |
| E-OWNER-SEND-STEPS L46-50 | 1/2: 2 files, mpegtsenc +26, Changelog +1 | numstat identical | FIXED (was 1 file) |
| E-OWNER-SEND-STEPS L51-54 | 2/2: 3 files, mak +43/-1 | numstat identical | FIXED (was +45/-1) |
| E-OWNER-SEND-STEPS L75 | seq B -> C(10) -> E(2) -> D(6) | C=10 | FIXED (was B1/B2, C(11)) |

## Checklists

| file+line | claim | actual | status |
|---|---|---|---|
| A-PREFLIGHT L58-59 | mbox 507373 B / 12844 lines (was 620888/15226, -113515) | wc identical; 620888-507373=113515 | MATCH |
| A-PREFLIGHT L70 | round-trip same 507373 B | size matches | MATCH |
| A-PREFLIGHT L75-78 | subjects 1:1 cover-mbox | 1:1 verified | MATCH |
| A-PREFLIGHT L81-83 | From: lines 2,11930; From lines 1,11929; signoffs 32,11964 | grep -n identical | MATCH |
| A-PREFLIGHT L84 | mbox zero non-ASCII | perl 0 | MATCH |
| A-PREFLIGHT L86,L162 | single TBD at mbox line 4820 | one hit, line 4820 | MATCH |
| A-PREFLIGHT L127-130 | mailsplit 0001+0002; A0+A+B 5/5 | 2 files; 1+2+2=5 | MATCH |
| A-PREFLIGHT L167-170 | C 01/10 context quotes pre-r6 comment, still C-seat-owned | C01 context is pre-r6 wording | FIXED (was "C 01/11 ... with the M10 re-cut") |
| A0-PREFLIGHT L3-8 | SCOPE NOTE: r2 record; union r3 now 43263 B / 7 files | 43263 B, 7 files | SCOPED (L15-17 35400 B, L26 6 files describe r2) |
| A0-PREFLIGHT L22-23,L32-44 | 3/3; subjects 1:1; From L2 + 1/1 signoff; ASCII 0; TAB 0; mailsplit 1/1 | mbox L2/L55; perl 0; 0 TABs; 1 file | MATCH |
| B-PREFLIGHT L3-8 | SCOPE NOTE: re-cut-2 record; union now 40708 B / 16 diffs / 11 goldens | 40708 B; 3+13 diffs; 11 refs | SCOPED (L43 19150 B, 14 diffs describe re-cut 2) |
| B-PREFLIGHT L70-74 | subjects 1:1; signoffs 2/2; ASCII-only | verified on current bytes | MATCH |
| B-POST L3-8 | SCOPE NOTE: staged decfate removed; live B is fate-only 40708 B | no decfate file in tree | SCOPED (L18-19 13067 B staged pack) |
| C-PREFLIGHT L13,L26,L32 | 10 mails / 10 commits / 650974 B | 650974 B, 10 | MATCH |
| C-PREFLIGHT L33-34,L38-39,L42 | DAG 23 commits; A0+A+B+C 15/15; stack-am 23/23 | 1+2+2+10+2+6=23; 1+2+2+10=15 | MATCH |
| C-PREFLIGHT L67-69,L72-73 | subjects 1:1 x10; signoffs 10/10 | verified | MATCH |
| C-PREFLIGHT L74-77 | goldens stereo x2 + 5.1 x2 in 05/10; 3 ref #extradata lines | grep 2/2 in 0005; 3 ref lines in 0009 | MATCH |
| C-PREFLIGHT L83-84 | mbox zero non-ASCII | perl 0 | MATCH |
| C-PREFLIGHT L119-121 | B 2/2 M17 comment stale ("which Series C 08/11 deletes") | present at B mbox L267 | MATCH (disclosure true) |
| D-PREFLIGHT L3-12 | SCOPE NOTE: r3 record; union now 162643 B; C(10), 18 arithmetic-only; 6 TABs | verified | SCOPED (L16-18 128977 B; L22-24 C(11) 19/19; L45-47 zero-TAB describe r3) |
| D-PREFLIGHT L10,L37-44 | 6 commits; subjects 1/1 x6 (5/6 wrap disclosed); signoffs 6/6; ASCII 0 | verified | MATCH |
| E-PREFLIGHT L3-11 | SCOPE NOTE: r3 record; re-cut now 12693 B; C(10), 12 arithmetic-only | verified | SCOPED (L14-16 11605 B; L20-22 C(11) 13/13 describe r3) |
| E-PREFLIGHT L30-39 | subjects 1:1 x2; signoffs 2/2; ASCII 0; 6 TAB recipe lines | verified (6 TABs) | MATCH |
| DEC-POST L3-9 | SCOPE NOTE: re-cut-2 staging; r6 now 507373 B; stack now 23 | verified | SCOPED (L23-27 606436 B; L52 C(11)/D(5)/D6-TBD describe re-cut 2) |
| DEC-POST L58,L60 | [PATCH 0/2] + 2 patches; mail-1 11 attachments | 2 patches; mail-1 alone is 11 (combined 15) | MATCH |
| mail-combined/CHECKLIST | no mbox claims (attachment/sample sizes only) | n/a | N/A |
| mail-torture-trio/CHECKLIST | no mbox claims (attachment sizes only) | n/a | N/A |

## SEND-SITTING (no edits; all MATCH)

Pin `2def950` + "(or later parent merge)": `224ec0b` is a
descendant and all six mboxes + six covers are blob-identical
between the two (`git rev-parse` SAME x12) — sizes carry over.

| line | claim | actual | status |
|---|---|---|---|
| L29-31 | A 507373, A0 43263, B 40708, C 650974, E 12693, D 162643 | wc identical x6 | MATCH |
| L44 | grep counts 2 1 2 10 2 6 = 23 | ^From counts identical | MATCH |
| L61-62 | 23/23 (24/24 pre-drop, C08 dropped) | 23 current | MATCH |
| L86-190 | mailsplit 0001 / 0001 0002 / 0001..0010 / 0001..0006 | mailsplit identical | MATCH |
| L93-199 | thread subjects per series | identical to mbox subjects | MATCH |
| L204-205 | 29 wire (23+6: 2+3+3+11+3+7) | arithmetic holds | MATCH |
| L230-243 | dry-run record vs 2def950 bytes (= same bytes) | counts/sizes match | MATCH |

## SUBMIT-PLAN / V2 (scratch planning records; untouched)

| file | claim | actual | status |
|---|---|---|---|
| ff-submit-v2/SUBMIT-PLAN-V2.md (192 l, dated 2026-10-16) | 5 series, 15 patches + D4 optional | plan-era state, predates 6/23 stack | SUPERSEDED-PLAN |
| ff-submit-plan/SUBMIT-PLAN.md L23 | 4 series, 12 patches | plan-era state | SUPERSEDED-PLAN |

Both are dated out-of-tree planning inputs, not send docs. Counts
describe the plan era. Left frozen; this sweep is the current record.

## Out of scope (noted, untouched)

Historical/verify docs outside the sweep set keep their era bytes
by design: COMBO-RERUN-6/7 (24-patch pre-drop), ED-STILL-CLEAN
(C 11 / E 11605 / D 128977), A-R4/R5-VERIFY (A 620505/620888),
MUST-TRIAGE (`/11` refs), A-SENDSTEPS-NITFIX (606385 B),
B-NITFIX-N1 (decfate), INTEGRATE-RUNBOOK history, MUST-VERIFY,
E-VERIFY, C-R2-VERIFY, D3-PROBE, UNION-D, STACK-DAG,
PREPOST-REVIEW. Spot cross-check: COMBO-RERUN-8 stack line and
UNION-D deliverable line already match current bytes.

## Observations (not size/count; no fix)

- B mbox L267 carries stale text "which Series C 08/11 deletes"
  (bytes; disclosed in C-PREFLIGHT L119-121). Bytes immutable here
  (docs match bytes, never reverse); a future B re-cut owns it.
- C 01/10 Makefile context still quotes the pre-r6 decoder comment
  (verified verbatim). Assembly matter; corroborates C HOLD
  (MUST-VERIFY). Not a size/count claim.
- A0/B/D/E §3 drip-order prose ("after A merges" etc.) vs the
  SEND-SITTING one-sitting order. Order, not size/count; untouched.
- B send-steps "11 goldens recorded" (L30-31) already agreed with
  bytes; only the L64 threading line said 9 (fixed).

Verdict: every in-scope live claim MATCHes merged bytes at end;
frozen records carry explicit scope notes. 11 files fixed +
this sweep doc.
