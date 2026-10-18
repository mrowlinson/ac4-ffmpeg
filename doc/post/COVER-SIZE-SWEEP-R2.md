# COVER-SIZE-SWEEP-R2 — doc size/count claims vs MUST-final mbox bytes

Seat `ff-cover-size-sweep-r2`, 2026-10-18. Base master `fc5573a`.
R1 (`COVER-SIZE-SWEEP.md`, base `224ec0b`) method re-run: `wc -c` /
`wc -l`, `git mailsplit`, `grep -c` subjects / signoffs / From
lines, `git apply --numstat`, perl non-ASCII sweep, `git rev-parse`
blob identity — all run locally in this seat against the worktree
bytes. Zero Dolby binaries (mechanical doc-vs-bytes check).
Venue probe: `ssh mrowlinson@169.254.103.231` TIMED OUT; local-only.

What changed since R1: `20bb917` re-cut B/C/D/E (MUST-final fixes
per `ASSEMBLY-DEBTS-R2.md` debts 1-6). A0/A mboxes + all six covers
are blob-identical R1→R2 (`git rev-parse` SAME x8); B/C/D/E differ.
Pin `2def950` is an ancestor of `fc5573a`.

## Actuals (this seat, master fc5573a)

| mbox | bytes | lines | patches (mailsplit) | signoffs | From `Michael Rowlinson <mrowlinson@gmail.com>` |
|---|---|---|---|---|---|
| series-A-decoder.mbox | 507373 | 12844 | 2 (0001 0002) | 2 | 2 |
| series-A0-head.mbox | 43263 | 1212 | 1 (0001) | 1 | 1 |
| series-B-fate.mbox | 40729 | 770 | 2 (0001 0002) | 2 | 2 |
| series-C-encoder.mbox | 650794 | 18394 | 10 (0001..0010) | 10 | 10 |
| series-D-mp4.mbox | 163076 | 4067 | 6 (0001..0006) | 6 | 6 |
| series-E-ts.mbox | 12774 | 304 | 2 (0001 0002) | 2 | 2 |
| total | 1418009 | 37591 | 23 | 23 | 23 |

