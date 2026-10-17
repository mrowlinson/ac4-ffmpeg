# INTEGRATE-RUNBOOK — AC-4 FFmpeg endgame sequence

Doc only. No code. All repos read-only until the step that says commit.
Date: 2026-10-16. Scratch/TMPDIR: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-integrate-runbook`.
Refresh6: 2026-10-17 (post-MUST-final: union mboxes, MUST-verify
verdicts, combo-8 HOLD, send-sitting-2 — pin + verdict sweep; stale
pre-MUST steps DELETED, not marked).
Refresh6 scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r3`
(run R3-final, clone `wt-final` @ master 224ec0b).
Branch: `ff/doc-runbook-r3` (this branch).
Prior refreshes 1-5: HISTORICAL as of this refresh — their pins
(re-cut 3/4/5 chains, staged-B, 24-patch counts, combo-5/6/7,
mail-1 prep, suite-watch pending, N5-pending, D-P3 wiring-open) are
superseded by §0 + the MUST records below. Only the B2-landing
mechanism section keeps its historical cross-checks (append-only).

Inputs read: `doc/post/STACK-DAG.md` (191 l),
`doc/post/MUST-TRIAGE.md` (327 l), `doc/post/MUST-VERIFY.md` (149 l),
`doc/post/COMBO-RERUN-8.md` (116 l),
`doc/post/SEND-SITTING.md` (256 l),
`doc/post/E-VERIFY.md` (127 l), `doc/post/UNION-D.md` (62 l),
`doc/post/A-PREFLIGHT-CHECKLIST.md` (187 l, r6),
`doc/post/C-PREFLIGHT-CHECKLIST.md` (143 l, union),
`doc/post/B-PREFLIGHT-CHECKLIST.md` (107 l),
`doc/covers/COVER-*.txt` (DAG chain set),
`doc/post/*-OWNER-SEND-STEPS.md` (six mechanics),
all six `doc/post/series-*.mbox` (union bytes).
Master: `224ec0b ff/merged: ff/doc-must-verify`.

## 0. Pin table — MUST-final union send packs (master 224ec0b)

Live post set = the six mbox send packs. Every size/count below was
re-verified first-hand on master bytes this refresh (`ls -l` +
`grep -c "^From "`; MUST-VERIFY.md:10-11 records the same table).

| send pack (= pin) | bytes | mails | union commit |
|---|---|---|---|
| A0 head `doc/post/series-A0-head.mbox` | 43263 | 1 | `36b9eec` union-B-A0 (M21 walker) |
| A decoder `doc/post/series-A-decoder.mbox` | 507373 | 2 | `554b92b` must-fix-A (r6, M4-M9) |
| B fate `doc/post/series-B-fate.mbox` | 40708 | 2 | `36b9eec` union-B-A0 (stack tests) |
| C encoder `doc/post/series-C-encoder.mbox` | 650974 | 10 | `91c214b` union-C (M10-M18, C08 dropped) |
| E TS `doc/post/series-E-ts.mbox` | 12693 | 2 | `3b65f39` must-fix-stack (M22/M19) |
| D mp4 `doc/post/series-D-mp4.mbox` | 162643 | 6 | `50e7de5` union-D (M19-M21/T3) |

Stack order (STACK-DAG.md:10-21): A0 → A → B → C → E → D.
Patch rollup: 1 + 2 + 2 + 10 + 2 + 6 = **23 patches**,
**29 wire messages** with the six covers (SEND-SITTING.md:201-207:
2+3+3+11+3+7). Applies with plain `git am` in DAG order only
(SEND-SITTING.md:59-67). The stale STAGED `series-B-decfate.mbox`
is removed from the tree (SEND-SITTING.md:29-36).

MUST-verify verdicts (MUST-VERIFY.md:13-20; base master `2def950`,
bytes identical to 224ec0b for all six mboxes):

| branch | commit | verdict | debt ref |
|---|---|---|---|
| A-r6 | `554b92b` | **POST-OK** | none |
| union-B-A0 | `36b9eec` | **POST-OK** | none (encneg strict is assembly debt, MUST-VERIFY.md:82-85) |
| union-C | `91c214b` | **HOLD** | 6-line context re-cut, MUST-VERIFY.md:75-78 + :123-125 |
| union-D | `50e7de5` | **HOLD** | toc-drop + T3 strict + 7 re-pins, MUST-VERIFY.md:95-119 + :123-131 |
| E stack re-cut | `3b65f39` | **HOLD** | 2-line M11 re-cut, E-VERIFY.md:1-10 + :105-118 |

