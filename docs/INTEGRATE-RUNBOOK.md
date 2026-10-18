# INTEGRATE-RUNBOOK — AC-4 FFmpeg endgame sequence

Doc only. No code. All repos read-only until the step that says commit.
Date: 2026-10-16. Scratch/TMPDIR: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-integrate-runbook`.
Refresh7: 2026-10-18 (post-reverify reality: C/D/E HOLDs lifted to
POST-OK, MUST-final fixed mboxes, zero-hand 23/23 assembly, 29/29
fate; stale HOLD-gated steps DELETED, not marked; DO-NOT-SEND stays
until combo-9/sweep-r2 land — pending, explicitly NOT lifted).
Refresh7 scratch: `/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-runbook-r4`
(run R4, own clone `wt` @ master 8c9ba74; venue MBP-TB unreachable
→ LOCAL-ONLY docs seat, zero Dolby invocation).
Branch: `ff/doc-runbook-r4` (this branch).
Prior refreshes 1-6: HISTORICAL as of this refresh — their pins
(re-cut 3/4/5 chains, staged-B, 24-patch counts, combo-5/6/7/8 as
live, mail-1 prep, suite-watch pending, N5-pending, D-P3
wiring-open, C/D/E HOLDs, pre-fix mbox sizes) are superseded by
§0 + the reverify records below. Only the B2-landing mechanism
section keeps its historical cross-checks (append-only).

Inputs read: `doc/post/STACK-DAG.md`,
`doc/post/MUST-TRIAGE.md`, `doc/post/MUST-VERIFY.md` (HISTORICAL
verdicts: pre-fix base `2def950`),
`doc/post/ASSEMBLY-DEBTS-R2.md` (fixed mboxes + proof),
`doc/post/C-R3-VERIFY.md`, `doc/post/D-R2-VERIFY.md`,
`doc/post/E-R2-VERIFY.md` (HOLD lifts),
`doc/post/DRIFT-AUDIT.md` (zero FF-drift),
`doc/post/COMBO-RERUN-8.md` (HISTORICAL: pre-fix bytes),
`doc/post/SEND-SITTING.md` (mechanics live, sizes stale),
`doc/post/COVER-SIZE-SWEEP.md` (HISTORICAL: 224ec0b bytes),
`doc/post/E-VERIFY.md`, `doc/post/UNION-D.md`,
`doc/post/A-PREFLIGHT-CHECKLIST.md` (r6),
`doc/post/C-PREFLIGHT-CHECKLIST.md` (union),
`doc/post/B-PREFLIGHT-CHECKLIST.md`,
`doc/covers/COVER-*.txt` (DAG chain set),
`doc/post/*-OWNER-SEND-STEPS.md` (six mechanics),
all six `doc/post/series-*.mbox` (fixed bytes).
Master: `8c9ba74 ff/merged: ff/doc-D-reverify`.

## 0. Pin table — MUST-final FIXED send packs (master 8c9ba74)

Live post set = the six mbox send packs. Every size/count below was
re-verified first-hand on master bytes this refresh (`ls -l` +
`grep -c "^From "`; ASSEMBLY-DEBTS-R2.md:25-33 records the
old→new byte table; zero mbox drift `20bb917..8c9ba74`).

| send pack (= pin) | bytes | mails | union commit |
|---|---|---|---|
| A0 head `doc/post/series-A0-head.mbox` | 43263 | 1 | `36b9eec` union-B-A0 (M21 walker) |
| A decoder `doc/post/series-A-decoder.mbox` | 507373 | 2 | `554b92b` must-fix-A (r6, M4-M9) |
| B fate `doc/post/series-B-fate.mbox` | 40729 | 2 | `36b9eec` union-B-A0 (stack tests) |
| C encoder `doc/post/series-C-encoder.mbox` | 650794 | 10 | `91c214b` union-C (M10-M18, C08 dropped) |
| E TS `doc/post/series-E-ts.mbox` | 12774 | 2 | `3b65f39` must-fix-stack (M22/M19) |
| D mp4 `doc/post/series-D-mp4.mbox` | 163076 | 6 | `50e7de5` union-D (M19-M21/T3) |

Stack order (STACK-DAG.md:10-21): A0 → A → B → C → E → D.
Patch rollup: 1 + 2 + 2 + 10 + 2 + 6 = **23 patches**,
**29 wire messages** with the six covers (SEND-SITTING.md:201-207:
2+3+3+11+3+7). Applies with plain `git am` in DAG order, zero hand
(ASSEMBLY-DEBTS-R2.md:41-44). The stale STAGED
`series-B-decfate.mbox` is removed from the tree
(SEND-SITTING.md:29-36).

Reverify verdicts (all POST-OK; MUST-VERIFY.md:15-20 HOLD rows are
HISTORICAL — pre-fix base `2def950` — lifted by the three reverify
docs on the fixed bytes):

| branch | commit | verdict | lift cite |
|---|---|---|---|
| A-r6 | `554b92b` | **POST-OK** | none owed (bytes unchanged; MUST-VERIFY.md:17) |
| union-B-A0 | `36b9eec` | **POST-OK** | none owed (MUST-VERIFY.md:18; B debt-5 fix in fixed mbox, ASSEMBLY-DEBTS-R2.md:18) |
| union-C | `91c214b` | **POST-OK** | C-R3-VERIFY.md:34-38 |
| union-D | `50e7de5` | **POST-OK** | D-R2-VERIFY.md:11-13 (N1 cover nit) |
| E stack re-cut | `3b65f39` | **POST-OK** | E-R2-VERIFY.md:3-7 |

Assembly verdict: **POST-OK** — 23/23 plain `git am`, zero hand,
23 commits (ASSEMBLY-DEBTS-R2.md:41-44; D-R2-VERIFY.md:19),
build rc=0 with 0 ac4 warnings
(ASSEMBLY-DEBTS-R2.md:46-48), `fate-ac4` **29/29** + A0D trio
**3/3** (ASSEMBLY-DEBTS-R2.md:49-51; D-R2-VERIFY.md:24),
T3 GEN deterministic (D-R2-VERIFY.md:25).
Combo-8 (17 plain + 6 hand + G1 reds) is HISTORICAL — pre-fix
bytes (COMBO-RERUN-8.md:22-36 + :79-92).

Drift verdict: **zero FF-side drift** — every dec-lib/src vs
plain and lavc-dec/core vs dec-lib delta is IDENTICAL or
INTENTIONAL (DRIFT-AUDIT.md:21-57); no FF-side fixes owed
(DRIFT-AUDIT.md:84-87). One plain-side carry: N5 rate semantics
should be ported to `dolby-ac4-cleanroom/src/ac4dec.c`, else
grate!=13 vectors rate differently there — owner call, port or
record plain-wontfix (FF is spec-correct per Table E.4)
(DRIFT-AUDIT.md:76-83).

## 1. Endgame sequence

### Step 0 — Gates: clock + link + global preflight (owner-run)

Clock + link (SEND-SITTING.md:16-21 — BOTH must pass):

```sh
date -u +%Y-%m-%dT%H:%M:%SZ   # must print >= 2026-10-17T22:23:09Z (B2 +24h); else STOP
ifconfig | grep 169.254.103.231  # TB-only send link; must print a line; else STOP
```

Global preflight on merged master (SEND-SITTING.md:23-57):

```sh
git log --oneline -1                     # expect 8c9ba74 ff/merged: ff/doc-D-reverify (or later parent merge)
ls -l doc/post/*.mbox                    # sizes MUST read §0: 43263 / 507373 / 40729 / 650794 / 12774 / 163076
perl -ne '$c += tr/\x80-\xFF//; END { print "non-ascii=$c (want 0)\n" }' \
  doc/post/series-*.mbox doc/covers/COVER-*.txt
grep -c "^From " doc/post/series-A0-head.mbox doc/post/series-A-decoder.mbox \
  doc/post/series-B-fate.mbox doc/post/series-C-encoder.mbox \
  doc/post/series-E-ts.mbox doc/post/series-D-mp4.mbox
# want 1 2 2 10 2 6 in that file order (23 patch mails)
grep -n "incl. fate-source" doc/covers/COVER-B.txt  # must print one line (COVER-B.txt:33)
```

Suite fact behind the clock: live 12/12 + sha-ok, live since
2026-10-16T22:23:09Z (B-PREFLIGHT-CHECKLIST.md:22-24); the +24h
end needs owner-at-post confirm (B-PREFLIGHT-CHECKLIST.md:25).
MUST-triage fact-checked this clock as M3 FALSE-POSITIVE with the
SEND-SITTING gate as its residue (MUST-TRIAGE.md:89-101).
Clock-time has now PASSED, but the D-reverify seat flags the
suite +24h window EXPIRED with live re-confirm owed before
A0/B2 post (D-R2-VERIFY.md:38-39) — re-confirm, do not assume.

### Step 1 — Lib prep DONE (blocks nothing now)

dec-lib + wrap + Q1 + ngis + fuzzfix + cleanfiles + hygiene +
N5fix + re-vendor, enc-libr2 + R3 + minors + overflow + S5FU,
dsi-ser + lavcshape: all merged pre-MUST; N5 LANDED in-core
(gpact pin 48000/1602/5727312B, A-PREFLIGHT-CHECKLIST.md:108-109;
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
(MUST-VERIFY.md:61-63); the fixed D drops its own create block
and keeps this header (ASSEMBLY-DEBTS-R2.md:16;
D-R2-VERIFY.md:20). Bytes unchanged by the re-cut round
(ASSEMBLY-DEBTS-R2.md:33).
Send: FIRST, after the step-0 clock (M3 fate refs need suite
clips). Mechanics `doc/post/A0-OWNER-SEND-STEPS.md`; one-sitting
form SEND-SITTING.md:78-95 (mailsplit → 0001, cover as 0000).
Expect zero warnings. DO NOT SEND until combo-9/sweep-r2 land
(step 9 — pending, not lifted).

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
byte-identical 507373 B (MUST-VERIFY.md:46). Bytes unchanged by
the re-cut round (ASSEMBLY-DEBTS-R2.md:33).
Send: SECOND, stacks after A0. Mechanics
`doc/post/A-OWNER-SEND-STEPS.md`; one-sitting form
SEND-SITTING.md:97-113 (0/2 cover → 1/2 core → 2/2 wire).
Zero sample deps. Expect zero 8bit warnings. DO NOT SEND until
combo-9/sweep-r2 land (step 9 — pending, not lifted).

### Step 4 — Series B doc + decoder-FATE (POST-OK, sends THIRD at clock end)

Pack: `series-B-fate.mbox` (40729 B FIXED, 2 patches) +
`COVER-B.txt` (`[PATCH 0/2] doc + FATE for the native AC-4
decoder`, COVER-B.txt:1). Union commit `36b9eec`. Stale STAGED
pack deleted from tree — B pack is `series-B-fate.mbox` only
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
(STACK-DAG.md:48-49). Fixed bytes ADD the debt-5 forward-fix:
both enc-neg CMDs carry `-strict -2`
(ASSEMBLY-DEBTS-R2.md:18; +5/-6 numstat,
ASSEMBLY-DEBTS-R2.md:29).
Verify: POST-OK as union-B-A0 (MUST-VERIFY.md:18, :50-63) —
same gates as step 2. The encneg-strict assembly debt is
LANDED in these bytes and proven green at every downstream tip:
C-tip 23/23 incl the enc-neg pair (C-R3-VERIFY.md:27), E-tip
25/25 (E-R2-VERIFY.md:35 + :92-99), D-tip 29/29
(D-R2-VERIFY.md:24). Carry: `B-OWNER-SEND-STEPS.md` Pack line
still quotes the pre-fix 40708 B (B-OWNER-SEND-STEPS.md:15-16)
— refresh to 40729 B at send; mbox bytes are source of truth.
Send: THIRD, at clock end, after A/A0 land (B FATE legs need the
A decoder + A0 carriage; STACK-DAG.md:28-29). Mechanics
`doc/post/B-OWNER-SEND-STEPS.md`; one-sitting form
SEND-SITTING.md:115-132 (0/2 cover → 1/2 doc → 2/2 fate).
Expect zero 8bit warnings. DO NOT SEND until combo-9/sweep-r2
land (step 9 — pending, not lifted).

### Step 5 — Series C encoder (POST-OK, sends FOURTH)

Pack: `series-C-encoder.mbox` (650794 B FIXED, 10 mails) +
`COVER-C.txt` (`[PATCH 0/10] native AC-4 encoder`, COVER-C.txt:1).
Union commit `91c214b` (r6 M10-M18 content re-chained onto
stack-B, mak in appendix shape; C-PREFLIGHT-CHECKLIST.md:1-13).
Old C08 neg gate DROPPED — its two layout negatives ride B 2/2
as FATE (C-PREFLIGHT-CHECKLIST.md:12-13; SEND-SITTING.md:29-36).
Subjects 01/10 tables → 02/10 core → 03/10 wire → 04/10 docs →
05/10 fate → 06/10 nosamples → 07/10 doc-note → 08/10 fold8 →
09/10 DSI-wire → 10/10 DSI-mask (SEND-SITTING.md:149-160).
MUST-final content: M10 `AC4_`-prefixed tables, M11 EXPERIMENTAL
+ 13 `-strict -2` CMDs, M12 cap, M13 sincos guard, M14 lavc
allocators, M15 TLS/mutex fix, M16 bump-drop, M17 script-delete
(C08 drops, series → 10), M18 single-row docs chained on B
(C-PREFLIGHT-CHECKLIST.md:16-21). Goldens full-hash in 05/10,
unchanged from r5 (MUST-VERIFY.md:67-70).
Verify: POST-OK — union-C HOLD lifted on the fixed bytes
(C-R3-VERIFY.md:34-38). A0+A+B+C plain `am` 15/15 rc=0, zero
hand (C-R3-VERIFY.md:23); fixed-vs-shipped proven context-only
(6 lines, zero `+`/`-` content — C-R3-VERIFY.md:24); build
rc=0, 0 ac4 warnings (C-R3-VERIFY.md:25); fate-Cenc 5/5 pinned
md5s (C-R3-VERIFY.md:26); full `fate-ac4` 23/23 with the
enc-neg pair GREEN (C-R3-VERIFY.md:27); statics 10/10 signoffs,
ASCII 0, placeholders 0, subjects 1:1 (C-R3-VERIFY.md:28).
Carry (non-blocking, owner-at-post): `C-OWNER-SEND-STEPS.md`
Pack line still quotes the shipped 650974 B
(C-OWNER-SEND-STEPS.md:12-13) — refresh to 650794 B at send
(C-R3-VERIFY.md:42-45); mbox bytes are source of truth.
Send: FOURTH, after B lands (extends B's docs row + appends the
B-created mak; STACK-DAG.md:30-31 + :47-49). Mechanics
`doc/post/C-OWNER-SEND-STEPS.md`; one-sitting form
SEND-SITTING.md:134-160. DO NOT SEND until combo-9/sweep-r2
land (step 9 — pending, not lifted).

### Step 6 — Series E TS-mux (POST-OK, sends FIFTH)

Pack: `series-E-ts.mbox` (12774 B FIXED, 2 patches) +
`COVER-E.txt` (`[PATCH 0/2] MPEG-TS mux descriptors for AC-4`,
COVER-E.txt:1). Stack commit `3b65f39` (M22 message rewrite +
M19 Changelog line after C's, STACK-DAG.md:58-68; E-tip
stacking L5 C / L6 E / L24 B re-proven on fixed bytes,
E-R2-VERIFY.md:87-90).
MUST-final content: M22 true-cause message (mpegtsenc PCR helper
+ `frame_bytes=0` fallback, encoder DOES set 2048 — every clause
re-verified code + empirically on fixed bytes,
E-R2-VERIFY.md:70-83; message clause carries the strict note,
E-R2-VERIFY.md:128-130); probe `-strict -2` load-bearing both
directions (E-R2-VERIFY.md:63-66); asynth-only, no sample wait.
Fixed bytes carry the debt-2 forward-fix: tail-CMD context +
GEN `-strict -2` + message clause, subjects unchanged
(ASSEMBLY-DEBTS-R2.md:15; E-R2-VERIFY.md:124-137).
Verify: POST-OK — E HOLD lifted on the fixed bytes
(E-R2-VERIFY.md:3-7). Full stack A0→E plain `am` 17/17 rc=0,
zero hand (E-R2-VERIFY.md:31 + :40-49); build rc=0, 0 ac4
warnings, `DEA.L. ac4` (E-R2-VERIFY.md:53-58); E fate 2/2 PASS
+ deterministic rerun 2/2 (E-R2-VERIFY.md:32 + :59-62); M22
paragraph TRUE (E-R2-VERIFY.md:33); Changelog stacking exact
(E-R2-VERIFY.md:34); full `fate-ac4` 25/25 GREEN, E-VERIFY reds
gone via the B debt-5 fix (E-R2-VERIFY.md:35 + :92-99);
statics 1:1, ASCII 0, placeholders 0 (E-R2-VERIFY.md:103-113).
`E-OWNER-SEND-STEPS.md` Pack line already refreshed in-commit
(12774 B, E-OWNER-SEND-STEPS.md:12 + :19-20) — no carry.
Send: FIFTH, after C lands (2/2 appends the B-created,
C-extended mak; STACK-DAG.md:32). Mechanics
`doc/post/E-OWNER-SEND-STEPS.md`; one-sitting form
SEND-SITTING.md:162-179. DO NOT SEND until combo-9/sweep-r2
land (step 9 — pending, not lifted).

### Step 7 — Series D MP4 + parser (POST-OK, sends LAST)

Pack: `series-D-mp4.mbox` (163076 B FIXED, 6 patches) +
`COVER-D.txt` (`[PATCH 0/6] MP4 ac-4+dac4 support + AC-4 parser`,
COVER-D.txt:1). Union commit `50e7de5` (D-seat content + stack
mechanics + T3, UNION-D.md:3-8). Subjects 1/6 parser → 2/6 dac4
demux → 3/6 dac4 mux → 4/6 parser fate → 5/6 mp4 roundtrip →
6/6 ts demux (SEND-SITTING.md:197-199).
MUST-final content: M19 Changelog lines re-anchored to the stack
(parser after E's TS line in 1/6, mov line after the parser line
in 3/6; UNION-D.md:12-16); M20 `mov_ac4_is_multiframe` + EINVAL
+ negative, landed verbatim (UNION-D.md:17-19); M21 parser half
— fixed D 1/6 drops its own header create and uses A0's shared
`ac4_toc.h` (parser include + calls, no second walk;
ASSEMBLY-DEBTS-R2.md:16; D-R2-VERIFY.md:20); T3 claimed by D —
`FATE_AC4_RT` roundtrip section riding 5/6 with strict GEN
(UNION-D.md:36-41; ASSEMBLY-DEBTS-R2.md:17); 7 post-D re-pins
carried in 1/6 (ASSEMBLY-DEBTS-R2.md:19).
Verify: POST-OK — union-D HOLD lifted on the fixed bytes
(D-R2-VERIFY.md:11-13). Full stack A0→D plain `am` 6/6 mboxes
rc=0, 23 commits, 0 warnings (D-R2-VERIFY.md:19); D1 toc-drop
`cmp`-EQ A0's header (D-R2-VERIFY.md:20); T3 GEN carries
`-strict -2`, 0 bare `-c:a ac4` across all six mboxes
(D-R2-VERIFY.md:21); 7/7 re-pins `cmp`-MATCH the d-repin oracle
(D-R2-VERIFY.md:22); build rc=0, 0 ac4 warnings
(D-R2-VERIFY.md:23); `fate-ac4` 29/29 + A0D trio 3/3, prior
reds GREEN (D-R2-VERIFY.md:24); T3 deterministic rerun
byte-identical (D-R2-VERIFY.md:25); statics 6/6 sequential,
6/6 signoffs, ASCII 0, mailsplit 6/6 (D-R2-VERIFY.md:26).
Carries (non-blocking, owner-at-post): N1 cover word fold —
`COVER-D.txt` 5/6 subject drops ` test` vs the mbox
(D-R2-VERIFY.md:30-34), 1 word at send; `D-OWNER-SEND-STEPS.md`
Pack line still quotes the pre-fix 162643 B
(D-OWNER-SEND-STEPS.md:9) — refresh to 163076 B at send; mbox
bytes are source of truth.
Send: LAST, after C+E land (appends the B/C/E-extended mak +
api/Makefile; STACK-DAG.md:33-34). Mechanics
`doc/post/D-OWNER-SEND-STEPS.md`; one-sitting form
SEND-SITTING.md:181-199. DO NOT SEND until combo-9/sweep-r2
land (step 9 — pending, not lifted).

### Step 8 — Assembly validation (debts-R2 record; combo-9 owed)

Record: `doc/post/ASSEMBLY-DEBTS-R2.md` (branch
`ff/doc-assembly-debts`, fixed mboxes landed `20bb917`, merged
`83e5783`; rebase docs-only, mbox bytes `cmp`-identical —
ASSEMBLY-DEBTS-R2.md:3-8). Stack A0(1) → A(2) → B(2) → C(10) →
E(2) → D(6) = 23 patches over pristine `3e6cd5e`, sizes
§0-exact. Plain `git am` in DAG order: 6/6 mboxes rc=0, 23
commits, zero hand
(ASSEMBLY-DEBTS-R2.md:41-44; D-R2-VERIFY.md:19 re-proves).
Hygiene: 23 subjects 1:1, 23/23 signoffs, 0 conflict markers,
0 non-ASCII, mailsplit 23/23 (ASSEMBLY-DEBTS-R2.md:35-37).
Build rc=0, 34 baseline warnings, 0 ac4
(ASSEMBLY-DEBTS-R2.md:46-48). `fate-ac4` 29/29 rc=0, zero
fail/error (ASSEMBLY-DEBTS-R2.md:49-50); A0D trio 3/3
(ASSEMBLY-DEBTS-R2.md:51). The 6 MUST-verify debts (C context,
E strict, D toc-drop, D T3 strict, B encneg strict, 7 re-pins)
are all LANDED in these bytes (ASSEMBLY-DEBTS-R2.md:12-19).
Combo-6/7/8 records are HISTORICAL (pre-fix bytes).
Recipe for the combo-9 closer (re-run on the fixed bytes —
same scratch shape as combo-8, COMBO-RERUN-8.md:3-8): fresh
pristine-`3e6cd5e` clone, sequential `git am` in DAG order,
out-of-tree build, `make -j18`, N5 validate, live-suite 12/12,
full `fate-ac4` + rerun + no-samples shape. PENDING — not run,
not lifted (step 9).

### Step 9 — Send mechanics (ONE sitting, after combo-9/sweep-r2 land)

Live procedure: `doc/post/SEND-SITTING.md` (send-sitting-2) —
six series back-to-back in stack order, no drip-sends
(SEND-SITTING.md:1-14). To `ffmpeg-devel@ffmpeg.org`, patches
only, NO attachments, From/ Signoff
`Michael Rowlinson <mrowlinson@gmail.com>`
(SEND-SITTING.md:12-14). Per-series: mailsplit `-osendX`
attached form + cover-as-0000 + `send-email`
(SEND-SITTING.md:78-199; attached form required — `-o send/`
fails on git 2.56.0, SEND-SITTING.md:230-243). After each
series: archive shows the full thread in order, record
Message-IDs + date; 29 wire total (SEND-SITTING.md:201-207).
Owner-at-post checklist (SEND-SITTING.md:245-256).
STALE inside SEND-SITTING: its §sizes are the pre-fix bytes
(SEND-SITTING.md:29-31: 40708/650974/12693/162643) — the §0
fixed sizes are the source of truth, and its dry-run record
(SEND-SITTING.md:230-243) predates the re-cut. This seat re-ran
all six mailsplits + dry-runs on the fixed bytes: 29 `Result:
OK`, ZERO warnings (RUNBOOK-R4-NOTES.md verification log).
NOTHING IS SENT until combo-9 (step-8 recipe) + sweep-r2 (fresh
size/count sweep on fixed bytes — COVER-SIZE-SWEEP.md records
the 224ec0b pre-fix bytes) land: both PENDING, gate stands.

### Step 10 — Per-series ship checklist (post in this order)

Order: A0(1) → A(2) → B(2) → C(10) → E(2) → D(6).

- A0: Step 2 done — union walker pack POST-OK; sends FIRST after
  the step-0 clock. No debt. DO-NOT-SEND until combo-9/sweep-r2.
- A: Step 3 done — r6 pack POST-OK; sends SECOND. No debt.
  DO-NOT-SEND until combo-9/sweep-r2.
- B: Step 4 done — union pack POST-OK, debt-5 strict fix LANDED
  in the 40729-B bytes; sends THIRD at clock end after A/A0
  land. Carry: send-steps Pack line refresh at send.
  DO-NOT-SEND until combo-9/sweep-r2.
- C: Step 5 done — content POST-OK on fixed 650794-B bytes
  (C-R3-VERIFY). Carry: send-steps Pack line refresh at send.
  Sends FOURTH. DO-NOT-SEND until combo-9/sweep-r2.
- E: Step 6 done — content POST-OK on fixed 12774-B bytes
  (E-R2-VERIFY); send-steps already refreshed. Sends FIFTH.
  DO-NOT-SEND until combo-9/sweep-r2.
- D: Step 7 done — content POST-OK on fixed 163076-B bytes
  (D-R2-VERIFY). Carries: N1 cover word + send-steps Pack line
  refresh at send. LAST. DO-NOT-SEND until combo-9/sweep-r2.
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

MUST-final cross-check (R3 refresh): combo-8 EXECUTED the
mechanism on union bytes — B 2/2 creates the mak
(series-B-fate.mbox:114-139, seed aggregate :317), C05 + fold8 +
E2 + D4/D5/D6/T3 append before the single aggregate with
monotonic extension (STACK-DAG.md:48-49; COMBO-RERUN-8.md:26-36),
no wholesale mak copy anywhere, final single `fate-ac4:`
aggregate with 11 vars (COMBO-RERUN-8.md:38-39). The MUST-final
re-cuts changed no mechanism: E2 strict + RT-GEN strict + B
encneg strict are same-stanza token adds, the D toc-drop removes
a create hunk, the 7 re-pins touch ref files only
(ASSEMBLY-DEBTS-R2.md:12-19). Append+merge stands.