Hygiene: non-ASCII bytes 0 across all mboxes + covers (perl).
TAB lines: A 0, A0 0, B 6, C 0, D 6, E 6 — all leading
make-recipe indentation, ASCII. B has 0 trailing-WS added lines.
Per-patch files (`diff --git` count): A 24+2; A0 7; B 3+13
(11 golden refs); C 7/28/6/2/3/1/1/2/10/1; D 12/1/2/3/4/3
(1/6: toc.h create dropped, 7 re-pins ride); E 2/3.
Subjects 1:1 cover↔mbox on 22/23 verbatim (D 5/6 matches on
first-line portion: cover wraps trailing `test`, disclosed in
D-PREFLIGHT — same wrap as R1). 29 wire = 23 patches + 6 covers
(2+3+3+11+3+7). MUST-final fixes confirmed in bytes: B enc-neg
CMDs x2 carry `-strict -2` (mbox L287/L292); 0 bare `-c:a ac4`
line-ends across all six mboxes; D 5/6 T3 GEN strict; E 2/2 GEN
strict + `passes -strict -2 (the native encoder is EXPERIMENTAL)`
clause; C 01/02/03 quote the r6 decoder comment (identical to
A-mbox L91-92); A0 creates `ac4_toc.h` +678.

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
| A-OWNER-SEND-STEPS L13-14 | pack 2 patches, 507373 B | 507373 B, 2 (blob SAME since R1) | MATCH |
| A-OWNER-SEND-STEPS L6 | supersedes 620888-B r5 (STALE) | historical, explicit | MATCH |
| A-OWNER-SEND-STEPS L36,L48-53 | mailsplit 0001 0002; 1/2 24 files; 2/2 ac4dec+allcodecs | 24+2 files, names match | MATCH |
| A0-OWNER-SEND-STEPS L11 | pack 1 patch, 43263 B | 43263 B, 1 (blob SAME since R1) | MATCH |
| A0-OWNER-SEND-STEPS L38,L47-57 | mailsplit 0001; 7 files +678/+290/+4/+2 + 3 refs | numstat identical | MATCH |
| A0-OWNER-SEND-STEPS L78 | seq ... B -> C(10) -> E(2) -> D(6) | C=10, one B mbox | MATCH |
| B-OWNER-SEND-STEPS L15-16 | pack 2 patches, 40729 B | 40729 B, 2 | FIXED (was 40708) |
| B-OWNER-SEND-STEPS L51,L60-64 | mailsplit 0001 0002; 2/2 mak+include+11 goldens | 13 files: mak+Makefile+11 refs | MATCH |
| B-OWNER-SEND-STEPS L73 | seq B -> C(10) -> E(2) -> D(6) | C=10 | MATCH |
| C-OWNER-SEND-STEPS L12-13 | pack 10, 650794 B | 650794 B, 10 | FIXED (was 650974) |
| C-OWNER-SEND-STEPS L5,L18-20 | 10 mails; 10 commits; am 10/10 | count 10; gate record | MATCH |
| C-OWNER-SEND-STEPS L40,L50-76 | mailsplit 0001..0010; files 7/28/6/2/3/1/1/2/10/1 | diff counts identical; 06 1/1; 10 ac4enc.c only | MATCH |
| C-OWNER-SEND-STEPS L78 | "No 08/11-style negatives gate" | historical ref, explicit | MATCH |
| C-OWNER-SEND-STEPS L100-101 | seq C(10) -> E(2) -> D(6) | 10/2/6 | MATCH |
| D-OWNER-SEND-STEPS L9 | pack 6 patches, 163076 B, union | 163076 B, 6 | FIXED (was 162643) |
| D-OWNER-SEND-STEPS L16-22 | union re-cut; r3 staging 128977 B superseded; MUST-final (toc-drop + 7 repins + T3 strict) | 163076 != 128977; fixes in bytes | FIXED (was size 162643, no MUST-final note) |
| D-OWNER-SEND-STEPS L51-59 | 1/6: 12 files, parser.c +420, no toc.h create, 7 repins | numstat identical (5x +1s + 420 + 1/1 x4 + 116/116 + 2/1 + 9/9) | FIXED (was 6 files, toc.h +678) |
| D-OWNER-SEND-STEPS L60-62 | 2/6: 1 file, mov.c +47 | +47, 1 file | MATCH |
| D-OWNER-SEND-STEPS L63-66 | 3/6: 2 files, movenc +141, Changelog +1 | numstat identical | MATCH |
| D-OWNER-SEND-STEPS L67-70 | 4/6: 3 files, test +1007, mak +12/-1 | numstat identical | MATCH |
| D-OWNER-SEND-STEPS L71-76 | 5/6: 4 files, test +708, mak +35/-2, ref +150, 4 negs | numstat identical; "Four negatives" | MATCH |
| D-OWNER-SEND-STEPS L77-80 | 6/6: 3 files, +691/+2, mak +12/-2, 2 negs | numstat identical | MATCH |
| D-OWNER-SEND-STEPS L82-84 | 6 TAB lines (make-recipe, ASCII) | 6 leading-TAB recipe lines | MATCH |
| D-OWNER-SEND-STEPS L102 | seq B -> C(10) -> E(2) -> D(6) | C=10 | MATCH |
| E-OWNER-SEND-STEPS L9 | pack 2 patches, 12774 B | 12774 B, 2 | FIXED (was 12693) |
| E-OWNER-SEND-STEPS L16-22 | stack re-cut; r3 staging 11605 B superseded; MUST-final GEN strict + msg clause | 12774 != 11605; fixes in bytes | FIXED (was size 12693, no MUST-final note) |
| E-OWNER-SEND-STEPS L48-53 | 1/2: 2 files, mpegtsenc +26, Changelog +1 | numstat identical | MATCH |
| E-OWNER-SEND-STEPS L54-57 | 2/2: 3 files, mak +43/-1 | numstat identical | MATCH |
| E-OWNER-SEND-STEPS L78 | seq B -> C(10) -> E(2) -> D(6) | C=10 | MATCH |

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
| A-PREFLIGHT L167-170 | C 01/10 context quotes r6 comment (MUST-final refresh, debt 1) | C01/C02/C03 quote A-mbox L91-92 verbatim | FIXED (was "pre-r6 comment") |
| A0-PREFLIGHT L3-9 | SCOPE NOTE: r2 record; union r3 now 43263 B / 7 files | 43263 B, 7 files (blob SAME) | MATCH (scope note current) |
| A0-PREFLIGHT L22-23,L32-44 | 3/3; subjects 1:1; From L2 + 1/1 signoff; ASCII 0; TAB 0; mailsplit 1/1 | holds on current bytes | MATCH |
| B-PREFLIGHT L3-9 | SCOPE NOTE: re-cut-2 record; MUST-final now 40729 B / 16 diffs / 11 goldens | 40729 B; 3+13 diffs; 11 refs | FIXED head (was 40708); body SCOPED |
| B-PREFLIGHT L70-74 | subjects 1:1; signoffs 2/2; ASCII-only | verified on current bytes | MATCH |
| B-POST L3-7 | SCOPE NOTE: staged decfate removed; live B is fate-only 40729 B | no decfate file in tree | FIXED head (was 40708); body SCOPED |
| C-PREFLIGHT L13,L26,L32 | 10 mails / 10 commits / 650794 B | 650794 B, 10 | FIXED (was 650974) |
| C-PREFLIGHT L33-34,L38-39,L42 | DAG 23 commits; A0+A+B+C 15/15; stack-am 23/23 | 1+2+2+10+2+6=23; 1+2+2+10=15 | MATCH |
| C-PREFLIGHT L67-69,L72-73 | subjects 1:1 x10; signoffs 10/10 | verified | MATCH |
| C-PREFLIGHT L74-77 | goldens stereo x2 + 5.1 x2 in 05/10; 3 ref #extradata lines | grep 2/2 in 0005; 3 `+`-ref lines in 0009 | MATCH |
| C-PREFLIGHT L83-84 | mbox zero non-ASCII | perl 0 | MATCH |
| C-PREFLIGHT L119-121 | B 2/2 M17 comment stale ("which Series C 08/11 deletes") | present at B mbox L266 (R1 said L267; text unchanged, line shifted by the strict fix) | MATCH (disclosure true) |
| D-PREFLIGHT L3-11 | SCOPE NOTE: r3 record; MUST-final now 163076 B; C(10), 18 arithmetic-only; 6 TABs | verified | FIXED head (was 162643); body SCOPED |
| D-PREFLIGHT L37-44 | 6 commits; subjects 1/1 x6 (5/6 wrap disclosed); signoffs 6/6; ASCII 0 | verified | MATCH |
| E-PREFLIGHT L3-10 | SCOPE NOTE: r3 record; MUST-final now 12774 B; C(10), 12 arithmetic-only | verified | FIXED head (was 12693); body SCOPED |
| E-PREFLIGHT L30-39 | subjects 1:1 x2; signoffs 2/2; ASCII 0; 6 TAB recipe lines | verified (6 TABs) | MATCH |
| DEC-POST L3-9 | SCOPE NOTE: re-cut-2 staging; r6 now 507373 B; stack now 23 | verified (blob SAME) | MATCH (scope note current) |
| DEC-POST L58,L60 | [PATCH 0/2] + 2 patches; mail-1 11 attachments | 2 patches; mail-1 alone is 11 (combined 15) | MATCH |
| mail-combined/CHECKLIST | no mbox claims (attachment/sample sizes only) | n/a | N/A |
| mail-torture-trio/CHECKLIST | no mbox claims (attachment sizes only) | n/a | N/A |