Combo-8 assembly verdict: **HOLD** — 23/23 `git am` green
(17 plain + 6 hand, COMBO-RERUN-8.md:22-36), build green
(COMBO-RERUN-8.md:46-50), N5 14/14 (COMBO-RERUN-8.md:51-54),
live suite 12/12 (COMBO-RERUN-8.md:55-57), sole red class G1
(7 post-D goldens, bytes proven, COMBO-RERUN-8.md:79-92);
before posting cut the 1 content + 4 mechanical re-cuts
(COMBO-RERUN-8.md:94-111).

## 1. Endgame sequence

### Step 0 — Gates: clock + link + global preflight (owner-run)

Clock + link (SEND-SITTING.md:16-21 — BOTH must pass):

```sh
date -u +%Y-%m-%dT%H:%M:%SZ   # must print >= 2026-10-17T22:23:09Z (B2 +24h); else STOP
ifconfig | grep 169.254.103.231  # TB-only send link; must print a line; else STOP
```

Global preflight on merged master (SEND-SITTING.md:23-57):

```sh
git log --oneline -1                     # expect 224ec0b ff/merged: ff/doc-must-verify (or later parent merge)
ls -l doc/post/*.mbox                    # sizes MUST read §0: 43263 / 507373 / 40708 / 650974 / 12693 / 162643
perl -ne '$c += tr/\x80-\xFF//; END { print "non-ascii=$c (want 0)\n" }' \
  doc/post/series-*.mbox doc/covers/COVER-*.txt
grep -c "^From " doc/post/series-A0-head.mbox doc/post/series-A-decoder.mbox \
  doc/post/series-B-fate.mbox doc/post/series-C-encoder.mbox \
  doc/post/series-E-ts.mbox doc/post/series-D-mp4.mbox
# want 1 2 2 10 2 6 in that file order (23 patch mails)
grep -n "incl. fate-source" doc/covers/COVER-B.txt  # must print one line (COVER-B.txt:33)
```

Suite fact behind the clock: live 12/12 + sha-ok, live since
2026-10-16T22:23:09Z (B-PREFLIGHT-CHECKLIST.md:16-18); the +24h
end needs owner-at-post confirm (B-PREFLIGHT-CHECKLIST.md:19).
MUST-triage fact-checked this clock as M3 FALSE-POSITIVE with the
SEND-SITTING gate as its residue (MUST-TRIAGE.md:89-101).

### Step 1 — Lib prep DONE (blocks nothing now)

dec-lib + wrap + Q1 + ngis + fuzzfix + cleanfiles + hygiene +
N5fix + re-vendor, enc-libr2 + R3 + minors + overflow + S5FU,
dsi-ser + lavcshape: all merged pre-MUST; N5 LANDED in-core
(gpact pin 48000/1602/5727312B, A-PREFLIGHT-CHECKLIST.md:108;
MUST-VERIFY.md:43-45 re-proves live). DSI drift DOC-ONLY stands.
The old per-commit chains are deleted from this doc — live proof
is the r6 preflight + MUST-VERIFY gates, not the merge history.
D-P3 wiring-open (old step 4d) is CLOSED by M21: one shared TOC
walker `libavcodec/ac4_toc.h`, A0 creates / D uses
(STACK-DAG.md:112-127; UNION-D.md:20-31).

### Step 2 — Series A0 head (POST-OK, sends FIRST)

Pack: `series-A0-head.mbox` (43263 B, 1 patch) + `COVER-A0.txt`
(`[PATCH] lavf/ac4: fill codecpar + timestamps from first frame`,
COVER-A0.txt:1). Union commit `36b9eec`. DAG head over pristine
`3e6cd5e` (STACK-DAG.md:10-11); cover states the full chain +
`Stack parent: none` (COVER-A0.txt:3-7).
MUST-final content: M21 one-walker — creates
`libavcodec/ac4_toc.h` (678 lines, mbox:57-74) and the demuxer
walks it (no private fps table, mbox:411 + :731), with
`need_parsing=HEADERS` (mbox:23); pre-D PARSE_NONE degrade proven
(MUST-VERIFY.md:57-60).
Verify: POST-OK (MUST-VERIFY.md:18) — stack A0+A+B 5/5 `am`
clean, B-tip `fate-ac4` 16/16 + deterministic rerun, A0 3/3
(MUST-VERIFY.md:52-56). A0-vs-D header delta is comment-only
(MUST-VERIFY.md:61-63); the D-side create collision rides the D
HOLD, not this series.
Send: FIRST, after the step-0 clock (M3 fate refs need suite
clips). Mechanics `doc/post/A0-OWNER-SEND-STEPS.md`; one-sitting
form SEND-SITTING.md:78-95 (mailsplit → 0001, cover as 0000).
Expect zero warnings.

### Step 3 — Series A decoder (POST-OK, sends SECOND)

Pack: `series-A-decoder.mbox` (507373 B, 2 patches, re-cut 6) +
`COVER-A.txt` (`[PATCH 0/2] native AC-4 decoder for raw .ac4
streams`, COVER-A.txt:1). Union commit `554b92b`. Staging
`b2df32f` (1/2, 24 files) + `1c1b2b0` (2/2, ac4dec.c +
allcodecs.c) on pristine `3e6cd5e`
(A-PREFLIGHT-CHECKLIST.md:47-59).
MUST-final content: r6 fixes M4 (97 getenv + 179 fprintf + fopen
dumps stripped), M5 (provenance scrub + `ac4_stages.h` deleted),
M6 (`pres_sel`-only, 5 dead options deleted), M7 (flush unref),
M8 (CRC disclosure), M9 (portability)
(A-PREFLIGHT-CHECKLIST.md:8-41) + the re-cut-5 guard fix
(`fate-source` CI blocker closed).
Verify: POST-OK (MUST-VERIFY.md:17) — `git am` rc=0 ZERO
warnings, trees exact-match staging (MUST-VERIFY.md:24-26);
`fate-source` byte-identical to ref (MUST-VERIFY.md:35-36;
A-PREFLIGHT-CHECKLIST.md:72); build rc=0 with 0 ours warnings
(MUST-VERIFY.md:37-39); smoke 11/11 `cmp`-identical to shim +
sprobe + r5 bytes (MUST-VERIFY.md:40-42;
A-PREFLIGHT-CHECKLIST.md:101); N5 live exact vs pins
(MUST-VERIFY.md:43-45); M6 `-pres_sel`-only /
F2-gate PASS (MUST-VERIFY.md:47-48;
A-PREFLIGHT-CHECKLIST.md:111-118); round-trip `format-patch`
byte-identical 507373 B (MUST-VERIFY.md:46).
Send: SECOND, stacks after A0. Mechanics
`doc/post/A-OWNER-SEND-STEPS.md`; one-sitting form
SEND-SITTING.md:97-113 (0/2 cover → 1/2 core → 2/2 wire).
Zero sample deps. Expect zero 8bit warnings.

### Step 4 — Series B doc + decoder-FATE (POST-OK, sends THIRD at clock end)

Pack: `series-B-fate.mbox` (40708 B, 2 patches) + `COVER-B.txt`
(`[PATCH 0/2] doc + FATE for the native AC-4 decoder`,
COVER-B.txt:1). Union commit `36b9eec`. Stale STAGED pack
deleted from tree — B pack is `series-B-fate.mbox` only
(SEND-SITTING.md:29-36).
MUST-final content: M17 layout negatives folded from dropped C08
into B 2/2 as FATE behind `ifeq ($(CONFIG_AC4_ENCODER),yes)`
(mbox:272-291; STACK-DAG.md:74-78); T2 frac pair
(mbox:108-124 + :245; STACK-DAG.md:85-89); T5 seek-eof `-copyts`
+4096 golden (mbox:94 + :252-253; STACK-DAG.md:102-107); M8
skip-bad rename (mbox:111-124 + :213-223; STACK-DAG.md:71-73);
M6 pres-only docs (STACK-DAG.md:69-70); T4 pres-1 disclosure
(STACK-DAG.md:98-101). B 2/2 CREATES `tests/fate/ac4.mak`
(mbox:114-139, seed aggregate mbox:317) + the Makefile include
(STACK-DAG.md:48-49).
Verify: POST-OK as union-B-A0 (MUST-VERIFY.md:18, :50-63) —
same gates as step 2. B 2/2 CMDs lacking `-strict -2` is proven
only at C-tip (encneg activates post-C) and is recorded assembly
debt, not a B HOLD (MUST-VERIFY.md:82-85); the re-cut carries it
(COMBO-RERUN-8.md:105-106).
Send: THIRD, at clock end, after A/A0 land (B FATE legs need the
A decoder + A0 carriage; STACK-DAG.md:28-29). Mechanics
`doc/post/B-OWNER-SEND-STEPS.md`; one-sitting form
SEND-SITTING.md:115-132 (0/2 cover → 1/2 doc → 2/2 fate).
Expect zero 8bit warnings.