## SEND-SITTING

Pin `2def950` + "(or later parent merge)" (L26): `fc5573a` is a
descendant (`merge-base --is-ancestor` true) but B/C/D/E blobs
DIFF (A0/A + all covers SAME x8) — §1 sizes and pin prose fixed;
dry-run section scope-noted (counts re-verified, sizes superseded).

| line | claim | actual | status |
|---|---|---|---|
| L29-36 | A 507373, A0 43263, B 40729, C 650794, E 12774, D 163076; MUST-final at fc5573a | wc identical x6 | FIXED (was R1 sizes + "at 2def950") |
| L45 | grep counts 2 1 2 10 2 6 = 23 | ^From counts identical | MATCH |
| L62-63 | 23/23 (24/24 pre-drop, C08 dropped) | 23 current | MATCH |
| L87-196 | mailsplit 0001 / 0001 0002 / 0001..0010 / 0001..0006 | mailsplit identical | MATCH |
| L94-199 | thread subjects per series | identical to mbox subjects | MATCH |
| L205-206 | 29 wire (23+6: 2+3+3+11+3+7) | arithmetic holds | MATCH |
| L231-249 | dry-run record vs 2def950 bytes + R2 scope note | counts/sizes-as-run match; current sizes in §1 | SCOPED head note; body kept |