### Step 5 — Series C encoder (HOLD: 6-line context re-cut, sends FOURTH)

Pack: `series-C-encoder.mbox` (650974 B, 10 mails) + `COVER-C.txt`
(`[PATCH 0/10] native AC-4 encoder`, COVER-C.txt:1). Union commit
`91c214b` (r6 M10-M18 content re-chained onto stack-B, mak in
appendix shape; C-PREFLIGHT-CHECKLIST.md:1-13). Old C08 neg gate
DROPPED — its two layout negatives ride B 2/2 as FATE
(C-PREFLIGHT-CHECKLIST.md:12-13; SEND-SITTING.md:29-36;
COMBO-RERUN-8.md:36). Subjects 01/10 tables → 02/10 core →
03/10 wire → 04/10 docs → 05/10 fate → 06/10 nosamples →
07/10 doc-note → 08/10 fold8 → 09/10 DSI-wire → 10/10 DSI-mask
(mbox:4, :2094, :16139, :17002, :17100, :17246, :17281, :17318,
:17383, :18343; SEND-SITTING.md:149-160).
MUST-final content: M10 `AC4_`-prefixed tables, M11 EXPERIMENTAL
(mbox:16891) + 13 `-strict -2` CMDs, M12 cap, M13 sincos guard,
M14 lavc allocators, M15 TLS/mutex fix, M16 bump-drop, M17
script-delete (C08 drops, series → 10), M18 single-row docs
chained on B (C-PREFLIGHT-CHECKLIST.md:16-21). Goldens full-hash
in 05/10, unchanged from r5 (MUST-VERIFY.md:67-70).
Verify: HOLD — content green, 6-line re-cut owed
(MUST-VERIFY.md:19, :65-85). Static green (650974 B/10 mails,
1:1 subjects, 10/10 signoffs, ASCII 0/0, 0 placeholders;
MUST-VERIFY.md:67-70); M10-M18 residue zero
(MUST-VERIFY.md:71-74); encoder fate 5/5 on pinned md5s,
`nm` dupsym-intersection EMPTY (MUST-VERIFY.md:79-81). The seam:
C 01/10 fails at `Makefile:219` on the A-r6 tree — 3 hunks quote
r5's 2-line decoder comment that A-r6 reworded (predicted in
A-r6 preflight, C-seat-owned); the 6-context-line refresh (no
content change) makes A0+A+B+C 15/15 rc=0
(MUST-VERIFY.md:75-78; MUST-VERIFY.md:123-125 debt row 1;
COMBO-RERUN-8.md:31 + :109-111 cut 5).
Send: FOURTH, after B lands (extends B's docs row + appends the
B-created mak; STACK-DAG.md:30-31 + :47-49). Mechanics
`doc/post/C-OWNER-SEND-STEPS.md`; one-sitting form
SEND-SITTING.md:134-160. DO NOT SEND until the 6-line re-cut
lands and combo-9 (or the parent-ruled equivalent) re-runs.

### Step 6 — Series E TS-mux (HOLD: 2-line M11 re-cut, sends FIFTH)

Pack: `series-E-ts.mbox` (12693 B, 2 patches) + `COVER-E.txt`
(`[PATCH 0/2] MPEG-TS mux descriptors for AC-4`, COVER-E.txt:1).
Stack commit `3b65f39` (M22 message rewrite + M19 Changelog line
after C's, STACK-DAG.md:58-68; mbox Changelog hunk mbox:33;
E-tip stacking L5 C / L6 E / L24 B proven in
E-VERIFY.md:59-61).
MUST-final content: M22 true-cause message (mpegtsenc PCR helper
+ `frame_bytes=0` fallback, encoder DOES set 2048 — every clause
verified code + empirically, E-VERIFY.md:63-81; message at
mbox:95-112); probe `-strict -2` (mbox:286, load-bearing both
sides of D, E-VERIFY.md:96-98); asynth-only, no sample wait.
Verify: HOLD — E-as-merged neither applies to nor passes on
union-C (M11 EXPERIMENTAL); content gates all green
(E-VERIFY.md:1-10). Subjects 1:1, signoffs 2/2, ASCII 0/0, 0
placeholders (E-VERIFY.md:47-58); build rc=0 with 0 ac4
warnings, `DEA.L. ac4` (E-VERIFY.md:85-91); E fate 2/2 PASS +
deterministic re-run (E-VERIFY.md:92-95). Re-cut spec (E seat):
tail-CMD context 1 line + GEN `-strict -2` 1 line + message
clause, subjects unchanged (E-VERIFY.md:105-114;
MUST-VERIFY.md:123-126 debt row 2; COMBO-RERUN-8.md:102-104
cut 2). NOT E's: C01-C03 context (C seat) + B encneg strict
(B seat) (E-VERIFY.md:115-118).
Send: FIFTH, after C lands (2/2 appends the B-created,
C-extended mak; STACK-DAG.md:32). Mechanics
`doc/post/E-OWNER-SEND-STEPS.md`; one-sitting form
SEND-SITTING.md:162-179. DO NOT SEND until the 2-line re-cut
lands and combo-9 re-runs.

### Step 7 — Series D MP4 + parser (HOLD: toc-drop + T3 strict + 7 re-pins, LAST)

Pack: `series-D-mp4.mbox` (162643 B, 6 patches) + `COVER-D.txt`
(`[PATCH 0/6] MP4 ac-4+dac4 support + AC-4 parser`, COVER-D.txt:1).
Union commit `50e7de5` (D-seat content + stack mechanics + T3,
UNION-D.md:3-8). Subjects 1/6 parser → 2/6 dac4 demux → 3/6
dac4 mux → 4/6 parser fate → 5/6 mp4 roundtrip → 6/6 ts demux
(mbox:4, :1215, :1314, :1562, :2644, :3612; SEND-SITTING.md:197-199).
MUST-final content: M19 Changelog lines re-anchored to the stack
(parser after E's TS line in 1/6, mov line after the parser line
in 3/6; UNION-D.md:12-16; mbox:58 + :1349-1350); M20
`mov_ac4_is_multiframe` + EINVAL + negative, landed verbatim
(UNION-D.md:17-19; mbox:1407 + :1535); M21 parser half — D 1/6
creates header-only `ac4_toc.h` (mbox:44-58 + :512-516) and the
parser includes it (mbox:141, no second walk; UNION-D.md:20-28);
T3 claimed by D — `FATE_AC4_RT` roundtrip section riding 5/6,
150-line ref, first-frame dts/pts 4096
(UNION-D.md:36-41; mbox:2673-2676 + :3414-3433).
Verify: HOLD — D fate + T3 green on the fixed assembly; own
re-cut owed (MUST-VERIFY.md:20, :87-119). Static green (162643
B/6, 6/6 signoffs, ASCII 0/0, 0 placeholders;
MUST-VERIFY.md:89-94). Seams, all first-hand: E-as-master tail
context + GEN strict (E debt); D 1/6 `ac4_toc.h: already exists`
vs A0's walker — fixed D drops the 684-line create block (D
debt); T3 GEN strict 1 line (D debt); B encneg strict 2 lines
(B debt) (MUST-VERIFY.md:95-106). Fixed-assembly D-tip (23
commits): build rc=0, `fate-ac4` 25/29 with D 4/4 + aliases
green (MUST-VERIFY.md:107-110); the 4 reds + A0 3/3 red are
EXACTLY the predicted post-D parser flips (MUST-VERIFY.md:111-115),
unpinned because union-D shipped zero-churn pre-walker
(MUST-VERIFY.md:116-119). Combo-8 confirms: G1 = 7 goldens need
the D re-pin re-cut, observed bytes `cmp`-identical to the
predicted d-repin bytes 7/7, reswap proof 32/32 PASS
(COMBO-RERUN-8.md:79-92).
Send: LAST, after C+E land (appends the B/C/E-extended mak +
api/Makefile; STACK-DAG.md:33-34). Mechanics
`doc/post/D-OWNER-SEND-STEPS.md`; one-sitting form
SEND-SITTING.md:181-199. DO NOT SEND until the D re-cut (7
re-pins + toc-drop + T3 strict; COMBO-RERUN-8.md:99-102 cut 1
+ :107-108 cut 4; MUST-VERIFY.md:123-131 debt rows 3/4/6)
lands and combo-9 re-runs.