## SUBMIT-PLAN / V2 (scratch planning records; untouched)

| file | claim | actual | status |
|---|---|---|---|
| ff-submit-v2/SUBMIT-PLAN-V2.md (192 l, dated 2026-10-16) | 5 series, 15 patches + D4 optional | plan-era state, predates 6/23 stack | SUPERSEDED-PLAN |
| ff-submit-plan/SUBMIT-PLAN.md L23 | 4 series, 12 patches | plan-era state | SUPERSEDED-PLAN |

Both absent from the tree (`find -iname '*submit-plan*'` empty);
dated out-of-tree planning inputs, not send docs. Left frozen;
this sweep is the current record. `DRIFT-AUDIT.md` (new since R1)
checked: no mbox size/count claims.

## Out of scope (noted, untouched)

Historical/verify docs outside the sweep set keep their era bytes
by design. Newly stale at R2 (all carry explicit base pins, kept
verbatim): COMBO-RERUN-8 stack line (L12-15, base `2def950`),
UNION-D deliverable line (L6, union bytes), MUST-VERIFY static
lines (L10-11, L67, L89, base `2def950`), E-VERIFY source line
(L19, repo @ `2def950`), RUNBOOK-R3-NOTES §0 bytes (L15, base
`224ec0b`), INTEGRATE-RUNBOOK §0 pin table + Pack lines (L40-43,
L86, L169, L197, L215, L234, L264 — R1-era bytes). Still-era as
at R1: COMBO-RERUN-6/7 (24-patch pre-drop), ED-STILL-CLEAN
(C 11 / E 11605 / D 128977), A-R4/R5-VERIFY (A 620505/620888),
MUST-TRIAGE (`/11` refs), A-SENDSTEPS-NITFIX (606385 B),
B-NITFIX-N1 (decfate), STACK-DAG (mixed-era sizes),
MUST-VERIFY, E-VERIFY, C-R2-VERIFY, D3-PROBE, PREPOST-REVIEW.

## Observations (not size/count; no fix)

- B mbox L266 carries stale text "which Series C 08/11 deletes"
  (bytes; disclosed in C-PREFLIGHT L119-121; R1 cited L267 — the
  strict fix shifted it one line). Bytes immutable here (docs
  match bytes, never reverse); a future B re-cut owns it.
- D cover (blob-identical, frozen send artifact) still describes
  1/6 landing "the new header-only libavcodec/ac4_toc.h"
  (COVER-D.txt L22, L58-59); MUST-final 1/6 uses A0's shared
  header instead of creating it. Content prose, not size/count;
  send artifact — untouched, noted for the D owner.
- A0/B/D/E §3 drip-order prose ("after A merges" etc.) vs the
  SEND-SITTING one-sitting order. Order, not size/count; untouched.
- E-VERIFY's "r5 decoder comment" seam notes describe `2def950`
  bytes (explicit base); the MUST-final r6 refresh (debt 1)
  supersedes the seam. Era record — untouched.

Verdict: every in-scope live claim MATCHes merged bytes at end;
frozen records carry explicit scope notes. 12 files fixed +
this sweep doc.