### Step 8 — Assembly validation (combo-8 record; combo-9 owed)

Record: `doc/post/COMBO-RERUN-8.md` (branch `ff/doc-combo-8`,
commit `643c968`, merged `2a1132f`). Stack A0(1) → A(2) →
B(2) → C(10) → E(2) → D(6) = 23 patches over pristine `3e6cd5e`,
sizes §0-exact (COMBO-RERUN-8.md:10-18). Per-patch: 17/23
plain-clean, 0 via `-3`, 6 hand, every hand merge byte-faithful
(COMBO-RERUN-8.md:22-36). Integrity: zero conflict markers,
single `fate-ac4:` aggregate (11 vars), `show --check` clean,
23/23 signoffs, ASCII 0 (COMBO-RERUN-8.md:38-42). Per-test:
B 11/11 incl 714 (combo-7 F1 CLOSED), C 5/5 goldens unchanged,
E 2/2, D 3/3 + T3 after RT-GEN strict, G1 4+3 reds =
predicted-exact (COMBO-RERUN-8.md:61-77).
Recipe for the combo-9 closer (re-run AFTER the step 5/6/7 +
B-mechanical re-cuts land — same scratch shape as combo-8,
COMBO-RERUN-8.md:3-8): fresh pristine-`3e6cd5e` clone, sequential
`git am` in DAG order, out-of-tree build, `make -j18`, N5
validate, live-suite 12/12, full `fate-ac4` + rerun + no-samples
shape. No code changes implied — re-cuts only
(COMBO-RERUN-8.md:113-116). Combo-6/7 records are HISTORICAL
(pre-MUST bytes); the old step-6 worktree recipe is deleted
(it pinned the superseded 24-stack).

### Step 9 — Send mechanics (ONE sitting, after all HOLDs lift)

Live procedure: `doc/post/SEND-SITTING.md` (send-sitting-2,
commit `ea3d10d`, merged `c3b3c5f`) — six series back-to-back in
stack order, no drip-sends (SEND-SITTING.md:1-14). To
`ffmpeg-devel@ffmpeg.org`, patches only, NO attachments, From/
Signoff `Michael Rowlinson <mrowlinson@gmail.com>`
(SEND-SITTING.md:12-14). Per-series: mailsplit `-osendX` attached
form + cover-as-0000 + `send-email` (SEND-SITTING.md:78-199;
attached form required — `-o send/` fails on git 2.56.0,
SEND-SITTING.md:230-243). After each series: archive shows the
full thread in order, record Message-IDs + date; 29 wire total
(SEND-SITTING.md:201-207). Owner-at-post checklist
(SEND-SITTING.md:245-256). Dry-run record on master bytes: six
mailsplits rc=0 with exact counts, six cover copies shape-ok,
`send-email --dry-run` Result OK on all 29 with ZERO warnings,
ASCII 0, signoffs 2/1/2/10/2/6
(SEND-SITTING.md:230-243). NOTHING IS SENT until steps 5/6/7 +
B-mechanical re-cuts land and combo-9 (or parent-ruled
equivalent) closes combo-8 (COMBO-RERUN-8.md:113-116);
MUST-VERIFY.md:134-137 states the same owner-at-post rule.

### Step 10 — Per-series ship checklist (post in this order)

Order: A0(1) → A(2) → B(2) → C(10) → E(2) → D(6).

- A0: Step 2 done — union walker pack POST-OK; sends FIRST after
  the step-0 clock. No debt.
- A: Step 3 done — r6 pack POST-OK; sends SECOND. No debt.
- B: Step 4 done — union pack POST-OK; sends THIRD at clock end
  after A/A0 land. Mechanical fwd-fix rides the re-cut round:
  both encneg CMDs `-strict -2` (COMBO-RERUN-8.md:105-106;
  MUST-VERIFY.md:123-130 debt row 5).
- C: Step 5 HOLD — content green; 6-line Makefile-context re-cut
  owed (no content change), then combo-9. Sends FOURTH.
- E: Step 6 HOLD — content green; 2-line M11 re-cut owed, then
  combo-9. Sends FIFTH.
- D: Step 7 HOLD — fate + T3 green on fixed assembly; toc-drop +
  T3 strict + 7 re-pins owed, then combo-9. LAST.
- Each series independently reviewable + revertable (revert order
  is reverse post order; posted patches never rebased — v2/v3
  re-posts per the old step-11 rules, kept).

### Step 11 — Rollback notes (from SEND-SITTING.md:209-228, kept)

Mail cannot be unsent. Wrong bytes → reply `v2` superseding
thread for that series only (a v2 re-carries its stack context:
C re-carries B, E re-carries C, D re-carries E). Broken thread →
resend the missing message as a reply in-thread. Mid-sitting
halt → sent series stay sent; remainder goes in a second sitting
in the same A0,A,B,C,E,D order. Out-of-order upstream landing →
follow-up rebase onto the landed tree, re-verify `git am`,
repost as v2 (chain A0→A→B→C→E→D is the application order; B's
A/A0 need is the only purely functional link). B landed before
A/A0 → B FATE legs fail upstream until A+A0 land, no repost
needed once they do. Suite files append-only (bad golden =
supersede + mak patch, never overwrite). Golden recalls and
merge-never-post rules from the old step 11 stand unchanged.

## B2 landing addendum (F1 fix, ff/doc-b2f1 — mechanism kept)

Source: `ff-B2-review/REVIEW-B2.md` F1 (medium). The staged B2 note
(`fate-b2/README.md`: "copy to tests/fate/ac4.mak; add include")
is stale and DELETED — it would clobber the E-track TS-mux tests
already living in `tests/fate/ac4.mak`, and both files define a
single-colon `fate-ac4:` target (make keeps the last, the other
set silently drops from the aggregate). No var-name collision
(`FATE_AC4_MPEGTS_*` vs `FATE_AC4/MD5/REMUX` — clean).

Landing procedure (mechanical, byte-verbatim stanza move, zero
content change; validated by the ff/doc-b2f1 dry-run — merged mak
parses under the real tests/Makefile, single aggregate, 18/18
tests expand):

1. No include step: `include $(SRC_PATH)/tests/fate/ac4.mak` is
   already at tests/Makefile:157.
2. Anchor: the single `fate-ac4:` aggregate line of in-tree
   `tests/fate/ac4.mak` (E-track tail).
3. Insert fate-b2/ac4.mak lines 151-369 (16 test stanzas + F6
   gates + F6 comment, byte-verbatim, single-blank separators)
   immediately BEFORE the aggregate line. Do NOT land lines
   1-150 (draft header, staging-only), 371-420 (parser staging
   block), or 425 (the staged file's own `fate-ac4:` line).
4. Land the B2 wiring with the other wiring lines, just above the
   aggregate: `# --- B2 wiring (decode/demux only; ENC terms
   re-added at C4) ---` + staged line 423 (`FATE_SAMPLES_AVCONV +=
   $(FATE_AC4-yes) $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)`).
5. Extend the ONE aggregate with the B2 vars (E vars first):
   `fate-ac4: $(FATE_AC4_MPEGTS_PMT-yes)
   $(FATE_AC4_MPEGTS_PROBE-yes) $(FATE_AC4-yes)
   $(FATE_AC4_MD5-yes) $(FATE_AC4_REMUX-yes)`.
6. Verify: `grep -c '^fate-ac4:'` = 1; E stanza + all 16 B2 tests
   present; `make -p` shows zero `overriding recipe` warnings.

Create-case note: if a from-scratch post-order stack reaches B2
with NO `tests/fate/ac4.mak` in tree, the B2 block lands as the
seed instead — but that re-opens the B/C creator boundary (C5'
also creates); do not improvise, re-stack per the assembly owner.

HISTORICAL cross-checks (r3/r4/Final/recut — pre-MUST bytes, kept
for the record): every re-audit re-confirmed append+merge as the
mechanism; the Final flip made B the creator (C5' rebases
create→append at C post; E2/D appends re-anchor).

MUST-final cross-check (this refresh): combo-8 EXECUTED the
mechanism on union bytes — B 2/2 creates the mak
(series-B-fate.mbox:114-139, seed aggregate :317), C05 + fold8 +
E2 + D4/D5/D6/T3 append before the single aggregate with
monotonic extension (STACK-DAG.md:48-49; COMBO-RERUN-8.md:26-36),
no wholesale mak copy anywhere, final single `fate-ac4:`
aggregate with 11 vars (COMBO-RERUN-8.md:38-39). The pending
re-cuts change no mechanism: E2 strict + RT-GEN strict + B
encneg strict are same-stanza token adds, the D toc-drop removes
a create hunk, the 7 re-pins touch ref files only
(COMBO-RERUN-8.md:99-111). Append+merge stands.
